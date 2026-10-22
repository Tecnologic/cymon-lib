# cymon-lib

[![CI](https://github.com/Tecnologic/cymon-lib/actions/workflows/ci.yml/badge.svg)](https://github.com/Tecnologic/cymon-lib/actions/workflows/ci.yml)

Device side of the **cymon** trace protocol: a C++20 library that lets an
embedded Cyphal node offer named float variables and oscilloscope-style
captures of them. The host tool [cymon](https://github.com/Tecnologic/cymon)
lists the variables, configures and arms a capture, and reads it back over
Cyphal.

The library has no Cyphal dependency. The firmware deserializes the cymon
service requests (DSDL in [`dsdl/cymon/`](dsdl/cymon)), calls the library and
serializes the results. [UNIMOC](https://github.com/Tecnologic/unimoc) is the
first user; see [docs/integration.md](docs/integration.md).

## Features

- **Variable registry**: up to `CYMON_MAX_VARS` (64) named float variables
  with a unit. Each one is read through a `const float*`.
- **Capture**: up to `CYMON_MAX_CHANNELS` (8) channels into a
  `CYMON_MAX_BUFFER_BYTES` (4 KiB) ring, with a pre-trigger depth. The sample
  period is a whole multiple of the tick period (decimation).
- **Trigger**: rising, falling, rising with hysteresis, immediate, or
  requested by the firmware (for example on another node's
  `cymon.TriggerEvent`). The trigger is only evaluated once the pre-trigger
  part has been recorded, so a complete capture contains recorded frames only.
- **Interrupt-safe sampling**: `Device::Tick()` does not allocate, take locks,
  call function pointers or use libm. It can run in a zero-latency interrupt.
- **No heap, no exceptions, no RTTI needed**: all storage is inside the
  `Device` object. The code is clean under the UNIMOC warning set (`-Wconversion`,
  `-Wold-style-cast`, ... with `-Werror`) and its clang-tidy rules.

## Protocol (DSDL)

All types are in the root namespace `cymon`, version **0.1**. Major version 0
means the protocol may still change. None of them has a fixed port-ID: the
node assigns the IDs through the standard port registers, and a client finds
them through the `.type` registers.

| Type | Kind | Port register | Purpose |
|---|---|---|---|
| `cymon.GetVariableList.0.1` | service | `uavcan.srv.cymon_variables.id` | Variable `index` → name and unit; an empty name ends the list |
| `cymon.GetBufferInfo.0.1` | service | `uavcan.srv.cymon_buffer_info.id` | Buffer size, channel limit, frames, tick period |
| `cymon.SetupCapture.0.1` | service | `uavcan.srv.cymon_setup.id` | Channels, sample period, samples, pre-trigger depth |
| `cymon.ArmTrigger.0.1` | service | `uavcan.srv.cymon_arm.id` | Arm (source, mode, level, band) or disarm |
| `cymon.ReadSamples.0.1` | service | `uavcan.srv.cymon_read.id` | State; once complete, the frames page by page |
| `cymon.TriggerEvent.0.1` | message | `uavcan.pub.cymon_trigger.id` / `uavcan.sub.cymon_trigger.id` | Published when a capture triggers; can trigger other nodes |
| `cymon.Error.0.1`, `cymon.TriggerMode.0.1`, `cymon.CaptureState.0.1` | enums | — | Shared constants (match `cymon::ErrorCode`, `TriggerMode`, `CaptureState`) |

A client runs a capture like this:

1. It walks `GetVariableList` from index 0 until it gets an empty name.
2. It calls `GetBufferInfo` once.
3. `SetupCapture`: the response contains the sample period actually used.
4. `ArmTrigger` with `arm = true`.
5. It polls `ReadSamples` with `max_frames = 0` until `state` is `COMPLETE`.
6. It reads the frames with `ReadSamples`, increasing `frame_offset`, until
   `end_of_data`. Frame `pretrigger_samples` is the trigger sample.

A `ReadSamples` response carries at most 64 floats (8 frames with 8 channels)
and fits in 5 CAN FD frames. `scripts/check_dsdl.sh` validates the types and
compiles the C serialization that Nunavut generates from them.

## Usage

```cpp
#include "cymon/device.hpp"

namespace {
cymon::Device trace;
float i_q_measured;   // written by the control loop
float speed;
}  // namespace

void SetupTrace() {
  trace.RegisterVariable("i_q", "A", &i_q_measured);   // ID 0
  trace.RegisterVariable("speed", "rad/s", &speed);    // ID 1
  trace.SetTickPeriod(62.5F);                          // Tick() at 16 kHz
}

// Sampling context (control-cycle interrupt), after the variables are updated:
void OnControlCycle() { trace.Tick(); }

// Cyphal thread: one call per received request (see docs/integration.md).
//   trace.GetVariable(index)          -> GetVariableList response
//   trace.HandleGetBufferInfo()       -> GetBufferInfo response
//   trace.HandleSetupCapture(config)  -> SetupCapture response
//   trace.HandleArmTrigger(config)    -> ArmTrigger response
//   trace.HandleReadSamples(off, n)   -> ReadSamples response
//   trace.TakeTriggerEvent(&event)    -> publish cymon.TriggerEvent
//   trace.RequestTrigger()            <- received cymon.TriggerEvent of another node
```

### Execution contexts

- `Tick()` runs in the sampling context, normally an interrupt at a fixed rate.
- Every other method runs in one thread, normally the Cyphal thread.
  `Tick()` may preempt it at any point. The capture state is a lock-free
  atomic. The thread changes the capture setup only while `Tick()` ignores it
  (not armed), and publishes the new state last. `Tick()` and the thread must
  not run in parallel on two cores.
- `RegisterVariable()` and `SetTickPeriod()` are start-up calls, made before
  the first `Tick()`.
- The variable names, units and sources must outlive the `Device`. A `float`
  written by another context is read with one 32-bit load, which is atomic on
  Cortex-M and x86-64.

### Limits

Each limit is a CMake cache variable, which becomes a compile definition of
the `cymon` target. Without CMake, define the macro before you include any
cymon header.

| Variable | Default | Effect |
|---|---|---|
| `CYMON_MAX_BUFFER_BYTES` | 4096 | Capture buffer size; at most 65535 floats |
| `CYMON_MAX_VARS` | 64 | Registry size (1..255) |
| `CYMON_MAX_CHANNELS` | 8 | Channels of one capture (1..16) |

The `Device` object takes roughly `CYMON_MAX_BUFFER_BYTES + 24 * CYMON_MAX_VARS
+ 100` bytes of RAM.

## Build and test

These steps need CMake 3.20 or later and a C++20 compiler.

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug   # tests are on for the top-level build
cmake --build build
ctest --test-dir build --output-on-failure
clang-tidy -p build src/cymon/*.cpp                # the UNIMOC rule set (.clang-tidy)
scripts/check_dsdl.sh                              # needs: pip install nunavut
```

The style is the UNIMOC style, checked with `clang-format` 22.1.1
(`pip install clang-format==22.1.1`).

As a subproject (`add_subdirectory`), the library builds only the `cymon`
target (alias `cymon::cymon`), without tests or extra warning flags, so the
parent build's toolchain flags apply. The DSDL root namespace is exported as
`CYMON_DSDL_DIR`.

## Changes in 0.2

0.2 changes the API and the protocol. Nothing used 0.1 in production.

- C++20 (0.1 asked for C++23 in CMake and said C++17 in this README).
  The minimum CMake version is 3.20.
- Variables take a `const float*` instead of a `std::function<float()>`, so
  `Tick()` is interrupt-safe and the library has no heap use.
- Methods use PascalCase (`VariableCount()`, `SetTickPeriod()`, `State()`),
  following UNIMOC. `ForceTrigger()` is now `RequestTrigger()`, and it is safe
  to call from the thread.
- Handlers return `ErrorCode` instead of an `ok` flag plus a code.
  `ReadSamples` returns the capture state and is the status poll.
  `SetupCapture` rounds the sample period to whole ticks.
- DSDL 0.1 replaces 1.0. The 1.0 files had no `@extent`/`@sealed` and
  were rejected by the DSDL compilers. `TriggerMode.IMMEDIATE` and
  `CaptureState` are new. `TriggerEvent` no longer repeats the source
  node-ID, which the transfer already carries.
- `pretrigger_samples` must be smaller than `num_samples`, so the trigger
  sample is always part of the capture.

## License

Apache 2.0 — see [LICENSE](LICENSE).
