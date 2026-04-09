# WIFI_SCAN: Wifi scanning

## Description

This module focuses on wifi scanning and the sending of results.

## Operation definition

Scanning intervals can be actively set in the SmartParks connect app (Configure intervals). On each
interval the application performs a scan of detectable wifi networks for 2 seconds and prints the
complete basic information of all detected networks.

## Settings

Configurable settings:

| Setting                       | Description                                                |
| ----------------------------- | ---------------------------------------------------------- |
| wifi_scan_interval            | Repeat wifi scan after duration                            |
| wifi_scan_aggregated_interval | Repeat sending wifi aggregated scan results after duration |

---

### Debug settings

Debugging of the module is possible with the following setting:

| Setting                                 | Accepted value | Description                                                               |
| --------------------------------------- | -------------- | ------------------------------------------------------------------------- |
| wifi_scan_report_zero_connections_found | True, False    | The device will report even if there were no scan results (empty message) |
