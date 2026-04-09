# Operation test

Module contains shell tests for SmartParks OpenCollar firmware, that are compatible with all custom
boards, defined in the boards folder. Only test compatible with selected hardware will be available.
Shell test can be accessed via serial interface or RTT, depending on selected config.

To enable serial shell test functionality, select

```txt
CONFIG_OPERATION_TEST_SERIAL=y
```

and to enable RTT shell test functionality, select

```txt
CONFIG_OPERATION_TEST_RTT=y
```

prior to building firmware in the `app/prj.conf` file. A choice between the two must be made - both
cannot be selected at the same time. By default non is selected.

Manual testing and compatible testing script are available. For the automated test script, refer to
the `scripts\testing` folder. For manual testing, flash the relevant hardware, open terminal and
type "help" to get information on all available tests.

To run each test, type keyword, displayed by the INFO print and wait for the output. After entering
the command, device will respond with:

```txt
ACK TEST_CMD
```

if test is supported and is going to be performed, or with

```txt
NACK
```

if test is not supported.

## User instructions for RTT console

Use RTT :

```txt
JLinkRTTClient
```

Run command:

```txt
JLinkExe -device NRF52840_XXAA -if SWD -speed 4000 -autoconnect 1 -usb 801034688
```

then use `JLinkRTTClient` interface to manually run commands.

## Tests

### I2C_SCAN

Run I2C scan across all available i2c peripherals. Device will respond with number of found
addresses in the `VALUE` field, and found addresses for each `i2c` peripheral.

```txt
I2C:OK|VALUE:6!I2C0:0x19 0x60 0x99 0xe0 |I2C1:0x42 0xc2
```

### ACC

Test the main accelerometer - `lis2dw12`. Device will return if communication with sensor was
successful or not. If yes, decimal values for each axis will be displayed.

```txt
ACC:OK|VALUE:[-10.03, 0.26, -1.40]!
```

if communication with sensor cannot be established, test will return:

```txt
ACC:ERR|VALUE:!
```

### TMP

Test internal temperature sensor. Device will return if communication with sensor was successful or
not. If yes, decimal temperature value in degrees will be displayed under the `VALUE` field.

```txt
TMP:OK|VALUE:24.25!
```

if communication with sensor cannot be established, test will return:

```txt
TMP:ERR|VALUE:!
```

### BAT

Test battery measurement and voltage. Test will measure and display battery level in mV, if
successful.

```txt
BAT:OK|VALUE:3812!
```

if measurement fails, test will return:

```txt
BAT:ERR|VALUE:!
```

### CHG

Test charging measurement and voltage. Test will measure and display charging level in mV, if
successful.

```txt
CHG:OK|VALUE:3812!
```

if measurement fails, test will return:

```txt
CHG:ERR|VALUE:!
```

### FLASH

Test external flash module. Test will check if flash in initialized (VALUE field), and perform
read/write test. If read/write test is successful, test will return:

```txt
FLASH:OK|VALUE:1!
```

If flash is initialized but read/write test failed, test will return:

```txt
FLASH:ERR|VALUE:1!
```

If flash is not initialized, test will return:

```txt
FLASH:ERR|VALUE:0!
```

### FLASH_ERASE

Erase external flash module. Test will try to erase flash and report status of the operation. Output
if successful:

```txt
FLASH_ERASE:OK|VALUE:!
```

or if erase failed:

```txt
FLASH_ERASE:ERR|VALUE:!
```

### LORA

Test lora module. Test will check if module is initialized. If not, error will be reported. If
initialized, FW version will be displayed in the `VALUE` field. For boards with `lr1120` FW version
0x0101 should be obtained and for boards with `lr1110` FW version 0x0307. Additional data includes
`DEV_EUI` - 8 bytes and `APP_KEY` - 16 bytes. Success output:

```txt
LORA:OK|VALUE:0x0101!DEV_EUI:00 16 C0 01 F0 04 B0 CD |APP_KEY:43 07 C9 D2 47 9D A5 4A D6 F1 5F 98 53 C1 C3 05
```

Module not available output:

```txt
LORA:ERR|VALUE:!DEV_EUI:|APP_KEY:
```

### LORA_REBOOT

Reboot lora module. Test will reboot module and report success:

```txt
LORA_REBOOT:OK|VALUE:!
```

or fail:

```txt
LORA_REBOOT:ERR|VALUE:!
```

### LORA_JOIN

Attempt to join the network. If module is unavailable, test will report error. Joined status is
reported in the `VALUE` field, and join attempts in `ATTEMPT` field. For example, if Lora join is
successful in second attempt, test will output:

```txt
LORA_JOIN:OK|VALUE:1!ATTEMPT:2
```

If, for example, Lora join is not successful and two attempts were made, test will output:

```txt
LORA_JOIN:OK|VALUE:0!ATTEMPT:2
```

Module not available output:

```txt
LORA_JOIN:ERR|VALUE:0!ATTEMPT:
```

### WIFI_LORA

Perform lora WiFi scan. Report success. `VALUE` will display number of results, while MAC addresses
and scanned rssi will be displayed as an array under `RES` field. If, for example, 6 scan results
are found:

```txt
WIFI_LORA:OK|VALUE:6!RES:[[54:13:79:5A:21:32, -92],[54:E6:FC:F5:52:32, -81],[60:02:92:1F:45:4A, -85],[C4:AD:34:9D:09:52, -48],[68:14:01:1C:41:E6, -95],[E8:39:23:49:EB:6A, -73],]
```

If module is available, but no results are found:

```txt
WIFI_LORA:OK|VALUE:0!RES:
```

Module not available output:

```txt
WIFI_LORA:ERR|VALUE:!RES:
```

### BT_SCAN

Perform BT scan. Report success. `VALUE` will display number of results, while MAC addresses and
scanned rssi will be displayed as an array under `RES` field. If, for example, 2 scan results are
found:

```txt
BT_SCAN:OK|VALUE:2!RES:[[74:69:4E:C7:FA:3A, -47],[61:27:92:A1:AD:63, -71],]
```

If module is available, but no results are found:

```txt
BT_SCAN:OK|VALUE:0!RES:
```

Scan failed:

```txt
BT_SCAN:ERR|VALUE:!RES:
```

### GET_FACTORY_NAME

Test will display set factory name under the `VALUE` field. If name is not set, `��������` will be
displayed.

```txt
GET_FACTORY_NAME:OK|VALUE:sp123456!
```

### GET_MAC

Test will display BT MAC under the `VALUE` field.

```txt
GET_MAC:OK|VALUE:C2:5F:08:A1:AD:4D!
```

### LED

Test led diodes. Keep in mind not all HW types have support for all LED colors. Refer to board
definitions. To run LED test add relevant color tho the command, for example, enter:

```txt
LED R
```

to test Red led. Test will turn the led on for 1 second and then turn it off. If desired color is
available, test will return:

```txt
LED:OK|VALUE:!
```

if led is not available, test will return:

```txt
LED:ERR|VALUE:!
```

### LOW_POWER

Test low power performance of the device. After acknowledging test command, device will tur off all
systems, including UART. Hence no rsp will be given. After 100 seconds, device will reboot.

Note that if using RTT, you must send Exit command to reach low power state! Just closing RTT viewer
will not result in power consumption drop.

### Important - low power test testing

At the moment low power test is not working, regardless if shell via serial or rtt interface is
selected. We are testing how to turn serial interface off, as our standard approach deoesn't work
for some reason.

### SETTING

SETTING command can be used to write settings values in internal memory of the device. Protocol has
not yet been determined and supported.

### INFO

Display information on all available tests.
