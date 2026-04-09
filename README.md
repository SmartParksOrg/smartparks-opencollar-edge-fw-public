<!-- markdownlint-disable MD041 -->
<img src="./pics/smartparks-logo.png" height="250" alt="SmartParks logo">

# Smart Parks OpenCollar Edge Firmware

This repository contains Smart Parks OpenCollar firmware for the Smart Parks OpenCollar trackers and
sensors.

<br>

Developed by Smart Parks and IRNAS.

<https://www.irnas.eu>

<img src="./pics/irnas-logo.png" height="25" alt="IRNAS logo">

---

## Overview

- Firmware documentation is located in the GitBook archive:
  <https://github.com/IRNAS/gitbook-opencollar/tree/main>

- Firmware is compatible with NCS 2.2.

## Requirements

- One of the following development boards:

  - |opencollar-edge (multiple versions in development, see the whole list below)|

## Repository folder structure

- `app` - application code for project
- `boards` - boards specific files (overlay, config, etc.)
- `docs` - useful documentation
- `drivers` - driver implementation for specific components
- `dts` - bindings
- `lib` - internal libraries
- `patches` - patch files
- `pics` - repository pictures
- `samples` - folder contains all samples used for debug purposes. Note that not all projects are
  supported for each HW.
- `scripts` - scripts used for building and various TTN, Node-RED integrations
- `tests` - specific feature tests
- `zephyr` - basics project path defines
- `CHANGELOG.md` - changes tracking
- `README.md` -basic project information
- `west.yaml` - manifest file
- `east.yaml` - east build and release instructions

## Setup

If not already set up, install west and other required tools. Follow the
[steps](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_installing.html) from
[Install the required tools](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_installing.html#install-the-required-tools)
up to (including)
[Install west](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_installing.html#install-west).

Also install [east](https://github.com/IRNAS/irnas-east-software).

Then follow these steps:

```bash
east init -m https://github.com/IRNAS/smartparks-opencollar-edge-fw smartparks-opencollar-edge-fw
cd smartparks-opencollar-edge-fw/
# Set up east globally (this only needs to be done once on each machine)
east install nrfutil-toolchain-manager
# Install toolchain for the version of NCS used in this project
east install toolchain
# Run `west update` via east to set up west modules in the repository
east update
# All of NCS is now cloned into this directory as well as this repository
# into the smartparks-opencollar-edge-fw folder.
```

## Building and flashing

Navigate to the `app/` folder prior to running the build command:

```bash
cd app
```

`east` environment is used for building and flashing.

### SETTINGS

Prior to build, configure desired settings:

1. Update desired settings in the `scripts\settings\settings.json` file.
2. Call script `py2h.py`
   - if error is displayed, you need to check what is wrong
   - if it went ok, script prints all supported ports
3. If desired, obtain new almanac by calling `get_almanac.py` (optional)

### BUILD COMMAND

```bash
east build -b <board>@<revision_group>
```

> [!NOTE] When building, instead of providing the hardware revision marked on the board, use the
> revision group as marked below. In case of doubt check the provided example.

#### Supported HW boards and their FW revisions

| Hardware name          | Hardware revision (marked on board) | Hardware revision group |
| ---------------------- | ----------------------------------- | ----------------------- |
| `collaredge_nrf52840`  | `1.0.0`                             | `1.0.0`                 |
|                        | [`1.1.0` - `1.3.0`]                 | `1.1.0`                 |
|                        | [`1.4.0` - `1.7.0`]                 | `1.4.0`                 |
|                        |                                     |                         |
| `freeedge_nrf52840`    | [`1.0.0` - `1.2.0`]                 | `1.0.0`                 |
|                        | [`1.3.0` - `1.5.0`]                 | `1.3.0`                 |
|                        | `1.6.0`                             | `1.6.0`                 |
|                        |                                     |                         |
| `rangeredge_nrf52840`  | [`1.4.0` - `1.5.0`]                 | `1.4.0`                 |
|                        | `1.6.0`                             | `1.6.0`                 |
|                        | `1.7.0`                             | `1.7.0`                 |
|                        | [`1.8.0` - `1.13.0`]                | `1.8.0`                 |
|                        |                                     |                         |
| `rhinoedge_nrf52840`   | [`1.4.0` - `1.6.0`]                 | `1.4.0`                 |
|                        |                                     |                         |
| `rhinopuck_nrf52840`   | `1.3.0`                             | `1.3.0`                 |
|                        |                                     |                         |
| `rhinopuck35_nrf52840` | `1.2.0`                             | `1.2.0`                 |
|                        |                                     |                         |

> [!NOTE] There are also special firmware types that add support for special functionalities that
> are not available in the firmware by default. More information regarding special firmware types
> can be found below.

##### Example

- Board type: Rangeredge
- Hardware revision: 1.11.0
- Hardware revision group: 1.8.0
- Build command:

```bash
east build -b rangeredge_nrf52840@1.8.0
```

#### Build types

Above command will build standard production FW. Other options are available:

- **LOG VERSION:** In `conf/log_rtt.conf` RTT and debug logging options are enabled. Use command:

  ```bash
    east build -b rangeredge_nrf52840@1.4.0 -u log
  ```

- **DEBUG VERSION:** In `conf/debug.conf` debugging options are enabled and in
  `conf/disable_mcuboot.conf` mcuboot is disabled due to size constraints. Use command:

  ```bash
    east build -b rangeredge_nrf52840@1.4.0 -u debug
  ```

  For debugging with GDB (GNU debugger) you can use the `east debug` command.

- **PROVISIONING VERSION:** In `conf/prov.conf` provisioning configs are defined. Use command:

  ```bash
    east build -b rangeredge_nrf52840@1.4.0 -u prov
  ```

### FLASH COMMAND

```bash
  east flash
```

Tracker will flash all LEDs when it boots, older versions will blink red 3 times or blink RGB if
supported.

### CLEAN COMMAND

Clean build folder with

```bash
  east clean
```

### Logs

Logs can be monitored via RTT. Build log FW as described above. Monitor output via your own RTT
viewer, or alternatively, create a RTT server with JLinkExe using

```bash
  east util connect
```

and use

```bash
  east util rtt
```

to run a RTT client and connect it to the running RTT server.

## Creating a release

To create a release, navigate to the Smart Parks GitHub
[repository](https://github.com/IRNAS/smartparks-opencollar-edge-fw). Through the `actions` tab
draft a new release process on the desired branch (default: `main`) with the desired release tag.

For each supported HW version, four FW types will be built: `production`, `debug`, `log` and
`provisioning`. Binaries will be added as a new release automatically after successfully finishing
the release process. In addition `lr11xx transceiver update` sample will be built for each HW type.

### Creating release binaries locally

Binaries can also be created locally by running:

```bash
  east release
```

Binaries will be saved in the `release` folder.

## Board initial testing

Blinky example can be used to test all custom boards, if device tree has been properly configured.

Located in folder: `samples\blinky`.

## Provisioning

Provisioning is done using test rack and script provided in separate git project.

### Provisioning without rack

Manual provisioning protocol:

- Get latest release for desired board, provisioning version. Below is an example for rhinoedge HW
  v1.3 FW v2.3.
- Flash and update LR module with
  `rhinoedge_tracker-lrmdm-rhinoedge_nrf52840-hv1.3.0-v2.3.0-prov.hex`:

```bash
nrfjprog -f nrf52 --program rhinoedge_tracker-lrmdm-rhinoedge_nrf52840-hv1.3.0-v2.3.0-prov.hex  --sectoranduicrerase --verify -r
```

Wait for valid device EUI to be printed out. If update fails try running reboot command:

```bash
nrfjprog -r
```

- Write device name into UICR with command:

```bash
nrfjprog --family NRF52 --memwr 0x10001084 --val 0x53503031 (for SP01, for other values just change last digit)
nrfjprog --family NRF52 --memwr 0x10001088 --val 0x30303030 (for 0000, for other values covert them to hex first)
```

- Flash provisioning FW and run tests. More on testing functions can be found in
  src/operation_test/README.md. By defoult for FW versions 4.0.0 and above, RTT viewer is used for
  debug and provisioning, unless specific FW with serial debug is build.

Alternatively you can also flash board with debug FW and observe its operation:

```bash
nrfjprog -f nrf52 --program rhinoedge_tracker-app-rhinoedge_nrf52840-hv1.3.0-v2.3.0-dbg.hex --sectoranduicrerase --verify -r
```

- Write down the device EUI in excel spreadsheet next to generated appKey and device name.
- Flash production FW and check power consumption.

```bash
nrfjprog -f nrf52 --program rhinoedge_tracker-app-rhinoedge_nrf52840-hv1.3.0-v2.3.0.hex --sectoranduicrerase --verify -r
```

- If needed add tracker to the TTN network.
- Log into the SmartParks app and change device name and LR AppKey to match those in Excel Sheet.

## Tracker type

In addition to HW type, FW type or tracker type can be set for a specific device. Tracker type is
user settable via setting: `tracker_type` with ID: `0x00`. Refer to settings guidelines on how to
change settings: <https://app.gitbook.com/@irnas/s/opencollar/technology/firmware>.

Possibles values are:

- HW default

  - `default_tracker` = 0,
  - `rhinoedge_tracker` = 1,
  - `rangeredge_tracker` = 5,
  - `rhinopuck_tracker` = 6,
  - `collaredge_tracker` = 8,
  - `freeedge_tracker` = 9,

- FW selectable

  - `elephantedge_tracker` = 2,
  - `wisentedge_tracker` = 3,
  - `cattracker_tracker` = 4,
  - `scanneredge_tracker` = 7,
  - `fenceedge_tracker` = 10,
  - `horseedge_tracker` = 11,
  - `collaredgepico_tracker` = 12,
  - `collaredgenano_tracker` = 13,
  - `baboonedge_tracker` = 14,
  - `pangolinedge_tracker` = 15

By default, `default_tracker` option will be set, assigning logical tracker type, following from HW
type. For some HW types and revisions, multiple options are possible. Below is the list of valid
combinations:

| Hardware name          | Hardware grouping                  | Selectable FW type                                                                                                                            |
| ---------------------- | ---------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| `rhinoedge_nrf52840`   | `1.4.0`                            | `rhinoedge_tracker`, `horseedge_tracker`, `baboonedge_tracker`, `pangolinedge_tracker`, `collaredgepico_tracker` and `collaredgenano_tracker` |
| `rangeredge_nrf52840`  | `1.4.0`, `1.6.0`, `1.7.0`, `1.8.0` | `rangeredge_tracker`, `fenceedge_tracker`, `scanneredge_tracker`, `elephantedge_tracker` and `wisentedge_tracker`                             |
| `rhinopuck_nrf52840`   | `1.3.0`                            | `rhinopuck_tracker`                                                                                                                           |
| `rhinopuck35_nrf52840` | `1.2.0`                            | `rhinopuck_tracker`                                                                                                                           |
| `collaredge_nrf52840`  | `1.0.0`, `1.1.0`, `1.4.0`          | `collaredge_tracker`                                                                                                                          |
| `freeedge_nrf52840`    | `1.0.0`, `1.3.0`, `1.6.0`          | `freeedge_tracker`                                                                                                                            |

Main user-observable difference between tracker types will be displayed image in the BT app.

## Special firmware types

There are special firmware types available that serve specific use cases for specific boards.

List of all available special firmware types:

| Special firmware type      | Hardware revisions   | Hardware grouping                  | Notes                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| -------------------------- | -------------------- | ---------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `rangeredge_airq_nrf52840` | [`1.4.0` - `1.13.0`] | `1.4.0`, `1.6.0`, `1.7.0`, `1.8.0` | This firmware type allows the user to connect the `firewatch-hardware` extension board into the I²C port of the device and use the air-quality features. Building this firmware type has it's own caveats, so please check [air quality README](app/src/sensors/air_quality/README.md), [BMV080 sensor README](../irnas/irnas-bmv080-driver/README.md) and [BME690 sensor README](../irnas/irnas-bme690-driver/README.md) for more information |
