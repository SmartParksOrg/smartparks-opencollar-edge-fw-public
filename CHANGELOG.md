All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)

## [Unreleased]

## [7.3.0] - 2026-04-09

### Added

- Add workflows for publishing releases to the public SmartParks repository.
- Add support for firmware type of elephantedge and wisentedge to rangeredge hardware version 1.8.0.
- Add user controllable configuration for the ublox minimum satellite check timeout. (The time after which we check if there are sufficient satellites detected to continue the ublox fix acquisition)

### Fixed

- Fix codechecker and pre-commit (now prek) detected code errors and typos.

## [7.2.0] - 2026-03-05

### Added

- Add SDFS UART (SD card writer) module and sample.
- Add changes to main README.md file
- Add support for multiple led colors (magenta, yellow and cyan).
- Add LP0 lib, containing the packetization and communication modules.
- Add LP0 feature to main app which includes sending messages via ABP and offloading logic.

### Fixed

- Fixed device rebooting on ungraceful bluetooth disconnection (e.g. going out of range).

### Removed

- Removed remaining RF Open Sky detection user settings that were not removed in the v7.1.0 release.

## [7.1.0] - 2026-02-10

### Added

- Add external switch user configurable power control, enabling the user to turn power for the external switch on or off.
- Add LoRa TX and RX tests to provisioning firmware.
- Add flash size printout as a separate test to provisioning firmware.
- Add freeedge_nrf52840 v1.6.0 board files and support for ambiguous flash.
- Add collaredge_nrf52840 v1.5.0 board files and support for ambiguous flash.
- Add RF front end module as a stand alone module in a separate (lib) folder.

### Fixed

- Fix AirQ report-sending interval overflow.
- Fix flash read from head functionality to start read from specified message.
- Fix external switch gpio pin initialization.
- Fix unnecessary uart0 (serial-uart) toggling when sending s-band messages.

### Removed

- Remove RF scanner and it's related components (RF Open Sky Detection, EWMA partition) from codebase.

## [6.16.3] - 2025-09-23

### Fixed

- Fix insufficient collaredge s-band power output by enabling serial-uart when sending.

## [6.16.2] - 2025-09-19

### Added

- Add BMV080 air quality message on detected obstruction even though new data is not available.

## [6.16.1] - 2025-09-16

### Changed

- Change BMV080 driver to version v1.0.1

## [6.16.0] - 2025-09-16

### Added

- Add charging status logic for Collaredge boards.
- Add Air Quality module featuring BME690 and BMV080 sensor support.

## [6.15.3] - 2025-09-05

### Fixed

- Fix makefile and github actions to rename settings.json and ttn_decoder.js artifacts to include the release version.

## [6.15.2] - 2025-09-04

### Fixed

- Fix `rtt_console` sample so it builds for rtt connection by default.
- Fix makefile to rename settings.json and ttn_decoder.js artifacts to include the release version.

## [6.15.1] - 2025-08-21

### Fixed

- Fix gps spamming logs prior to fix when outdoor detection is enabled.
- Fix external switch wrong message length and contents.

## [6.15.0] - 2025-08-19

### Added

- Add support for generic external switches.
- Add cold fix interval user setting, which after expiring makes the device perform a cold fix on the next gps fix interval.

### Fixed

- Fix gps `fix_timestamp` jumping into the future.
- Fix TTN decoder's multi-byte unsigned variables being interpreted as signed, resulting in negative values on most-significant-bit flip.

## [6.14.3] - 2025-07-16

### Fixed

- Fix flash chip not suspending when out of use due to missing power management.

## [6.14.2] - 2025-07-15

### Fixed

- Fix wrong lr11xx-transceiver-update sample automatic configuration selection for boards using lr1110.

## [6.14.1] - 2025-07-09

### Fixed

- Fix wrong length of empty ublox message when sent from negative outdoor detection and add timestamp.

## [6.14.0] - 2025-07-02

### Added

- Add the `rf front end module` to `lr_send` sample.
- Add outdoor detection module.
- Add sample for reading the lis2dw12 accelerometer's temperature sensor.
- Add support for accelerometer single value reading from FIFO buffer.
- Add support for accelerometer FIFO buffer which is triggered by movement and watermark triggers.
- Add bt_scan manufacturer ID user setting documentation to bt_scan/README.md.

### Changed

- Changed user setting `flash_status_interval` which controls periodic flash consumption reports from 86400 to 0 (off by default).

## [6.13.2] - 2025-06-23

### Fixed

- Fix lora rf-sw-tx-mode rf path in Collaredge v1.1.0 and v1.4.0 overlay.

## [6.13.1] - 2025-06-16

### Fixed

- Fix RF scanning wrong compare operator, when comparing EWMA buffer size with EWMA partition size.

## [6.13.0] - 2025-06-13

### Added

- Add user configurability for accelerometer ODR (output data rate) and g force scale.
- Add 2nd iridium satellite sending interval.
- Add support for ambiguous external flash chips.
- Disable Bluetooth auto disconnect when offloading data from device.

### Changed

- Rename all occurrences of collaredgefree to freeedge throughout the codebase to avoid potential confusion with collaredge.

## [6.12.1] - 2025-03-27

### Fixed

- Fix flash message read speed for transmission over Bluetooth.

## [6.12.0] - 2025-03-26

### Added

- Add manufacturer ID user setting for bluetooth scanning.

### Changed

- Update Github CI to run release processes on docker images instead of locally running the release process.

### Fixed

- Fix flash read messages dropping due to outgoing queue overflow when requested over LoRaWAN.

## [6.11.2] - 2025-01-17

### Changed

- Change ttn_decoder's CMDQ message parsing function to include raw heart rate variability (HRV) value.

## [6.11.1] - 2025-01-16

### Changed

- Extend contents of CMDQ message to add 5 minute heart rate variability (HRV) value.

## [6.11.0] - 2025-01-08

### Added

- Add support for collaredge v1.4.0 boards.
- Add command that sends the devices current unix timestamp.

### Fixed

- Fix LoraWAN stack resetting device when RF scan is triggered while device is joining LR network.

## [6.10.0] - 2024-12-20

### Changed

- RF feature start-delay is now started with an extra delay of one cycle.
- Do not append a status message when the buffer is empty, except when CMD_SEND_SAT_BUFFER is sent via BT or LoRa.
- Prevent satellite sends on buffer overflow; only trigger sends at set intervals.
- Removed satellite_min_signal_strength logic.
- Set RF feature flag to false when there are errors during RF scanning
- Always show RF scanner as online
- Changed Rockblock satellite timeout multiplier when adding message to satellite buffer from satellite_max_messages_per_interval to satellite_retry.
- Increased Rockblock satellite module ping retry counter from 3 to 5.

### Fixed

- Fixed incorrect status message length handling when appending.
- Removed satellite_max_messages_per_interval logic; buffer constraints make this redundant.
- Corrected success state handling in satellite_enter_send_receive for mixed send/receive outcomes.
- Resolved buffer clearance issues leading to duplicate data in queued downlink scenarios.
- Limited receiving downlink messages to a maximum of 3 to prevent excessive attempts.
- Fixed initial RockBlock discovery when enabling the satellite feature from the app. Updated sys_features.satellite_com handling for correct online/offline app status.
- Put radio in standby when switching over 1500mhz threshold to get correct RF scanning results
- Javascript syntax error in RF scanner decoder
- Device returns from low power level to normal power level when charging (and reboots &lt;- TEMPORARY FIX).
- Turn off Rockblock signal strength check by default.

## [6.9.1] - 2024-11-08

### Fixed

- Fix the device disabling satellite sending until reboot after unsuccessful ping.
- Fix unused microphone and external rf scanner connector consuming power (only applicable on RangerEdge).

## [6.9.0] - 2024-10-24

### Added

- Add support for CollarEdgeFree v1.3.0.
- Add support for RangerEdge v1.8.0.
- Add sending all settings in multiple messages over Bluetooth when calling `cmd_send_all_settings`.
- Add RF scan report message suppression if `should_alert` is disabled.
- Disable Bluetooth advertising and terminate the existing connection when conducting a CMDQ scan.
- Disable LoRaWAN duty cycle limitation.

### Changed

- Update settings README.md with new available commands and other minor style changes.
- Update rf scanner code for increased scan speed.
- Disable `rf_scan_multiple_intervals` scheduling by default.
- Update main README.md with additional information and some minor style changes.

### Fixed

- Fix device not joining LoRaWAN while charging.
- Fix incorrect string comparing if statements.
- Fix RF open sky detection scan causing device crash with longer scan times on production version.
- Fix RF scan causing device crash with longer scan times on production version.
- Fix Rockblock getting stuck in endless loop when failing to send message while emptying queue.
- Fix Bluetooth advertisement data being set while advertisement is disabled.

### Removed

- Removed RF Open Sky Detection scan duration user setting.

## [6.8.1] - 2024-09-12

### Added

- Add wifi-scan support for collaredge and collaredge free.

## [6.8.0] - 2024-09-12

### Added

- Add s-band support for collaredgefree v1.0.0.
- Add support for collaredge v1.1.0.
- Add s-band support for collaredge v1.0.0.

## [6.7.2] - 2024-09-11

### Fixed

- Fix Open sky detection scan prior to gps fix default user setting not updating.

## [6.7.1] - 2024-09-11

### Fixed

- Fix gps motion trigger (Skip fix if no motion detected) timestamp log print.
- Fix minor typos in Open sky detection prior to gps fix feature.
- Fix Open sky detection scan prior to gps fix being enabled by default.

## [6.7.0] - 2024-09-06

### Added

- Add RF open sky detection scan prior to GPS fix.
- Add motion detection mode with user settable duration and threshold.
- Add fence led blink when pulse detected.

### Fixed

- Fix RF scan staying off after pause induced by BT connection.
- Fix device dropping RF scan (log) messages.
- Fix device not setting frequency of (RF scan) when reading RSSI values, by resetting LoRaWAN modem on failure.

## [6.6.1] - 2024-08-27

### Fixed

- Fix VHF bursts interrupting LoRaWAN join process.

## [6.6.0] - 2024-08-14

### Fixed

- Fix LoRaWAN join status displaying as init error on join error.

### Removed

- Removed `CONFIG_GPIO_AS_PINRESET=n` from all rhino board configuration files, needed for automatic post-flash device reset on older programming PCBs.

### Added

- Add s-band lorawan send type.
- Add VHF (very high frequency) bursts feature.
- Add `horseedge`, `fenceedge` and `scanneredge` device firmware types.
- Add decoupling command and functionality for CollarEdge.
- Add support for CollarEdge and CollarEdge Free boards.

## [6.5.3] - 2024-08-09

### Fixed

- Fix open sky detection feature turned on by default.

## [6.5.2] - 2024-08-09

### Fixed

- Fix device BT advertising while performing RF scan.

## [6.5.1] - 2024-07-30

### Fixed

- Fix lorawan reporting false join err after gps fix.

## [6.5.0] - 2024-07-26

### Added

- Add satellite minimum signal strength setting and check before sending data.
- Add periodic Ublox check (while obtaining a fix) for minimum number of available satellites.
- Add `debug` build type for debugging with a debugger.
- Add RF open sky detection functionality.
- Add support for multiple RF scan types.
- Add internal RF scanner. External RF scanner hardware is no longer supported!

### Changed

- Rename existing `debug` build type to `log` (release build with logs).

### Fixed

- Fix LoRaWAN chip becoming unresponsive due to floating `event-gpios` pin.
- Fix device not reporting LoRaWAN module and join status.
- Fix almanac update failing when updating via SmartParks app.
- Fix incorrect flash utilization percentage calculation.

### Removed

- Remove pwm (pulse width modulation) node from device-tree files.

## [6.4.1] - 2024-07-02

### Fixed

- Fix RF scanner scanning before LoRaWAN is suspended.

## [6.4.0] - 2024-07-01

### Fixed

- Fix LoRaWAN performing join during RF scan.

## [6.3.0] - 2024-06-04

### Added

- Add infinity-loop failsafe for gps fix intervals.
- Add adaptive data rate (ADR) `lr_adr_profile` user setting.

### Changed

- Disabled confirmed status messages (port 4) by switching lr_confirm_flag bit 4 off.

## [6.2.2] - 2024-04-26

### Changed

- Disable modem-e info message (port 199) from being sent at regular intervals.

### Fixed

- Device crashing after 5-7 min due to a thread priority induced race condition. Changed `lorawan_thread` priority to 6.

## [6.2.1] - 2024-04-18

### Added

- Add timeout when waiting on LoRaWAN almanac update.

### Changed

- Update SWDR001-Zephyr driver to v1.5.4.

## [6.2.0] - 2024-04-18

### Fixed

- Fix lora getting stuck in wait loop during lr11xx crypto initialization.

### Added

- Add custom port and message for GPS resend location
- Add `satellite_max_messages_per_interval` setting that allows up to X satellite messages to be sent per satellite send interval.
- Add LoRaWAN frequency region setting, so that all TTN specified frequency plans are supported.
- Add success flag to parsed cmdq messages in ttn_decoder (cmdq_success).
- Add charging voltage measurement corrections.
- Add separate GPS interval start times for the ublox_multiple_intervals feature.
- Add multiple interval customization support for RF scanning.

### Removed

- Removed v5 migration firmware and related "LoRaWAN update status" message type.

## [6.1.3] - 2024-03-06

### Changed

- Update SWDR001-Zephyr driver to 1.5.2

## [5.0.0] - 2024-03-05

### Added

- Add/update CMDQ, bt_scan, wifi_scan, mic t5838, LR-FHSS sample README.md
- Add v5 migration firmware that updates the LoRaWAN chip

### Fixed

- Fix logical error when handling the command `CMD_GET_UBLOX_FIX` via LoRaWAN. The response message is now sent correctly.

### Changed

- Update main readme to be clearer on the supported hardware and firmware types.

## [6.1.2] - 2024-2-14

### Fixed

- Fix CMDQ crashing
- Fix CMDQ empty messages being appended
- Fix CMDQ ttn_decoder typo

## [6.1.1] - 2024-02-07

### Fixed

- Fix release artifact versioning

## [6.1.0] - 2024-02-07

### Added

- Add DTS files for RangerEdge 1.7.
- Add RF front-end-module support
- Add LR satellite send sample
- LR S-band module
- Add t5838 mic support
- Add zero ble connection reports
- Add zero wifi connections found reports
- Add BT CMDQ scanning support
- Add BT CMDQ messaging support
- Add S-band rf frequency configurability

### Removed

- Remove old HW support

## [4.4.3] - 2023-11-15

### Fixed

- Ublox valid fix flag reset

## [4.4.2] - 2023-11-09

### Changed

- Remove LoRaWAN message hard limit length

## [4.4.1] - 2023-11-08

### Fixed

- Ublox short message length bug fix
- Removed altitude from Ublox short message

## [4.4.0] - 2023-11-07

### Added

- add flash status message
- add short ublox message
- add resend position functionality

### Fixed

- fix bug in gps reset command
- change lora modem implementation to allow faster message sending.
- fix double usage of IDs in commands structure

## [4.3.1] - 2023-06-23

## [4.3.0] - 2023-06-22

### Fixed

- add delay before reading charging pin after interrupt as value can jump around couple of ms, causing wrong state handling
- add delay before reading reed pin after interrupt as value can jump around couple of ms, causing wrong state handling
- add reed safety check function

## [4.2.1] - 2023-06-07

### Fixed

- Change fence threshold values.

## [4.2.0] - 2023-05-26

### Fixed

- Change fence scaling factor and display units.

## [4.1.0] - 2023-04-25

### Added

- fence support

## [4.0.2] - 2023-04-11

### Fixed

- ifdef condition for WiFi scan

## [4.0.1] - 2023-04-11

### Changed

- display uptime in days instead of hours
- WiFi scan is enabled only for Ranger HW - only HW that supports scanning
- modify ranger voltage divider
- add 200 ms sleep in voltage divider

## [4.0.0] - 2023-03-31

### Removed

- Memfault support
- Manual release script

### Added

- Satellite support
- RF scanner support
- New test procedure, using shell
- Option for RTT/serial testing
- Option for RTT/serial debug
- west.yaml file
- NCS 2.2 port
- tracker type as setting
- new release process using east

### Fixed

- Get almanac script - loracloud to V1 migration

## [2.15.0] - 2023-02-15

### Fixed

- Fix message length in BT module to avoid too long messages based on LoRaWAN limitation.

## [2.14.0] - 2023-02-14

## [2.13.0] - 2023-02-03

### Added

- Change temperature sensor sampling to avoid wtd reboots in the case of non-functioning sensor.

## [2.12.0] - 2022-10-27

### Changed

- Change default settings.

### Added

- Get setting test protocol for provisioning

## [2.11.0] - 2022-10-03

### Fixed

- Add MISO pulldown in DT definition for RangerEdge. Helps with power consumption on some boards.

## [2.10.0] - 2022-09-27

### Fixed

- Fix wraparound in time tracking without GPS time update.
- Fix wraparound bug in lr1110 driver.

## [2.9.0] - 2022-09-23

### Fixed

- Call to periodic error check function.

### Added

- New test procedure.

## [2.8.0] - 2022-07-14

### Fixed

- Flash size read.
- Flash wraparound bug fix.

## [2.7.0] - 2022-07-07

### Added

- Add CatTracker HW2.1 support.

### Fixed

- Update CatTracker HW2.0 battery measurement.

## [2.6.0] - 2022-06-28

### Fixed

- Update CatTracker HW2.0 RF switches.

## [2.5.0] - 2022-06-22

### Added

- Add CatTracker HW2.0 support.

### Fixed

- RhinoEdge voltage divider

## [2.4.0] - 2022-06-06

### Changes

- Move temperature sensor to separate folder.
- Check accelerometer error before enabling motion triggered gsp.
- Check accelerometer trigger support before enabling motion triggered gsp.
- Enable to use any valid accelerometer data in status message.
- Link accelerometer data only to accelerometer performance, not all sensors.

### Fixed

- Fix config sensor options for ElephantEdge board.
- Debug lsm6dsl sensor.

## [2.3.0] - 2022-05-17

### Changes

- Update WisentEdge, ElephantEdge and CatTracker board definitions.
- Add conditional check for reed functionality in FW for boards without reed support.
- Change lr gps fix to: Before each scan, check nr. of satellites obtained in prev scan. If 0, proceed with dummy scan, otherwise skip it. If 0 satellites are obtained in dummy scan, skip proper scan. Settings for dummy scan - GPS only (no BeiDou), max SV = 1
- Adjust nr. of satellites in lr gps scan, based on available message size
- Remove flash msg length constrain for lr gps messages - we get extra 4 bytes, due to timestamp size
- Remove Rhinoedge HW 1.4 from the release script.

[unreleased]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v7.3.0...HEAD
[7.3.0]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v7.2.0...v7.3.0
[7.2.0]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v7.2.0...v7.2.0
[7.2.0]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v7.2.0...v7.2.0
[7.2.0]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v7.1.0...v7.2.0
[7.1.0]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.16.3...v7.1.0
[6.16.3]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.16.2...v6.16.3
[6.16.2]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.16.1...v6.16.2
[6.16.1]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.16.0...v6.16.1
[6.16.0]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.15.3...v6.16.0
[6.15.3]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.15.2...v6.15.3
[6.15.2]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.15.1...v6.15.2
[6.15.1]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.15.0...v6.15.1
[6.15.0]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.14.3...v6.15.0
[6.14.3]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.14.2...v6.14.3
[6.14.2]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.14.1...v6.14.2
[6.14.1]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.14.0...v6.14.1
[6.14.0]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.13.2...v6.14.0
[6.13.2]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.13.1...v6.13.2
[6.13.1]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.13.0...v6.13.1
[6.13.0]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.12.1...v6.13.0
[6.12.1]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.12.0...v6.12.1
[6.12.0]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.11.2...v6.12.0
[6.11.2]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.11.1...v6.11.2
[6.11.1]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.11.0...v6.11.1
[6.11.0]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.10.0...v6.11.0
[6.10.0]: https://github.com/SmartParksOrg/smartparks-opencollar-edge-fw/compare/v6.9.1...v6.10.0
[6.9.1]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.9.0...v6.9.1
[6.9.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.8.1...v6.9.0
[6.8.1]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.8.0...v6.8.1
[6.8.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.7.2...v6.8.0
[6.7.2]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.7.1...v6.7.2
[6.7.1]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.7.0...v6.7.1
[6.7.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.6.1...v6.7.0
[6.6.1]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.6.0...v6.6.1
[6.6.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.5.3...v6.6.0
[6.5.3]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.5.2...v6.5.3
[6.5.2]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.5.1...v6.5.2
[6.5.1]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.5.0...v6.5.1
[6.5.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.4.1...v6.5.0
[6.4.1]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.4.0...v6.4.1
[6.4.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.3.0...v6.4.0
[6.3.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.2.2...v6.3.0
[6.2.2]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.2.1...v6.2.2
[6.2.1]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.2.0...v6.2.1
[6.2.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.1.3...v6.2.0
[6.1.3]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v5.0.0...v6.1.3
[5.0.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.1.2...v5.0.0
[6.1.2]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.1.1...v6.1.2
[6.1.1]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v6.1.0...v6.1.1
[6.1.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v4.4.3...v6.1.0
[4.4.3]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v4.4.2...v4.4.3
[4.4.2]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v4.4.1...v4.4.2
[4.4.1]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v4.4.0...v4.4.1
[4.4.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v4.3.1...v4.4.0
[4.3.1]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v4.3.0...v4.3.1
[4.3.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v4.2.1...v4.3.0
[4.2.1]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v4.2.0...v4.2.1
[4.2.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v4.1.0...v4.2.0
[4.1.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v4.0.2...v4.1.0
[4.0.2]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v4.0.1...v4.0.2
[4.0.1]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v4.0.0...v4.0.1
[4.0.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v2.15.0...v4.0.0
[2.15.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v2.14.0...v2.15.0
[2.14.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v2.13.0...v2.14.0
[2.13.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v2.12.0...v2.13.0
[2.12.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v2.11.0...v2.12.0
[2.11.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v2.10.0...v2.11.0
[2.10.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v2.9.0...v2.10.0
[2.9.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v2.8.0...v2.9.0
[2.8.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v2.7.0...v2.8.0
[2.7.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v2.6.0...v2.7.0
[2.6.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v2.5.0...v2.6.0
[2.5.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v2.4.0...v2.5.0
[2.4.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/v2.3.0...v2.4.0
[2.3.0]: https://github.com/IRNAS/smartparks-opencollar-edge-fw/compare/aaa2fea25e34c5bbd29b7c3ef1163ef8f0dbca82...v2.3.0
