# Integrating cymon-lib into a Cyphal firmware

This guide connects `cymon::Device` to a Cyphal node that uses libcanard and
the C serialization Nunavut generates. That is the UNIMOC setup; the steps
for UNIMOC itself are listed in its `docs/cymon_integration_plan.md`.

## 1. Build

Add the library as a subproject and link it:

```cmake
set(CYMON_MAX_BUFFER_BYTES 8192 CACHE STRING "" FORCE)   # optional limits
add_subdirectory(path/to/cymon-lib ${CMAKE_CURRENT_BINARY_DIR}/cymon)
target_link_libraries(my_firmware PRIVATE cymon::cymon)
```

As a subproject, the library takes the parent build's flags and builds no
tests. It needs C++20 and no heap.

## 2. Generate the serialization

Add `dsdl/cymon` (exported as `CYMON_DSDL_DIR`) as one more root namespace
for Nunavut. It depends on no other namespace:

```sh
python -m nunavut -O <out> -l c --target-endianness little <cymon-lib>/dsdl/cymon
```

This produces `cymon/<Type>_0_1.h` with the usual
`cymon_<Type>_Request_0_1_deserialize_()` and `..._Response_0_1_serialize_()`
functions, plus the `_EXTENT_BYTES_` and `_SERIALIZATION_BUFFER_SIZE_BYTES_`
constants.

## 3. Port-IDs and registers

The cymon types have no fixed port-IDs. Give each one a register, following
the Cyphal convention, so tools can find it:

| Register (`natural16[1]`) | Read-only companion (`string`) |
|---|---|
| `uavcan.srv.cymon_variables.id` | `uavcan.srv.cymon_variables.type` = `cymon.GetVariableList.0.1` |
| `uavcan.srv.cymon_buffer_info.id` | `uavcan.srv.cymon_buffer_info.type` = `cymon.GetBufferInfo.0.1` |
| `uavcan.srv.cymon_setup.id` | `uavcan.srv.cymon_setup.type` = `cymon.SetupCapture.0.1` |
| `uavcan.srv.cymon_arm.id` | `uavcan.srv.cymon_arm.type` = `cymon.ArmTrigger.0.1` |
| `uavcan.srv.cymon_read.id` | `uavcan.srv.cymon_read.type` = `cymon.ReadSamples.0.1` |
| `uavcan.pub.cymon_trigger.id` | `uavcan.pub.cymon_trigger.type` = `cymon.TriggerEvent.0.1` |
| `uavcan.sub.cymon_trigger.id` | `uavcan.sub.cymon_trigger.type` = `cymon.TriggerEvent.0.1` |

An ID of 65535 disables the port. Service-IDs must be in the unregulated
range 0..255. The suggested defaults are 120..124 for the five services in
the order of the table, with the trigger subject disabled. The cymon host
tool reads `uavcan.node.port.List` and the `.type` registers, so any IDs
work as long as they are unique on the node.

Also add every server to the node's `uavcan.node.port.List` and subscribe the
service requests with the request extent (`..._Request_0_1_EXTENT_BYTES_`).

## 4. Service handlers (Cyphal thread)

One function per service: deserialize the request, call the device,
serialize the response. Error handling of the (de)serialization is left out
here.

```cpp
#include "cymon/ReadSamples_0_1.h"
#include "cymon/SetupCapture_0_1.h"
#include "cymon/device.hpp"

std::size_t OnSetupCapture(cymon::Device& device, std::span<const std::byte> request,
                           std::span<std::byte> response) {
  cymon_SetupCapture_Request_0_1 req{};
  std::size_t size = request.size();
  cymon_SetupCapture_Request_0_1_deserialize_(&req, reinterpret_cast<const uint8_t*>(request.data()), &size);

  cymon::Device::CaptureConfig config;
  // At most 16 IDs by the DSDL; more than kMaxChannels is answered with BAD_CONFIG.
  config.num_channels = static_cast<uint8_t>(req.channel_ids.count);
  std::copy_n(req.channel_ids.elements, std::min(req.channel_ids.count, cymon::kMaxChannels),
              config.channel_ids.begin());
  config.sample_period_us = req.sample_period_us;
  config.num_samples = req.num_samples;
  config.pretrigger_samples = req.pretrigger_samples;
  const auto result = device.HandleSetupCapture(config);

  cymon_SetupCapture_Response_0_1 resp{};
  resp.result.value = static_cast<uint8_t>(result.error);
  resp.actual_sample_period_us = result.actual_sample_period_us;
  size = response.size();
  cymon_SetupCapture_Response_0_1_serialize_(&resp, reinterpret_cast<uint8_t*>(response.data()), &size);
  return size;
}

std::size_t OnReadSamples(const cymon::Device& device, std::span<const std::byte> request,
                          std::span<std::byte> response) {
  cymon_ReadSamples_Request_0_1 req{};
  std::size_t size = request.size();
  cymon_ReadSamples_Request_0_1_deserialize_(&req, reinterpret_cast<const uint8_t*>(request.data()), &size);

  const auto result = device.HandleReadSamples(req.frame_offset, req.max_frames);
  cymon_ReadSamples_Response_0_1 resp{};
  resp.state.value = static_cast<uint8_t>(result.state);
  resp.num_channels = result.num_channels;
  resp.num_samples = result.num_samples;
  resp.pretrigger_samples = result.pretrigger_samples;
  resp.sample_period_us = result.sample_period_us;
  resp.end_of_data = result.end_of_data;
  resp.samples.count = result.sample_count;
  std::copy_n(result.samples.begin(), result.sample_count, resp.samples.elements);
  size = response.size();
  cymon_ReadSamples_Response_0_1_serialize_(&resp, reinterpret_cast<uint8_t*>(response.data()), &size);
  return size;
}
```

The other three services follow the same pattern:

- `GetVariableList` uses `GetVariable(index)`. Copy `name` and `unit` into
  the byte arrays; an empty name ends the list.
- `GetBufferInfo` uses `HandleGetBufferInfo()`.
- `ArmTrigger` uses `HandleArmTrigger()`. The trigger mode arrives as a raw
  `uint8`; the device rejects values it does not know.

On every spin of the Cyphal thread, publish a pending trigger event:

```cpp
cymon::Device::TriggerEvent event;
if (device.TakeTriggerEvent(&event)) { /* serialize cymon_TriggerEvent_0_1 and publish */ }
```

When a `cymon.TriggerEvent.0.1` from another node arrives on the subscribed
trigger subject, call `device.RequestTrigger()`.

## 5. Sampling (interrupt)

Register the variables once at start-up and call `Tick()` at a fixed rate,
after the values of that period are written:

```cpp
device.RegisterVariable("i_q", "A", &measurement.i_q);
device.SetTickPeriod(62.5F);  // the rate Tick() is called at [us]
...
void OnControlCycle() {
  // ... control ...
  device.Tick();
}
```

`Tick()` copies at most `CYMON_MAX_CHANNELS` floats and evaluates one
comparison. It reads the variables only through their pointers, so it is
allowed in a zero-latency interrupt.

## 6. Check with the host tool

```sh
cymon serve --iface canable:/dev/ttyACM0 --bitrate 1000000   # see the cymon README
```

On the **Scope** page of the tool, each node with cymon services shows its
variables. If a node is missing, its `uavcan.srv.cymon_*` registers are
probably absent or set to 65535.
