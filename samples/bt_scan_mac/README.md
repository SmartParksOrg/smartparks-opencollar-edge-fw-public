# Bluetooth sample: scan for mac address

## Description

This sample focuses on bluetooth scanning, matching and displaying data from the provided bluetooth
mac address.

> [!IMPORTANT] This sample shows a use-case for scanning and displaying data from the Elephant BLE
> sub-skin implant. If you're not using the sample for this use-case, you need to change the
> `process_scan_data` function to suit the data format of your bluetooth advertiser. With the
> Elephant implant, the device advertises data in bursts of 8 packets every 3 minutes. More
> information about the Elephant BLE sub-skin implant can be found
> [by following this link](https://github.com/IRNAS/smartparks-opencollar-edge-fw/issues/389).

## Configuration

User configurable sampling parameters, acceptable values and definitions. These can be changed at
the top of `main.c`. | Constant| Accepted values | Comments| |--------|------------------|---------|
| MAC | Hex. number | Set the mac address you're scanning for. Example: 0x1234567890ab |
SCAN_INTERVAL | [0 ≈4300000000]s | Set the Bluetooth waiting time in between scans (seconds). |
SCAN_DURATION | [50 200000]ms | Set the Bluetooth scan duration of each scan (milliseconds).

## Building and Flashing

To build and flash you can use:

This sample was created and tested for the rangeredge_nrf52840 and nrf52840dk_nrf52840. You can use
the commands bellow to build and flash this sample to one of the before mentioned boards.

- rangeredge_nrf52840

```bash
east build -b rangeredge_nrf52840 && east flash
```

- nrf52840dk_nrf52840.

```bash
east build -b nrf52840dk_nrf52840 && east flash
```
