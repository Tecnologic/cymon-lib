# cymon-lib

[![CI](https://github.com/Tecnologic/cymon-lib/actions/workflows/ci.yml/badge.svg)](https://github.com/Tecnologic/cymon-lib/actions/workflows/ci.yml)

Platform-agnostic C++17 library for Cyphal-connected embedded nodes.
Exposes named scalar variables and oscilloscope-style capture via a Cyphal
CAN-FD service protocol. The library is pure C++ with no Cyphal/canard
dependency — the integrating application calls the library's handlers when
it receives a Cyphal service request.

## Features

- **Named variable registry** — register up to `CYMON_MAX_VARS` (default 64)
  scalar variables with a `std::function<float()>` getter and iterate them
  via a `GetVariableList`-style index query.
- **Oscilloscope-style capture** — configurable number of channels,
  sample count, pre-trigger depth, and trigger modes (rising, falling,
  hysteresis).
- **Paged readout** — `ReadSamples` returns interleaved float frames in pages
  so the caller can fit data into Cyphal transfer payloads.
- **Hardware-agnostic timer interface** — call `Device::Tick()` from your
  timer ISR/task; report the actual achieved period with
  `set_actual_sample_period_us()`.
- **Compile-time limits** — override `CYMON_MAX_BUFFER_BYTES` and
  `CYMON_MAX_VARS` via preprocessor defines before including any header.

## Protocol (DSDL)

All DSDL types live under `dsdl/cymon/` and are **unregulated** (no numeric
port-ID prefix).

| Service / Message       | Description                                      |
|-------------------------|--------------------------------------------------|
| `GetVariableList.1.0`   | Iterator-style variable enumeration (index→name) |
| `GetBufferInfo.1.0`     | Query buffer limits                              |
| `SetupCapture.1.0`      | Configure channels, sample count, pretrigger     |
| `ArmTrigger.1.0`        | Arm/disarm trigger with level & mode             |
| `ReadSamples.1.0`       | Paged readout of captured samples                |
| `TriggerEvent.1.0`      | Broadcast message emitted when trigger fires     |

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

To build with a specific compiler:

```bash
cmake -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

To disable tests:

```bash
cmake -B build -DCYMON_BUILD_TESTS=OFF
cmake --build build
```

## Integration

```cpp
#include "cymon/device.hpp"

cymon::Device device;

// Register variables (call once at startup).
device.RegisterVariable("speed", "rpm", [] { return read_speed(); });
device.RegisterVariable("current", "A",  [] { return read_current(); });

// In your Cyphal service handler:
auto resp = device.HandleSetupCapture(config);
auto resp = device.HandleArmTrigger(arm_cfg);
auto resp = device.HandleReadSamples(frame_offset, max_frames);

// In your hardware timer ISR / task:
device.Tick();
```

## Licence

Apache 2.0 — see [LICENSE](LICENSE).
