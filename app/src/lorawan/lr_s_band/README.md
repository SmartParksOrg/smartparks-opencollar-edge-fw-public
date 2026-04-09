# LR S-Band

## Usage with smtc modem

Standalone lr module for basic s-band usage support. Before using s-band functionality, user is
responsible for suspending smtc modem engine if in use. After usage, engine should be resumed if
needed. See implementation in the `lorawan.c` under `LORAWAN_EVENT_S_BAND_MESSAGE` case handling -
interface for app usage of s-band module.

## Usage

Before attempting to send message via s-band, NwkSKey, AppSKey and Device address should be passed
to the module using `lr_s_band_set_keys()` function. Same set of keys will be used for message
generation until new set is provided. If caller of the send function wants to be notified when TX is
completed, external handler if type `lr_s_band_tx_done_handler_t` can be registered using
`lr_s_band_tx_done_handler_register` function.

To send message, call `lr_s_band_send_message()` passing lr context structure, binary payload,
length and desired lorawan port to use. Module will configure lr1120 radio for fhss uasge and
generate lorawan compatible message using provided keys, binary payload and provided port. Tools for
message generation are located in the `lorawan_tools` module. Frame counter will be manually
increased on each send and reset on device reboot. If rf front-end module is available, it will be
set to TX mode to amplify transmission power.

## User settings

Several user settings are required for correct s-band send operation. They are managed by interface
modules and are just passed to the s-band module functions.

### s_band_send_interval - 0x20

`uint32_t` interval of sending status and short position message vsa s-band in seconds. By default,
interval is disabled. To change it use standard method via LoRaWAN or BT app. For example to set 5
minute interval first convert 300 s to hex format, little endian encoded: `[2C 01 00 00]`. Then for
LoRaWAN send message of the format:

```txt
20 04 [value]
```

or in this case:

```txt
20 04 2C 01 00 00
```

to port 3. If using BT app set port number as first value and send it in decimal format:

```txt
3 32 4 44 1 0 0
```

or hex format:

```txt
0x03 0x20 0x04 0x2c 0x01 0x00 0x00
```

### lp0_app_key - 0x44

16 byte array containing AppSKey for correct message encoding.

### lp0_network_key - 0x45

16 byte array containing NwkSKey for correct message encoding.

### lp0_dev_addr - 0x46

4 byte array containing Device address for correct message encoding.

### s_band_rf_frequency_hz - 0x4F

Set the frequency on which s-band LoRa operates. Type `uint32`.

```C
Default value: 2008450000
Min: 1980000000,
Max: 2100000000
```

### s_band_send_mode - 0x6C

Set the s-band send mode. Available send modes:

```C
s_band_send_mode {
 LR_S_BAND_SEND_WITHOUT_FHSS = 0,
 LR_S_BAND_SEND_WITH_FHSS = 1,
 LR_S_BAND_SEND_BOTH = 2
}
```

## User commands

### cmd_s_band_send - 0xBD

To request immediate s-band send of status and short position message vsa s-band send
"cmd_s_band_send" command. Using LoRaWAN send message:

```txt
BD 00
```

to port 32. Using BT app send custom command:

```txt
32 189 0
```

or in hex format:

```txt
0x20 0xbd 0x00
```

keep in mind you will be disconnected from the device immediately as s-band send cannot be preformed
while having active BT connection.
