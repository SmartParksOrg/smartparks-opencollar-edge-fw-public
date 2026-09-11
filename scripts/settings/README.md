# Auto-generated settings

Module for auto-generating settings files from a JSON structure.

> [!IMPORTANT] When upgrading from versions `v7.3.0` and older to `v7.4.0` and newer, a settings
> transfer is performed. The firmware automatically detects the presence of old settings and
> transfers them into the new settings structure. Afterwards it clears the flash space where old
> settings were saved in an effort to avoid the accidental use of stale settings, so downgrading
> from `v7.4.0` will effectively set all settings back to their default values.

## settings.json

JSON file that defines settings, values, commands and messages. `settings.json` structure is parsed
at build time to several `.h` and `.c` files, as described below.

It contains the following main fields:

- `settings_family` - family names and their byte values
- `settings` - persistent, user-configurable settings
- `commands` - actions that can be requested from the device
- `values` - runtime values that can be read from the device
- `messages` - descriptions of messages sent by the device
- `ports` - communication port names and numbers

### settings_family

Settings are grouped into families according to the subsystem that uses them. A setting is uniquely
identified by the pair `(family, id)`, not by `id` alone. This allows each family to use its own ID
range; for example, `tracker_type`, `lr_send_flag`, and `ble_adv` all have ID `0x00`, but belong to
families `0x02`, `0x03`, and `0x04`, respectively.

For persistent storage, the two bytes are combined into a 16-bit key as `(family << 8) | id`. The
family byte is also included before the ID when settings and values are transferred over the
settings protocol. Commands, messages, and ports are not assigned to families.

The currently defined families are:

| Name               | ID     | Current entries       | Used for                                                                                                                                                                                        |
| ------------------ | ------ | --------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `old`              | `0x00` | No current settings   | Permanently reserved for legacy settings from firmware before v7.4.0. This family must never be reused; it is scanned while migrating settings from the old, single-ID namespace.               |
| `reserved_special` | `0x01` | Internal records only | Protected firmware storage such as time and position state, flash bookkeeping, and the LoRaWAN context. This family must not be used for entries in `settings.json`.                            |
| `general`          | `0x02` | 13 settings           | Device-wide behavior and identity, including tracker type, status and diagnostic intervals, logging, Wi-Fi scan scheduling, device name/PIN, and LED control.                                   |
| `flags`            | `0x03` | 3 settings            | Bit fields that select which data is sent over long-range radio, stored in flash, or sent over satellite.                                                                                       |
| `ble`              | `0x04` | 16 settings           | Bluetooth advertising, scanning, filtering, connection behavior, and BLE command-queue operation.                                                                                               |
| `lora`             | `0x05` | 36 settings           | Long-range communication: LoRaWAN credentials and behavior, LR GNSS, S-band, VHF, and LP0 radio parameters.                                                                                     |
| `gps`              | `0x06` | 25 settings           | u-blox/GNSS scheduling, fix and retry behavior, initial coordinates, accuracy limits, constellation selection, and motion-triggered positioning.                                                |
| `satellite`        | `0x07` | 7 settings            | Satellite communication enablement, send schedules, and retry behavior.                                                                                                                         |
| `sensors`          | `0x08` | 23 settings           | Accelerometer, fence, outdoor-detection, external-switch, and air-quality sensor behavior.                                                                                                      |
| `values`           | `0xA0` | 20 values             | Readable runtime and diagnostic data such as reset reason, position, acceleration, battery/charging voltage, timestamps, temperatures, and message counts. These are values, not user settings. |

When adding an entry, choose the family by the subsystem that owns the behavior and ensure its ID is
unique within that family. Do not allocate settings in `old`, `reserved_special`, or `values`.

### settings

Each setting field is identified by `setting_name` and contains the following fields:

- `id` - `uint8_t` value, unique within its family
- `family` - `uint8_t` family value from `settings_family`
- `default` - default value
- `min` - min value
- `max` - max value
- `length` - in bytes
- `conversion` - data type, possible options: `uint8`, `int8`, `uint16`, `int16`, `uint32`, `int32`,
  `float`, `string`, `byte_array`, `bool`.

### commands

Each command field is identified by `command_name` and contains the following fields:

- `id` - `uint8_t` value
- `length` - in bytes (of extra command data - 0 if none is needed)
- `conversion` - data type of extra data, see options for settings (`uint8` if length is 0)
- `value` - default value, 0 if none

### values

Each value field is identified by `value_name` and contains the following fields. Values currently
use the dedicated `values` family (`0xA0`):

- `id` - `uint8_t` value, unique within its family
- `family` - `uint8_t` family value from `settings_family`
- `default` - default value
- `min` - min value
- `max` - max value
- `length` - in bytes
- `conversion` - data type, possible options: `uint8`, `int8`, `uint16`, `int16`, `uint32`, `int32`,
  `float`, `string`, `byte_array`, `bool`.

### messages

Each message field is identified by `message_name` and contains the following fields:

- `port` - port message is send message on (must be contained on the `ports` list)
- `id` - `uint8_t` value
- `length` - max length in bytes
- `conversion` - `byte_array`

## Python parser

Python parser `py2h.py` and validation script `validate_data.py` are used to convert `settings.json`
file to `.c` and `.h` files. All auto-generated files are equipped with
`/* AUTOGENERATED FILE - DO NOT MODIFY!  */` comment. Script is automatically run during release
build. During developing process, manually run `py2h.py` to update settings.
