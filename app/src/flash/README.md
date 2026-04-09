# External flash module

Add module description and operation logic.

## Flash status

Flash module reports basic stats in flash status message.

**Message format** Flash status message is send on port 14. Same flag configuration options to set
sending flags (send to LR, store to flash, send to satellite) apply as for other messages.

**Message ID:** 0x94. **Message length:** 2 (header) + 5 (data) **Message format:**

- `byte[0]` - ID 0x94
- `byte[1]` - length 0x05
- `byte[2]` - % used (0 - 100 values)
- `bytes[3-6]` - nr. of messages in flash

Periodic sending of flash status message is governed by `flash_status_interval` setting:

```json
"flash_status_interval": {
            "id": "0x43",
            "default": 86400,
            "min": 0,
            "max": 604800,
            "length": 4,
            "conversion": "uint32"
        }
```

that controls number of seconds between sends. If set to 0, message will not be send/stored. By
default it is set to 24h.

User can request flash status using `cmd_get_flash_status` command. Status message will be send on
channel, that command is received on, i.e. if send via LoRaWAN status message will be send via
LoRaWAN, if send via BT, status message will be send via BT.

```json
"cmd_get_flash_status": {
            "id": "0xB3",
            "length": 0,
            "conversion": "uint8",
            "value": 0
        }
```
