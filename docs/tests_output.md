# Test procedure description

This document briefly describes test procedure, available tests, input commands and expected
outputs.

## Setup

To enter test procedure, HW with provisioning FW is needed.

### Building

To build provisioning FW, open main `prj.conf` file and set the following config options:

```txt
# Low power setting - see Kconfig for config dependencies
CONFIG_LOW_POWER=y
# Debug mode, for production set n
CONFIG_DEBUG_MODE=n
# Prosivioning mode, for production set n
CONFIG_PROVISIONING_MODE=y
```

Proceed with build procedure as described in the main repo.

### Flashing

Either use built FW, as described in the previous step, and flash board directly using `west flash`
command or use latest release for relevant HW version, with `-prov` extension:

```bash
nrfjprog -f nrf52 --program rhinoedge_tracker-app-rhinoedge_nrf52840-hv1.3.0-v2.3.0-prov.hex --sectoranduicrerase --verify -r
```

### HW setup

Connect your HW UART lines - HW specific. Make sure both TX and RX lines are connected.

## Enter test mode

When HW is rebooted, following command will be outputted:

```txt
Input "TEST" to start operation test procedure:
```

to enter test procedure input `TEST`. After each command press `Enter` to send. The list of
available tests with short descriptions and keywords should appear.

```txt
AVAILABLE COMMANDS ARE:
I2C -> scan I2C devices
ACC -> test accelerometer (I2C)
MIC -> test microphone
TMP -> test temperature sensor (internal)
BAT -> get battery voltage reading
CHG -> get charging voltage reading
CHG_EN -> enable charging (enabled by default)
CHG_DIS -> disable charging (enabled by default)
CHG_STAT -> get charging status pin
FLASH -> test external flash chip
FLASH_ERASE -> external flash full erase
LORA -> test LORA communication (SPI), TX CW test
SET_LORA_KEYS -> set app keys
SET_LORA_REGION -> 1 - EU, 3 - US
SET_LORA_ADR -> ADR value 0 - 15
SET_LOCATION_TIME -> longitude latitude unix time
LORA_REBOOT -> reboot lora module
LORA_JOIN -> test lora join
WIFI_LORA -> test LORA WiFi scanning
GPS_LORA -> test LORA GPS scanning consumption and satellite SNR
GPS -> test consumption, satellite SNR and test pins 6 and 7
FIX_GPS -> test get fix, satellite SNR (this test takes a while)
BT_SCAN -> test BT scan
GET_BT_NAME -> get device advertisement name
SET_BT_NAME -> set device advertisement name
GET_FACTORY_NAME -> get device factory name
GET_MAC -> get device MAC
LOW_POWER -> low power test
SETTING -> change any device setting.
INFO -> get info on available test commands
EXIT -> exit test procedure
If nothing is selected, the procedure will timeout after approx. 60 seconds
```

To run the test enter the relevant command and extra parameters if needed. After entering the
command, device will respond with:

```txt
ACK TEST_CMD
```

if test is supported and is going to be performed, or with

```txt
NACK
```

if test is not supported.

## Tests

### I2C

Run I2C scan across all available i2c peripherals. Device will respond with number of found
addresses in the `VALUE` field, and found addresses for each `i2c` peripheral.

```txt
I2C:OK|VALUE:6!I2C0:0x19 0x60 0x99 0xe0 |I2C1:0x42 0xc2
```

### ACC

Test main accelerometer - `lis2dw12` in specific case. Device will return result if communication
with sensor was successful or not. If yes, decimal values for each axis will be displayed.

```txt
ACC:OK|VALUE:[-10.03, 0.26, -1.40]!
```

### TMP

Test internal temperature sensor. Device will return result if communication with sensor was
successful or not. If yes, decimal temperature value in degrees will be displayed under the `VALUE`
field.

```txt
TMP:OK|VALUE:24.25!
```

### MIC

Test microphone. Test will check only if microphone is initialized. It will return `OK` or `ERR`
according to that.

```txt
MIC:OK|VALUE:!
```

### BAT

Test battery measurement and voltage. Test will measure and display battery level in mV, if
successful.

```txt
BAT:OK|VALUE:3812!
```

### CHG

Test charging measurement and voltage. Test will measure and display charging level in mV, if
successful.

```txt
CHG:OK|VALUE:3812!
```

### CHG_DIS/EN

ToDo... What does this test even do?

### CHG_STAT

Measure value of charging status pin, if present on the HW. Test will return logical value of the
pin if successful.

```txt
CHG_STAT:OK|VALUE:0!
```

### FLASH

Test external flash module. Test will check if flash in initialized (VALUE field), and perform
read/write test.

```txt
FLASH:OK|VALUE:1!
```

### FLASH_ERASE

Erase external flash module. Test will try to erase flash and report status of the operation.

```txt
FLASH_ERASE:OK|VALUE:!
```

### LORA

Test lora module. Test will check if module is initialized. If not, error will be reported. if
initialized FW version will be displayed in the `VALUE` field. Additional data includes `DEV_EUI` -
8 bytes and `APP_KEY` - 16 bytes.

```txt
LORA:OK|VALUE:0x10107!DEV_EUI:00 16 C0 01 F0 04 B0 CD |APP_KEY:43 07 C9 D2 47 9D A5 4A D6 F1 5F 98 53 C1 C3 05
```

### LORA REBOOT

Reboot lora module. Test will reboot module and report success.

```txt
LORA_REBOOT:OK|VALUE:!
```

### LORA JOIN

Attempt to join the network. If module is unavailable, test will report error. Joined status is
reported in the `VALUE` field, and join attempts in `ATTEMPT` field.

```txt
LORA_JOIN:OK|VALUE:1!ATTEMPT:2
```

### GPS LORA

Attempt to obtain LoRaGPS payload. If request fails, test will report error. In all cases, `VALUE`
field remains empty. When request is successful, `SAT` field will indicate number of satellites and
`FIX` field will display obtained payload. If number of satellites is 0, second byte of payload
indicates error code: 07 - no satellites and 08 - Almanac too old.

```txt
GPS_LORA:OK|VALUE:!SAT:0|FIX:01 FE A0 47 08 16 62 12 B1 92 87 18 76 3E 62 07 BC 81 01 B4  C8 46 19 88 CD 28 C2 16 6D 07 92 37 F3 2B 3A AC 25 6D 2B
```

### WIFI LORA

Perform lora WiFi scan. Report success. `VALUE` will display number of results, while MAC addresses
and scanned rssi will be displayed as an array under `RES` field.

```txt
WIFI_LORA:OK|VALUE:6!RES:[[54:13:79:5A:21:32, -92],[54:E6:FC:F5:52:32, -81],[60:02:92:1F:45:4A, -85],[C4:AD:34:9D:09:52, -48],[68:14:01:1C:41:E6, -95],[E8:39:23:49:EB:6A, -73],]
```

### GPS

Check if Ublox GPS module is available. If yes, turn power on for 5 seconds then turn it off and
report.

```txt
GPS:OK|VALUE:!
```

### FIX GPS

Obtain GPS fix. Test will return status ok, if attempt was successful. `VALUE` field indicates if we
did get valid position or not. `TTF` represents fix time in seconds. `POS` field represents obtained
position in te form: `[lat*10000000 lon*10000000 alt*1000]`. `SIV` fieald represents number of
satellites used in the fix.

```txt
FIX_GPS:OK|VALUE:1!TTF:30|FIX:[465151007 156372697 371206]|SIV:4
```

### BT scan

Perform BT scan. Report success. `VALUE` will display number of results, while MAC addresses and
scanned rssi will be displayed as an array under `RES` field.

```txt
BT_SCAN:OK|VALUE:2!RES:[[74:69:4E:C7:FA:3A, -47],[61:27:92:A1:AD:63, -71],]
```

### GET FACTORY NAME

Test will display set factory name under the `VALUE` field. If name is not set, `��������` will be
displayed.

```txt
GET_FACTORY_NAME:OK|VALUE:sptest00!
```

### GET MAC

Test will display BT MAC under the `VALUE` field.

```txt
GET_MAC:OK|VALUE:C2:5F:08:A1:AD:4D!
```

### LOW POWER

Test low power performance of the device. After acknowledging test command, device will tur off all
systems, including UART. Hence no rsp will be given. After 100 seconds, device will reboot.

### SETTING

SETTING command can be used to write settings values in internal memory of the device. After command
is acknowledged, user must input new setting as a byte array of the format:
`[setting_id, setting_len, setting[]]` following the protocol described: ... If successful, device
will return OK status and read back new setting value of the ID.

```txt
SETTING:OK|VALUE:!ID:0E|VAL:03
```

### INFO

Display information on all available tests.

### EXIT

Exit test procedure.
