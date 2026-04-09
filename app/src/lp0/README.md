# LP0

The LP0 module's primary uses are:

- Ability to send LoRa messages without the LoRaWAN stack, which include:
  - LoRa messages with a custom header,
  - LoRa messages with LoraWAN header (but without the need for the whole stack)
  - S-Band messages (with or without FHSS).
- Scan and discover offloading stations.
- Offload flash-saved messages to offloading station.
- Receiving custom messages.

## Available Modes

Available LP0 modes:

- Message Queue (For LoRa or S-band messages) mode from other device processes
- Discovery mode (for devices that will offload data, e.g. trackers)
  - On successful discovery with a offloading station, the device will start offloading data
- Offload station mode

### 1. Sending/Receiving messages

Send/receive messages with a LoRaWAN header without the use of the LoRaWAN stack. The authentication
is done via ABP (Activation By Personalization).

---

#### Normal LoRa messages

Send LoRa messages by setting the `lp0_send_flag` setting. Messages on the corresponding ports that
you selected will be sent using LP0.

#### S-band messages

TODO-FUTURE: Support for S-band messages sent via LP0 will be added in the future.

---

### 2. Device Discovery

Discover offloading stations in range via pinging. The offloading device (tracker) sends a ping
which consists of a message on port 33 which the offloading station detects and returns an
acknowledge message. Upon a successfully received acknowledge response the device starts
transmitting all flash logs to the offload station.

> [!CAUTION] This feature is still experimental, so before enabling it make sure you have a way to
> manually reboot the device in case of potential errors. Connecting to the offloading device is via
> Bluetooth is permitted (only offloading devices, not offload stations).
>
> This feature dramatically increases power consumption.

<!-- -->

> [!NOTE] TODO-FUTURE: Currently the device continuously transmits all messages, and upon completion
> starts with a new ping which starts a new offload. In the future we will add message confirmation
> logic, which will detect the sent messages and mark sent messages so they're sent only once.
>
> Before the above mentioned message confirmation logic is added the full offload time can cause a
> lorawan engine error on LP1, which can cause a device reboot after offload completes and normal
> LP1 procedures resume.

### 3. Data offload

The device is an offload station in a continuous RX mode, listening for all messages on the selected
frequency. If a message is sent on port 33, that is considered as a ping. The offload station will
respond by sending and acknowledge message back to the device. Messages on all other ports are
passed to the SDFS UART module to be saved to the SD card.

On each successful ping, the device sends an instruction to the SDFS module to create a new file on
the SD card to house the newly received messages and avoid file overflow. If the SDFS module is not
connected, the device listens to the messages but does nothing with them.

> [!CAUTION] This feature is still experimental, so before enabling it make sure you have a way to
> manually reboot the device in case of potential errors. Connecting to the device via Bluetooth
> while the device is performing the continuous RX for detection offloading device pings has the
> potential to stall the device. In such cases a manual reboot is needed to reboot the device.
>
> This feature dramatically increases power consumption.

## User settings and configuration

### LoRa parameters

User configurable LoRa parameters are:

#### Authentication

```json
"lp0_app_key": {
    "id": "0x44",
    "default": "{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}",
    "min": "{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}",
    "max": "{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}",
    "length": 16,
    "conversion": "byte_array"
},
"lp0_network_key": {
    "id": "0x45",
    "default": "{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}",
    "min": "{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}",
    "max": "{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}",
    "length": 16,
    "conversion": "byte_array"
},
"lp0_dev_addr": {
    "id": "0x46",
    "default": "{0x00,0x00,0x00,0x00}",
    "min": "{0x00,0x00,0x00,0x00}",
    "max": "{0x00,0x00,0x00,0x00}",
    "length": 4,
    "conversion": "byte_array"
}
```

#### Sending / receiving frequency

```json
"s_band_rf_frequency_hz": {
    "id": "0x4F",
    "default": 2008450000,
    "min": 1980000000,
    "max": 2100000000,
    "length": 4,
    "conversion": "uint32"
},
"lp0_tx_frequency_hz": {
    "id": "0x89",
    "default": 869150000,
    "min": 0,
    "max": 2100000000,
    "length": 4,
    "conversion": "uint32"
},
"lp0_rx_frequency_hz": {
    "id": "0x8A",
    "default": 869150000,
    "min": 0,
    "max": 2100000000,
    "length": 4,
    "conversion": "uint32"
}
```

#### Other parameters

```json
"lp0_send_flag": {
    "id": "0x88",
    "default": 0,
    "min": 0,
    "max": 4294967295,
    "length": 4,
    "conversion": "uint32"
}
```

Use the `lp0_send_flag` to control which messages will be sent using LP0. Each bit corresponds to a
port number. The least significant bit is used for port 1. The rest follow suit. Example:

"Enable sending over LP0 for ports 3, 8 and 21"

> Set the lp0_send_flag to `1048708` (Bitwise (MSB) notation: 0000 0000 0001 0000 0000 0000
> 1000 0100)

#### LP0 modulation parameters

```json
"lp0_communication_params": {
    "id": "0x8B",
    "default": "{0x07,0x05,0x01,0x01}",
    "min": "{0x00,0x00,0x00,0x00}",
    "max": "{0x00,0x00,0x00,0x00}",
    "length": 4,
    "conversion": "byte_array"
}
```

| Byte # | Description                         | Default value                                                                                                                                                                                         |
| ------ | ----------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 0      | Spreading factor                    | LR11XX_RADIO_LORA_SF7 (0x07)                                                                                                                                                                          |
| 1      | Bandwidth                           | LR11XX_RADIO_LORA_BW_250 (0x05)                                                                                                                                                                       |
| 2      | Coding rate                         | LR11XX_RADIO_LORA_CR_4_5 (0x01)                                                                                                                                                                       |
| 3      | Lorawan RX1 window delay in seconds | 1 second (this setting must be the same across all devices in the same custom network as the durations of custom LP0 rx windows are calculated based on number of devices and delay until RX1 window) |

Available spreading factors:

```txt
- LR11XX_RADIO_LORA_SF5  = 0x05,
- LR11XX_RADIO_LORA_SF6  = 0x06,
- LR11XX_RADIO_LORA_SF7  = 0x07,
- LR11XX_RADIO_LORA_SF8  = 0x08,
- LR11XX_RADIO_LORA_SF9  = 0x09,
- LR11XX_RADIO_LORA_SF10 = 0x0A,
- LR11XX_RADIO_LORA_SF11 = 0x0B,
- LR11XX_RADIO_LORA_SF12 = 0x0C
```

Available bandwidths:

```txt
- LR11XX_RADIO_LORA_BW_10  = 0x08 (Bandwidth 10.42 kHz),
- LR11XX_RADIO_LORA_BW_15  = 0x01 (Bandwidth 15.63 kHz),
- LR11XX_RADIO_LORA_BW_20  = 0x09 (Bandwidth 20.83 kHz),
- LR11XX_RADIO_LORA_BW_31  = 0x02 (Bandwidth 31.25 kHz),
- LR11XX_RADIO_LORA_BW_41  = 0x0A (Bandwidth 41.67 kHz),
- LR11XX_RADIO_LORA_BW_62  = 0x03 (Bandwidth 62.50 kHz),
- LR11XX_RADIO_LORA_BW_125 = 0x04 (Bandwidth 125.00 kHz),
- LR11XX_RADIO_LORA_BW_250 = 0x05 (Bandwidth 250.00 kHz),
- LR11XX_RADIO_LORA_BW_500 = 0x06 (Bandwidth 500.00 kHz),
- LR11XX_RADIO_LORA_BW_200 = 0x0D (Bandwidth 203.00 kHz, 2G4 and compatible with LR112x chips only),
- LR11XX_RADIO_LORA_BW_400 = 0x0E (Bandwidth 406.00 kHz, 2G4 and compatible with LR112x chips only),
- LR11XX_RADIO_LORA_BW_800 = 0x0F (Bandwidth 812.00 kHz, 2G4 and compatible with LR112x chips only)
```

Available coding rates:

```txt
- LR11XX_RADIO_LORA_NO_CR     = 0x00,  (No Coding Rate),
- LR11XX_RADIO_LORA_CR_4_5    = 0x01,  (Coding Rate 4/5 Short Interleaver),
- LR11XX_RADIO_LORA_CR_4_6    = 0x02,  (Coding Rate 4/6 Short Interleaver),
- LR11XX_RADIO_LORA_CR_4_7    = 0x03,  (Coding Rate 4/7 Short Interleaver),
- LR11XX_RADIO_LORA_CR_4_8    = 0x04,  (Coding Rate 4/8 Short Interleaver),
- LR11XX_RADIO_LORA_CR_LI_4_5 = 0x05,  (Coding Rate 4/5 Long Interleaver),
- LR11XX_RADIO_LORA_CR_LI_4_6 = 0x06,  (Coding Rate 4/6 Long Interleaver),
- LR11XX_RADIO_LORA_CR_LI_4_8 = 0x07,  (Coding Rate 4/8 Long Interleaver)
```

#### LP0 Node configuration and parameters

Configure the device by setting the `lp0_node_params`.

| Byte | Setting name                                                 | Additional information                                                                                                                                                                    |
| ---- | ------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 0    | Offload feature                                              | 0 - Offload feature off. Sending over LP0 is still permitted. 1 - Offloading data source (tracker). 2 - Device is an offload station                                                      |
| 1    | Offload station ID                                           | The ID of the station [0-255]                                                                                                                                                             |
| 2    | Maximum Number of nodes with overlapping coverage in network | [0-255]                                                                                                                                                                                   |
| 3    | Use lorawan header when sending ping?                        | 1 = True, 0 = False                                                                                                                                                                       |
| 4    | Ping interval (seconds)                                      | [0 - 255]s Setting the interval to 0 will make the device perform a ping as soon as possible once LP0 returns to idle. Only relevant if device is set as offloading data source (tracker) |

```json
"lp0_node_params": {
    "id": "0x8C",
    "default": "{0x00,0x00,0x0a,0x01,0x3c}",
    "min": "{0x00,0x00,0x00,0x00,0x00}",
    "max": "{0x00,0x00,0x00,0x00,0x00}",
    "length": 5,
    "conversion": "byte_array"
}
```

## Custom header definition

The custom header used for LP0 communication is defined as:

| Byte #  | Description         |
| ------- | ------------------- |
| 0-1     | Sync bytes          |
| 2-5     | Dev ID (LSB)        |
| 6       | Message type / Port |
| 7       | Payload length      |
| 8-(n-1) | Payload data        |
