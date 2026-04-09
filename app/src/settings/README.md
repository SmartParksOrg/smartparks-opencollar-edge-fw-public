# Settings module

Settings module controls stored values, commands and messaging forms. Structures are auto-generated
at build time from [settings.json](../../../scripts/settings/settings.json) allowing for convenient
modification when new HW or tracker is introduced. More on how [auto-generated](generated_settings)
files are created is [described in settings scripts module](../../../scripts/settings/README.md).

## User commands instructions

User can change pre-defined settings and send commands to devices from mobile app or via LoRaWAN
communication.

### Settings

List of settings, tracker supports, is specified in
[settings.json](../../../scripts/settings/settings.json) file. All available settings are located
under "settings" cluster. Each setting is of the format:

```javascript
"settingname": {
        "id": "0x01",
        "default": 0,
        "min": 0,
        "max": 1000000,
        "length": 4,
        "conversion": "uint32"
}
```

where:

- `id` - `uint8_t` value
- `default` - default value
- `min` - min value
- `max` - max value
- `length` - in bytes
- `conversion` - data type, possible options: `uint8`, `int8`, `uint16`, `int16`, `uint32`, `int32`,
  `float`, `byte_array`, `bool`.

To change any setting, a custom command can be send via BT app or using LoRaWAN communication. Port
3 is used for commands handling.

**Change settings via LoRaWAN:**

Send downlink message to port 3 of the format:

```txt
id length [data in byte array format]
```

To set the above "settingname" example setting to value 1000, first convert 1000 to byte array:
`E8 03 00 00` and replace id with `01` and length with `04`:

```txt
01 04 E8 03 00 00
```

> [!IMPORTANT] All integer settings must be encoded in little-endian. Byte arrays are specified from
> left to right ([see example below](#example-outdoor-detection-parameters)).

#### Example (Outdoor detection parameters)

Lets set the outdoor detection parameters to the below specified values:

| Parameter            | Value  | Value \* 1000 | Hex (little endian) | Byte position |
| -------------------- | ------ | ------------- | ------------------- | ------------- |
| Bias                 | -4.917 | -4917         | {0xCB,0xEC}         | 0 - 1         |
| Temperature weight   | 4.715  | 4715          | {0x6B, 0x12}        | 2 - 3         |
| Accelerometer weight | 4.906  | 4906          | {0x2A, 0x13}        | 4 - 5         |
| Hour weight          | 3.961  | 3961          | {0x79,0x0F}         | 6 - 7         |
| Time-zone offset (s) | 7200   | 7200          | {0x20, 0x1C}        | 8 - 11        |

```json
"outdoor_detection_parameters": {
    "id": "0x7C",
    "default": "{0xCB,0xEC,0x6B,0x12,0x2A,0x13,0x79,0x0F,0x20,0x1C,0x00,0x00}",
    "min": "{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}",
    "max": "{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}",
    "length": 12,
    "conversion": "byte_array"
}
```

Setting id: 124 (0x7C)

Setting length: 12 (0x0C)

Resulting settings message:

```txt
0xCB 0xEC 0x6B 0x12 0x2A 0x13 0x79 0x0F 0x20 0x1C 0x00 0x00
```

Set the port to which we sent the message (our example uses `port 3` which is used by all settings).
If using Bluetooth or similar, add the port number in front of the payload
([see below for more information](#change-settings-via-bt-app)).

#### Change settings via BT app

Use "Custom command" field to set settings that are not supported in the user interface. The main
change when using the app for sending commands is that port needs to be specified as the first
value. App supports commands in hex format with "0x" preamble or in decimal format. The above
setting can be send either as:

```txt
0x03 0x01 0x04 0xE8 0x03 0x00 0x00
```

or as

```txt
3 1 4 232 3 0 0
```

Spaces need to be added!

## Commands

Various commands are available for user to interact with tracker. All commands are defined in
[settings.json](../../../scripts/settings/settings.json) file under "commands" cluster. Each command
is of the format:

```javascript
"cmd_name": {
        "id": "0xA1",
        "length": 1,
        "conversion": "uint8",
        "value": 0
}
```

where:

- `id` - `uint8_t` value
- `length` - in bytes (of extra command data - 0 if none is needed)
- `conversion` - data type of extra data, see options for settings (`uint8` if length is 0)
- `value` - default value, 0 if none

Commands are send in similar way to settings via BT app or using LoRa communication. Port 32 is used
for sending commands.

**Send command via LoRa:**

Send command in the following form to port 32:

```txt
id length [value in byte array format]
```

Keep in mind that additional value is always lille-endian encoded. If command length is 0, no
additional value needs to be send, hence:

```txt
id 0
```

For example to send "cmd_name" test command with value 3, the following string is send to port 32:

```txt
A1 01 03
```

**Send command via BT app:**

Same format as defined above is used. Again port needs to be added as first value in the array. The
above command can be send in either of the formats:

```txt
0x20 0xA1 0x01 0x03
```

or

```txt
32 161 1 3
```

## List of available commands

The following commands are supported at the moment. All need to be send to port 32. Below, message
formats for using LoRa communication are given. To adjust message format for BT, follow the
instructions above.

### A0 - cmd_join

Start LoRaWAN joining sequence. Leave network if already joined and start re-joining.

```txt
A0 00
```

### A1 - cmd_reset

Reboot tracker.

```txt
A1 00
```

### A2 - cmd_send_all_val

Tracker will return all values, in stacked format, as described in the following command. Not
recommended to use via LR, due to payload size limitations.

```txt
A2 00
```

### A3 - cmd_send_single_val

Obtain value of a single value field, specified by its id. List of all values can be found in
settings.json file under "values" cluster.

```txt
A3 01 value_id
```

For example, to get value with id D1 send:

```txt
A3 01 D1
```

Tracker will respond on port 30 in the format:

```txt
value_id value_length [value in byte array format]
```

### A4 - cmd_send_status

Status message will be send via LoRa/BT communication to status port 4.

```txt
A4 00
```

### A5 - cmd_send_position

Obtain tracker latest GPS position (lat lon, alt). Tracker will not obtain new GPS data. Send
command:

```txt
A5 00
```

Tracker will respond with "msg_last_position" on port 31.

### A6 - cmd_reset_gps

Reboot Ublox GPS module.

```txt
A6 00
```

### A7 - cmd_send_all_settings

Tracker will return all settings, in stacked format, as described in the following command. Not
supported for use via LR, due to payload size limitations.

```txt
A7 00
```

### A8 - cmd_send_single_setting

Obtain value of a single setting field, specified by its id.

```txt
A8 01 setting_id
```

For example, to get "cmd_name" send:

```txt
A8 01 01
```

Tracker will respond on port 3 in the format:

```txt
setting_id setting_length [setting value in byte array format]
```

### A9 - cmd_reset_initial_position

Reset initial reference position of the tracker to hardcoded position. (not recommended to use)

```txt
A9 00
```

### AA - cmd_reset_initial_time

Reset initial reference time to hardcoded value. (not recommended to use)

```txt
AA 00
```

### AB - cmd_clear_nvs

Clear internal storage. All set values are going to be deleted and set to default. Keep in mind that
LR keys are going to be deleted as well!

```txt
AB 00
```

### AC - cmd_reset_to_def_settings

Revert all setting to default values. Keep in mind that LR keys are going to be deleted as well!

```txt
AC 00
```

### AD - cmd_send_status_lr

Status message will be send via LoRa communication to status port 4.

```txt
AD 00
```

### AE - cmd_send_lr_fix

Obtain LR GNSS message and send it via LoRa communication.

```txt
AE 00
```

### AF - cmd_set_location_and_time

Set reference location and time of the tracker.

```txt
AF 0C [lon*10^7 int32 byte array] [lat*10^7 int32 byte array] [unix timestamp uint32 byte array]
```

### B1 - cmd_get_wifi_scan

Get aggregated data in wifi scan. Tracker will response with "msg_wifi_scan_aggregated" (see next
chapter).

```txt
B1 00
```

### B2 - cmd_get_ble_scan

Get aggregated data of BT scan. Tracker will response with "msg_ble_scan_aggregated" (see next
chapter).

```txt
B2 00
```

### B3 - cmd_get_flash_status

Request flash status message.

```txt
B3 00
```

### B4 - cmd_get_lr_satellite_data

Perform LR GPS scanning and return both GNSS message as well as satellite data message, used in the
fix.

```txt
B4 00
```

### B5 - cmd_get_ublox_satellite_data

Perform Ublox position fix and return both position message, as well as satellite data message, used
in the fix.

```txt
B5 00
```

### B6 - cmd_almanac_update

Update almanac.

Due to its length, full Almanac needs to be updated in several chunks. Total Almanac consists of 20
Bytes (Header) +128 (number of SV) \* 20 Bytes =2580 Bytes. Using the command: "cmd_almanac_update"
user can provide parts of Almanac data to device either via LoRa or BT communication. Each command
must be send to port 32 and must be of the form:

```txt
byte 0: cmdID 0XB6
byte 1: length of the cmd
byte 2-21: Almanac (id is obtained from first byte, as represents satellite id and 128 - header).
... stack as many as available
```

On received header with ID 128, FW will update Almanac. Update will take place only if provided date
of the new Almanac is more recent than already stored one.

### B7 - cmd_get_mac

Get BT MAC address from the device.

```txt
B7 00
```

Device responds with "msg_mac_id".

### B8 - cmd_get_ublox_fix

Perform Ublox position fix and return position message.

```txt
B8 00
```

### B9 - cmd_reset_lr

Reset LR module.

```txt
B9 00
```

### BA - cmd_flash_clear

Clear all data from external flash.

```txt
BA 00
```

### BB - cmd_flash_get_all

Get all messages stored on specific port. Use port=0 to get messages from all ports.

```txt
BB 01 port_nr
```

Tracker will start to return all available messages from head to tail on port 29 via BT/LoRa. When
all messages are send, confirmation message, "msg_cmd_confirm", will be received.

### BC - cmd_flash_get_from_head

Read specific number of messages, starting from specified message from head on defined port.

> [!NOTE] The flash head is always located at the last saved message in flash. The flash is read
> from the last populated block backwards.

```txt
BC 0C [port_nr encoded as 4 bytes array] [start msg  nr. (from head) - 4 bytes] [nr. of messages - 4 bytes]
```

To read 10 messages on port 4, starting from 50th message, send:

```txt
BC 0C 04 00 00 00 32 00 00 00 0A 00 00 00
```

Tracker will start to return all available messages from head to tail on port 29 via BT/LoRa. When
all messages are send, confirmation message, "msg_cmd_confirm", will be received.

### BD - cmd_s_band_send

```txt
BD 00
```

Tracker will compose and send an s-band message.

### BE - cmd_set_operation_mode_com_th

Set operation mode of the lora - GPS thread. User can pass 3 values:

- 0 - thread disabled - thread will only receive messages from other threads and report to watchdog
- 1 - thread low power - disabled functionality + status message will be send via LoRaWAN
- 2 - normal operation

Send message:

```txt
BE 01 operation_mode
```

### BF - cmd_disable_flash_th

Disable flash thread operation. Nothing will be stored to flash.

```txt
BF 00
```

### C0 - cmd_disable_bt_th

Disable BT thread operation. BT module will not be active.

```txt
C0 00
```

### C1 - cmd_set_operation_mode_main_th

Set operation mode of the main thread. User can pass 3 values:

- 0 - thread disabled - BT advertising will be turned off
- 1 - thread low power - BT advertising will be turned off
- 2 - normal operation

Send message:

```txt
C1 01 operation_mode
```

### C2 - cmd_check_pin

If pin code is set, device need to validate pin code before allowing BT connection from another
device. Pin code will be validated against value stored in settings or LoRaWAN app key. It can be up
to 16 bytes long.

```txt
C2 pin_len pin[pin_len]
```

### C3 - cmd_set_hibernation_mode

Set device in the hibernation mode - same effect as reed switch. Keep in mind only magnet can reboot
device from hibernation as BT and LoRAWAN modules are disabled!

```txt
C3 00
```

### C4 - cmd_send_lr_message

Send message to device to be send via LR.

```txt
C4 data_len data[data_len]
```

Up to 46 bytes are supported.

### C5 - cmd_read_all_lr_messages

> [!NOTE] This command is used for debugging and only works when sent over Bluetooth.

```txt
C5 00
```

Read all incoming internal messages (`execute`, `send` or `store`) currently in the LoRaWAN thread
and send them over Bluetooth. Read messages are removed from queue.

### C6 - cmd_send_sat_buffer

```txt
C6 00
```

Tracker will try to send a satellite message.

### C8 - cmd_fence_measure

```txt
C8 01 00
```

Measure fence voltage.

### C9 - cmd_aggregated_bt_scan

```txt
C9 00
```

Internal command to initiate periodic BT scan between threads. Aggregated message will be
stored/send as defined in sending flags. Not intended to use by user.

### CA - cmd_single_bt_scan

```txt
CA 00
```

Internal command to initiate periodic BT scan between threads. Single scan message will be
stored/send as defined in sending flags. Not intended to use by user.

### CB - cmd_bt_disconnect

```txt
CB 00
```

Disconnect a Bluetooth device if connected.

### CC - cmd_send_bt_cmdq_results

```txt
CC 00
```

Send latest CMDQ scan results. (Does not initiate a new CMDQ scan).

### CD - cmd_decouple_collar (Collaredge board specific feature)

```txt
CD 00
```

CollarEdge has a specific port used for the decoupling / releasing of the collar. On command the
device turns on power to the connector for 5 seconds.

### CE - cmd_send_timestamp

```txt
CE 00
```

The device will send its current unix timestamp.
