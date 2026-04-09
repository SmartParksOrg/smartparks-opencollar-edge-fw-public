# Satellite module

For RangerEdge HW, version 1.4, satellite module add-on is available.

## FW build

To enable satellite module support, separate FW branch is available. Main modifications from
standard build are:

1. satellite module is supported only for RangerEdge HW v1.4. Hence in
   `rangeredge_nrf52840_1_4_0.conf` file, config:

```txt
CONFIG_SATELLITE=y
```

enables satellite support. If not selected module is not going to be included and called.

1. Communication between satellite add-on and RangerEdge board is done via uart. Hence `rb-uart`
   alias needs to be set in DT. For RangerEdge v1.4 see implementation in
   `rangeredge_nrf52840_1_4_0.overlay` file.

2. As satellite module is utilizing uart for communication, RTT is used for logging. Uart configs
   for logging:

```txt
select UART_CONSOLE
select LOG_BACKEND_UART
```

are replaced with

```txt
select RTT_CONSOLE
select USE_SEGGER_RTT
```

At the moment RTT is selected for all tracker types in main Kconfig. This needs to be changed in the
feature.

## Satellite communication functionality

Tracker advertises if satellite functionality is currently enabled or not in the status in byte
`features` (byte 15 if 0. and 1. byte are count as header), bit 1 (enabled/disabled). In the FW,
functionality is stored as the global `sys_features.satellite_com`, set false on boot.

User can enable or disable satellite functionality by setting value of setting:

```json
"satellite_enabled": {
    "id": "0x3A",
    "enabled": true,
    "default": true,
    "min": false,
    "max": true,
    "length": 1,
    "conversion": "bool"
}
```

Communication with satellite module is done in the `satellite_thread`, which is created only if
`CONFIG_SATELLITE` is selected.

### Initialization

In init function enable GPIO pin and uart peripheral are initialized. If satellite functionality is
working and user did enable satellite module with `satellite_enabled` setting,
`sys_features.satellite_com` will be set to true and otherwise to false during initialization.

### Turning on and off satellite functionality

If supported in HW, user can turn on/off satellite support by changing `satellite_enabled` setting.

### Sending messages via satellite module

Sending messages via satellite module is governed similar to LoRaWAN send. Setting flag:

```json
"sat_send_flag": {
    "id": "0x39",
    "enabled": true,
    "default": 138,
    "min": 0,
    "max": 4294967295,
    "length": 4,
    "conversion": "uint32"
},
```

determines if message on port 1 - 32 will be send via satellite or not. If for specific port bit is
set, message will be transferred to satellite thread that indefinitely waits for new command message
to arrive. New message is added to send buffer of length 340 byes. In similar way to storing to
flash, message is added to the buffer as:

- msg port number
- message
- timestamp

If there is not enough space in the buffer, the oldest message in the buffer is removed and new one
added.

### Periodic sending of the satellite buffer

Satellite send buffer is send periodically. Communication thread checks if buffer needs to be send.
Send period is determined by setting:

```json
"satellite_send_interval": {
    "id": "0x04",
    "enabled": true,
    "default": 3600,
    "min": 0,
    "max": 86400,
    "length": 4,
    "conversion": "uint32"
},
```

If satellite module is enabled and period is reached, communication thread will send
`CMD_SEND_SAT_BUFFER` command to satellite thread. User can also send the same command to initiate
buffer sending.

When send command is received in satellite thread, we check if send buffer is empty. If this is the
case, status message is added to the buffer. Module then enters sending mode by enabling satellite
module and configuring port. Message is queued and send/receive loop is entered. Number of retries
is defined by user setting:

```json
"satellite_retry": {
    "id": "0x3B",
    "enabled": true,
    "default": 10,
    "min": 1,
    "max": 15,
    "length": 1,
    "conversion": "uint8"
}
```

where delay between retries is determined at random by formula:

- 1-5 s for first 3 retries
- 5 - 20 s for first 6 retries
- 20 - 40 s for all the others

In same loop module also tries to receive messages, that are parsed in the same way as other
messages received via LR or BT module.

Current number of retries is also reported in device status message in byte byte 15 if 0. and 1.
byte are count as header, represented as integer number stored in bits 4 - 8, with max value 32.

### User command for sending satellite buffer

User can initiate immediate send of satellite buffer by calling the command:

```json
"cmd_send_sat_buffer": {
    "id": "0xC6",
    "length": 0,
    "conversion": "uint8",
    "value": 0
},
```
