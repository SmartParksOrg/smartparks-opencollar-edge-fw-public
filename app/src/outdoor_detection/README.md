# Outdoor detection module

The Outdoor detection module is designed to detect when the device is outside based on the gathered
accelerometer, temperature, and chronological data. That data is then compared with expert-provided
weights from which the device can approximate the probability that it is outside.

## Basic operation

This module is currently used to control GPS fixes. Currently, the only available expert-provided
weights are for Pangolins.

## Configuration

The module currently operates with 3 user-configurable settings:

```yaml
"outdoor_detection_enabled": {
    "id": "0x7A",
    "default": false,
    "min": false,
    "max": true,
    "length": 1,
    "conversion": "bool"
},
"outdoor_detection_tau": {
    "id": "0x7B",
    "default": 11,
    "min": 0,
    "max": 100,
    "length": 1,
    "conversion": "uint8"
},
"outdoor_detection_parameters": {
    "id": "0x7C",
    "default": "{0xCB,0xEC,0x6B,0x12,0x2A,0x13,0x79,0x0F,0x20,0x1C,0x00,0x00}",
    "min": "{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}",
    "max": "{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}",
    "length": 12,
    "conversion": "byte_array"
}
```

Setting the `outdoor_detection_enabled` to `true` enables this module and disables the classically
used GPS fix interval.

The `outdoor_detection_tau` user settings controls the probability needed for the module to trigger
a GPS fix.

The `outdoor_detection_parameters` are used to fine-tune the weights responsible for calculating the
final outdoor probability. The Parameters are set as follows:

- Bias (2 bytes)
- Temperature weight (2 bytes)
- Accelerometer weight (2 bytes)
- Hour weight (2 bytes)
- Time-zone offset in seconds (4 bytes) - Default offset is set to Central Africa time (GMT+2)

> [!IMPORTANT] All weights need to be little endian encoded integers! All values should be
> multiplied by 1000 prior to saving.

### Configuration example

Lets set the outdoor detection parameters to the below specified values:

| Parameter            | Value  | Value \* 1000 | Hex (little endian) | Byte position |
| -------------------- | ------ | ------------- | ------------------- | ------------- |
| Bias                 | -4.917 | -4917         | {0xCB,0xEC}         | 0 - 1         |
| Temperature weight   | 4.715  | 4715          | {0x6B, 0x12}        | 2 - 3         |
| Accelerometer weight | 4.906  | 4906          | {0x2A, 0x13}        | 4 - 5         |
| Hour weight          | 3.961  | 3961          | {0x79,0x0F}         | 6 - 7         |
| Time-zone offset (s) | 7200   | 7200          | {0x20, 0x1C}        | 8 - 11        |
