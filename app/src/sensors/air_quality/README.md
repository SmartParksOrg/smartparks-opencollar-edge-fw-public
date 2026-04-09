# Air quality

This module is used for performing air quality measurements. It combines the use of BME690 and
BMV080 sensors.

> [!IMPORTANT] This module is available only for `rangeredge` boards, for which a special firmware
> type needs to be used.
>
> To use this feature, flash the device (or perform a DFU) of the device. The files for that can be
> found on our
> [Github releases page](https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/releases) in
> `open-collar-air-quality-<version>`.

## BME690 specifics

The BME690 sensor uses the BSEC library to run IAQ calculations. The sampling interval is controlled
by the BSEC library itself (which results in a non-directly-configurable 5 minute interval reports).

## BMV080 specifics

The sensor uses it's Duty cycle mode with an interval of 5 minutes. The sensor is asleep for 4:55
min and samples for the last 5 seconds of the interval. The communication thread checks on every
`air_quality_interval` if there is available data, packs it into a message and send it to the
enabled recipients (e.g. `lr_send_flag`). By default the result are send over LoRa.

## User settings

```json
"air_quality_enabled": {
    "id": "0x86",
    "default": false,
    "min": false,
    "max": true,
    "length": 1,
    "conversion": "bool"
},
"air_quality_interval": {
    "id": "0x87",
    "default": 300,
    "min": 10,
    "max": 86400,
    "length": 4,
    "conversion": "uint32"
}
```

## Messaging structure

| Decoded name        | Description                        | Unit          | Type  | Sensor origin |
| ------------------- | ---------------------------------- | ------------- | ----- | ------------- |
| air_q_pm1_mass      | Mass of particles of size PM 1     | μg/cm³        | float | BMV080        |
| air_q_pm2_5_mass    | Mass of particles of size PM 2.5   | μg/cm³        | float | BMV080        |
| air_q_pm10_mass     | Mass of particles of size PM 10    | μg/cm³        | float | BMV080        |
| air_q_pm1_num       | Number of particles of size PM 1   | particles     | float | BMV080        |
| air_q_pm2_5_num     | Number of particles of size PM 2.5 | particles/cm³ | float | BMV080        |
| air_q_pm10_num      | Number of particles of size PM 10  | particles/cm³ | float | BMV080        |
| air_q_is_obstructed | Is sensor obstructed               | Boolean       | bool  | BMV080        |
| air_q_IAQ           | Index of Air Quality               | IAQ index     | float | BME690        |
| air_q_temperature   | Temperature                        | °C            | float | BME690        |
| air_q_pressure      | Pressure                           | Pa            | float | BME690        |
| air_q_humidity      | Relative humidity                  | r.H.          | float | BME690        |
| air_q_raw_gas       | Raw gas resistance                 | Ohms          | float | BME690        |
