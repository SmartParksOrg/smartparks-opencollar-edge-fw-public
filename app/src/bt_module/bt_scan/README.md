# BT_SCAN: Bluetooth scanning

## Description

This module focuses on bluetooth scanning and the sending of results.

Available Bluetooth operations:

- Single scan
- Aggregated scan (only sends already aggregated results)

## Operation definition

Scanning intervals can be actively set in the SmartParks connect app (Configure intervals).

### Single scan

Perform a Bluetooth scan. Scans passively for a configurable duration. See settings below for the
description. When scanning, each detected device triggers a callback, which saves the scan result
into a buffer. Results can be filtered by setting the `ble_scan_filter` setting to one of the
specified filter types (use provided enumeration). The results are then ordered by RSSI value.

### Aggregated scan

Sends aggregated results of BT scans without actually conducting a scan. After sending empties the
results buffer.

## Settings

Configurable settings:

| Setting                      | Description                                                     |
| ---------------------------- | --------------------------------------------------------------- |
| ble_scan_duration            | Duration of the bluetooth scan                                  |
| ble_scan_interval            | Repeat bluetooth scan after duration                            |
| ble_scan_aggregated_interval | Repeat sending bluetooth aggregated scan results after duration |
| ble_scan_filter              | Filter bluetooth scan results (see definitions below)           |
| ble_scan_manufacturer_id     | Search for devices with a specific manufacturer ID              |

---

### Filter types

| Filter               | Filter type description           | Enumeration |
| -------------------- | --------------------------------- | ----------- |
| BT_SCAN_FILTER_NONE  | Don't filter                      | 0           |
| BT_SCAN_FILTER_SP    | SmartParks manufacturer ID        | 1           |
| BT_SCAN_FILTER_MAC   | MAC address (not implemented yet) | 2           |
| BT_SCAN_FILTER_PHONE | Phone/mobile device               | 3           |

---

### Debug settings

Debugging of the module is possible with the following setting:

| Setting                                | Accepted value | Description                                                               |
| -------------------------------------- | -------------- | ------------------------------------------------------------------------- |
| ble_scan_report_zero_connections_found | True, False    | The device will report even if there were no scan results (empty message) |

### Manufacturer ID

Scan for devices with specific manufacturer ID.

```json
"ble_scan_manufacturer_id": {
    "id": "0x72",
    "default": 2657,
    "min": 0,
    "max": 65535,
    "length": 2,
    "conversion": "uint16"
}
```

The setting default (2657) converts to `0x0A61` which is the Smart Parks manufacturer ID.
