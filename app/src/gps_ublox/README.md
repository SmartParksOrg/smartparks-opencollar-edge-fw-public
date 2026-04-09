# GPS module

Ublox GPS module is present on all tracker HW types.

## Basic operation

Tracker tries to obtain GPS fix on defined interval time and send position data via
LoRaWAN/satellite/store it to flash, based on set flags for specific message.

GPS interval is defined with user setting `ublox_send_interval` with id `0x02` representing
`uint32_t` interval in seconds.

Two position messages can be produced:

---

**Standard message** is send on port 2 and contains the following data:

| Byte(s) | Explanation                |
| ------- | -------------------------- |
| 0       | message id                 |
| 1       | payload length\*           |
| 2       | success byte               |
| 3       | amount of hot fix retries  |
| 4       | amount of cold fix retries |
| 5-6     | time to fix                |
| 7-10    | latitude                   |
| 11-14   | longitude                  |
| 15-18   | altitude                   |
| 19      | fix type                   |
| 20      | SIV                        |
| 21-22   | h_acc_est                  |
| 23      | pDOP                       |
| 24-27   | fix time                   |
| 28      | active tracking            |
| 29-30   | cog                        |
| 31      | sog                        |

Standard message will be sent regardless if fix is invalid.

---

**Short message** is send on port 13 and contains the following data:

| Byte(s) | Explanation                       |
| ------- | --------------------------------- |
| 0       | message id                        |
| 1       | payload length\*                  |
| 2-5     | time of last position acquisition |
| 6-9     | latitude                          |
| 10-13   | longitude                         |
| 14-15   | horizontal accuracy               |

*Payload length does not include the space used for the first two bytes (message id and payload
length).*Example: Payload of this particular case would be 14.\*

Short message will be sent only if gps fix was successfully obtained.

---

User can set sending/storing preferences for both messages in the standard way by setting send via
LoRaWAN/send via satellite/store to flash settings flags.

In debug mode, governed by setting: "gps_sat_data" with id: 0x0A (turn this on: 0A 01 01 or turn
this off: 0A 01 00 to port 3) also additional **satellite data message** is send to port 9.

## Two intervals operation

Two-interval operation is supported. To use 2 intervals functionality, setting:
`ublox_multiple_intervals` with id: `0x29` must be turned on with command: `29 01 00` send to
port 3.

We can divide each day into two intervals: `interval1` and `interval2`. The start of each interval
is defined by `ublox_interval1_start` and `ublox_interval2_start` respectively. Supported values are
from 0 to 23, representing UTC time hours. Using these settings, the intervals are defined as:
`interval1: [ublox_interval1_start, ublox_interval2_start)` and
`interval2: [ublox_interval2_start, ublox_interval1_start)`.

> [!NOTE] Setting both intervals to the same hour will result in the selection of `interval1`.

### Example

```javascript
interval1: 15 min
interval2: 1 h
ublox_interval1_start: 7
ublox_interval2_start: 18
```

This means that at 07:00 UTC, the device will start by obtaining a GNSS fix, and repeat the
procedure every 15 minutes. At exactly 17.45, the last 15-minute fix will be made. When we switch to
`interval2` at 18:00 a fix will be made at the start and on every hour until the next day at 7.00
when we switch back to `interval1`.

The start of `interval1` can be changed with the command: `27 01 val_in_hex_format` send on port 3.

If two-interval operation mode is selected, interval of gps fix can be defined for each interval
separately. For `interval1` the default setting `ublox_send_interval` with id `0x02` is used. For
`interval2` the setting `ublox_send_interval_2` with id `0x26` is used.

## Cold and hot fix

GPS fix acquisition logic:

1. Upon gps fix acquisition start, the tracker will attempt to obtain a cold fix.

2. Duration of the cold fix is determined with setting `cold_fix_timeout` with id: `0x16`. Default
   value is 200s. To change its value send the command
   `[16 02 [uint16 val in hex format, little endian]]` to port 3.

3. During the cold fix, after `ublox_min_satellites_timer` (default `30`) seconds, a check is made
   to ensure there is a sufficient amount of satellites detected to continue fix acquisition. The
   minimum amount of satellites needed to continue the fix is controlled by `ublox_min_satellites`.
   Setting `ublox_min_satellites` to 0 will disable the feature.

4. If the cold fix is unsuccessful, the tracker will perform a new fix attempt for as many times as
   defined by setting `cold_fix_retry` with id `0x14`. Default value is 10. To change value send
   command `[14 01 [val in hex format]]` to port 3. Current attempt number is included in gps
   message on port 2.

5. After a defined number of unsuccessful cold fix attempts, gps module will turn off. To reboot gps
   module send command: `[A6 00]` to port 32. If unsuccessful, reset tracker with command: `[A1 00]`
   to port 32.

6. If a successful cold fix is obtained, the GPS module will stay turned on for a user configurable
   period of time. This can be changed by setting `ublox_leave_on` with id `0x33` representing time
   in seconds. Default value is 15s.

7. After a successful cold fix, tracker will attempt hot fix acquisition on the next interval.

8. Duration of hot fix is determined with the setting `hot_fix_timeout` with id `0x17`. Default
   value is 45s. To change the value send `[17 02 [uint16 val in hex format, little endian]]` to
   port 3.

9. During the hot fix, after `ublox_min_satellites_timer` (default `30`) seconds, a check is made to
   ensure there is a sufficient amount of satellites detected to continue fix acquisition. The
   minimum amount of satellites needed to continue the fix is controlled by `ublox_min_satellites`.
   Setting `ublox_min_satellites` to 0 will disable the feature.

10. If hot fix is unsuccessful, tracker will make new attempts for as many times as defined by the
    `hot_fix_retry` setting, with id `0x15`. Default value is 3. To change the value send the
    `[15 01 [val in hex format]]` command to port 3. Current attempt nr. is included in gps message
    on port 2.

11. After a defined number of unsuccessful hot fix attempts, tracker will again try with cold fixes.

12. Minimum fix acquisition time is defined with the user setting `ublox_min_fix_time` with id
    `0x28`, representing time in seconds. Its default value is 5s. Tracker will attempt to acquire
    position for at least `ublox_min_fix_time` seconds, even is successful fix ws obtained in
    shorter time.

### Cold fix interval

Cold fixes can be triggered on an hourly interval using the `ublox_cold_fix_hour_interval` user
setting. On interval expiration, this setting forces the next gps fix the device performs to be a
cold fix. Setting this user setting to 0 will disable this feature.

## Backoff

Back off functionality is available.

Back off factor is defined with setting `gps_backoff_factor` with id `0x25`. Value, divided by 10,
represents the scaling factor between unsuccessful cold fix attempts. So for factor 1.5, the setting
must be set to 15, i.e.: `[25 01 0F]` to port 3. To disable the backoff functionality, set value of
the coefficient to 1 (i.e. setting 10: `[25 01 0A]` command to port 3).

If two-interval operation is supported backoff will be reset on interval switch.

## Active tracking

Active tracking mode is supported. To enable active tracking, user setting `ublox_active_tracking`
with id `0x2B` must me set to true. In this mode GPS module will be turned on all the time.

## Motion triggered GPS

Motion triggered GPS mode is supported. To enable the functionality, the user setting
`enable_motion_trig_gps` with an id value of `0x2E` must be enabled. Motion threshold is checked
using the accelerometer sensor, the sensitivity of which can be configured by the `motion_ths`
setting with an id value of `0x2D`. Default value is set to 6 - refer to lis2dw12 sensor data-sheet
for an in-depth explanation.

The motion triggered GPS fix functionality is split into two modes:

### 1. (DEFAULT) Skip GPS fix if no motion was detected during GPS fix interval

The device will skip the GPS fix if no movement was detected in the duration of a
`ublox_send_interval`. If the device consecutively skips the gps fix for (user defined, default: 5)
`gps_skipped_triggered_interval` times, the device will obtain a new fix and reset the internal
skip-counter.

Setting the `gps_skipped_triggered_interval` to 0 will result in the device conducting a GPS fix on
every `ublox_send_interval`.

#### Example

```c
enable_motion_trig_gps: true,
motion_ths: 6,
ublox_send_interval: 360 s,
gps_skipped_triggered_interval: 10
```

If no motion is detected, the device will skip GPS fix acquisitions for ~3600s (approx. 10 \* 360s)
before performing a fresh fix. Detecting motion during one of the intervals will result in the
device obtaining a fix and resetting the internal skip-counter.

### 2. Motion triggered GPS in specified time window

> [!NOTE] This motion trigger functionality can be used by itself and in congruence with other GPS
> features (except for #1 Skip GPS fix), as it does not rely on the standard ublox send/fix
> intervals.

Each motion trigger event temporarily increases the internal motion trigger counter for a (user
defined, in seconds) `gps_triggered_interval` duration. When the internal trigger counter is equal
or exceeds the (user configurable, default: 5)
`gps_motion_triggered_min_num_of_triggers_per_interval` setting, a fresh GPS fix will be performed,
independent of ublox fix/send intervals.

The minimum in-between fix failsafe duration applies (Hardset to 30s). The device will still count
movement detections but a fix will not occur during the failsafe duration.

Setting `gps_triggered_interval` or `gps_motion_triggered_min_num_of_triggers_per_interval` to 0
will disable the feature and enable the #1 default GPS skip fix behavior.

#### Example

```c
enable_motion_trig_gps: true,
motion_ths: 6,
gps_triggered_interval:60 s,
gps_motion_triggered_min_num_of_triggers_per_interval: 5
```

If at least 5 motion trigger events events are detected in a 60s period, a GPS fix will be
performed.

## Manual commands

User can initiate GPS fix outside defined intervals by using commands:

**cmd_get_ublox_fix** with id `0xB8` - standard message will be send (port 2) on communication
channel command was received on.

**cmd_get_ublox_satellite_data** with id `0xB8` - a fix will be made and the data of detected ublox
GPS satellites will be sent

To reset GPS module, send the **cmd_reset_gps** command with id `0xA6`.

## Resending position message

User can enable periodic resend of the most recent valid position fix, by setting
`gps_resend_interval` with id `0x0A` that determines interval in seconds. Short GPS position message
will be sent only to LoRaWAN.
