# Fence module

Fence module is a support interface module for fence board add-on module, available for RangerEdge
HW v1.6.0 and later. Sample, demonstrating its functionality can be found in the samples folder.

## Usage

To enable fence support, user must set `CONFIG_FENCE_PORT=y`. Config is dependent on RangerBoard
build and it is selected by default for RangerEdge HW v1.6.0 and later builds.

Two interface functions are available:

- `int fence_init(void)` - initializes enable GPIO pin and configures ADC device, as defined in dts
  file.
- `int fence_measure(void)` - turns on power for the module, then waits for the part of the fence
  signal without the pulse. If that is detected before timeout, measuring sequence, with duration
  defined in settings is performed. During measurement sequence, peeks are counted, their average
  voltage in mV is calculated, as well as average energy of the pulse. Module returns message with
  data and turns off the power supply to the fence module.

## User settings

User can control module performance by changing the following settings. Keep in mind settings will
have action only if used with HW that supports fence module.

### fence_enabled - 0x3F

A boolean value enabling/disabling fence functionality.

One can monitor fence status by observing `byte[15]` of the status message. Bit 3 indicates if fence
module is enabled or disabled. Refer to message parser for more information.

### fence_interval - 0x40

An `uint32_t` value determining interval to perform fence measurement in seconds. If set to 0,
measurement will never happen.

### fence_sampling_length - 0x41

An `uint16_t` value determining measurement length in seconds. Its minimum value is 1 second and max
60 s.

### fence_mv_scaling_factor - 0x42

An `uint32_t` scaling factor representing scaling value that measured mV value should be multiplied
with. Set to 10000 (corresponding to 10kV when measuring 1000mV ) by default.

## Message

Standard message header is added to the beginning of the fence message. Other bytes are produced by
fence module:

- `byte[0]` - msg ID `0x92`
- `byte[1]` - msg length 6 bytes
- `byte[2]` - success/fail measurement status
- `byte[3]` - pulse counter
- `byte[4-5]` - average peak voltage in V
- `byte[6-7]` - average pulse energy

If module is disabled, no message will be sent. If HW does not support fence module, no message will
be sent.

Measurement status can return the following codes:

- `FENCE_MEASUREMENT_SUCCESSFUL = 0` - measurement performed successfully
- `FENCE_ERR_POWER = 1` - did not manage to power up fence module
- `FENCE_ERR_NO_PULSE = 2` - initial part with no pulse signal was not detected before timeout
- `FENCE_ERR_ADC = 3` - error on ADC read
- `FENCE_ERR_DEFAULT = 4` - other error

Messages are sent to the `port_fence` - 12.

## Commands

User command **cmd_fence_measure - 0xC8** can be used to initiate fence measurement process and
sending of data message. If called on HW that does not support fence module, or fence module is
disabled, error will be returned.
