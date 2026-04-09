# VHF beacon

VHF (Very high frequency) transmission beacon. On interval, the feature will add a VHF transmission
event to the LoRaWAN queue.

Only one VHF event can exist inside the LoRaWAN queue at a time, to avoid overfilling the queue.
Processing other device features (GPS fix, LoRaWAN message transmission, s-band, etc.) affects the
VHF pulse interval consistency. Depending on enabled features and their respective settings, the
interval could be delayed from a few seconds to a few minutes.

## User settings

Several user settings are required for VHF beacon operation.

### General operational settings

#### vhf_enabled 0x61

```c
"vhf_enabled": {
    "id": "0x61",
    "default": false,
    "min": false,
    "max": true,
    "length": 1,
    "conversion": "bool"
}
```

#### vhf_num_of_packets_per_burst 0x67

Set the default number of VHF "beeps" per burst

```c
"vhf_num_of_packets_per_burst": {
    "id": "0x67",
    "default": 1,
    "min": 1,
    "max": 255,
    "length": 1,
    "conversion": "uint8"
}
```

#### vhf_time_between_packets_ms 0x68

Set the default time in between VHF "beeps" per single burst. **Only applicable when more than one
packet is sent per burst.**

```c
"vhf_time_between_packets_ms": {
    "id": "0x68",
    "default": 250,
    "min": 1,
    "max": 10000,
    "length": 2,
    "conversion": "uint16"
}
```

#### vhf_external_path 0x69

By default, VHF transmission is done using the on-board LoRaWAN antenna. Enabling the external path,
the board will send the VHF transmission over the external VHF antenna connector.

> [!IMPORTANT] This setting is applicable only for non-rhino\* boards as rhino\* boards do not have
> the appropriate VHF connector. Enabling this setting while using a rhino\* board will produce no
> change in operation.
>
> Affected Rhino\* board types:
>
> - Rhino Edge Cube
> - Rhino Puck50
> - Rhino Puck35

```c
"vhf_external_path": {
    "id": "0x69",
    "default": false,
    "min": false,
    "max": true,
    "length": 1,
    "conversion": "bool"
}
```

#### vhf_tx_frequency_khz 0x6A

Command example: Set the VHF transmission frequency to 150000: `0x6A 0x04 0xF0 0x49 0x02 0x00` send
to port 3.

```c
"vhf_tx_frequency_khz": {
    "id": "0x6A",
    "default": 150000,
    "min": 150000,
    "max": 300000,
    "length": 4,
    "conversion": "uint32"
}
```

#### vhf_single_pulse_duration_ms 0x6B

Sets the duration of each pulse inside of a burst.

```c
"vhf_single_pulse_duration_ms": {
    "id": "0x6B",
    "default": 20,
    "min": 5,
    "max": 10000,
    "length": 2,
    "conversion": "uint16"
}
```

### Two intervals operation

VHF scheduling allows for two separate intervals

Two-interval operation is supported. To use 2 intervals functionality, setting:
`vhf_multiple_intervals` with id: `0x66` must be turned on with command: `66 01 00` send to port 3.

We can divide each day into two intervals: `interval1` and `interval2`. The start of each interval
is defined by `vhf_interval1_start` and `vhf_interval2_start` respectively. Supported values are
from 0 to 23, representing UTC time hours. Using these settings, the intervals are defined as:
`interval1: [vhf_interval1_start, vhf_interval2_start)` and
`interval2: [vhf_interval2_start, vhf_interval1_start)`.

> [!NOTE]Notes
>
> - Setting both intervals to the same hour will result in the selection of `interval1`.
> - Setting `interval1` or `interval2` to 0 will turn off the feature for that interval
>   respectively.

#### Example

```javascript
vhf_interval1: 15 min
vhf_interval2: 1 h
vhf_interval1_start: 7
vhf_interval2_start: 18
```

This means that at 07:00 UTC, the device will produce a VHF burst every 15 minutes. When we switch
to `interval2` at 18:00 a VHF burst will be transmitted every hour until the next day at 7.00 when
we switch back to `interval1`.

Start of `interval1` can be changed with command: `64 01 val_in_hex_format` send on port 3.

#### vhf_interval1 0x62

```c
"vhf_interval1": {
    "id": "0x62",
    "default": 60,
    "min": 0,
    "max": 86400,
    "length": 4,
    "conversion": "uint32"
}
```

#### vhf_interval1_start 0x64

```c
"vhf_interval1_start": {
    "id": "0x64",
    "default": 7,
    "min": 0,
    "max": 23,
    "length": 1,
    "conversion": "uint8"
}
```

#### vhf_multiple_intervals 0x66

```c
"vhf_multiple_intervals": {
    "id": "0x66",
    "default": false,
    "min": false,
    "max": true,
    "length": 1,
    "conversion": "bool"
}
```

#### vhf_interval2 0x63

```c
"vhf_interval2": {
    "id": "0x63",
    "default": 60,
    "min": 0,
    "max": 86400,
    "length": 4,
    "conversion": "uint32"
}
```

#### vhf_interval2_start 0x65

```c
"vhf_interval2_start": {
    "id": "0x65",
    "default": 19,
    "min": 0,
    "max": 23,
    "length": 1,
    "conversion": "uint8"
}
```
