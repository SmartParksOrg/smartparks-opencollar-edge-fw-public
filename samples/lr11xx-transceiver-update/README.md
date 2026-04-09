# LR1110 Firmware update sample

This will update the lr1110 to the latest transceiver firmware. This sample is a copy of the
[lr1110_firmware_update](https://github.com/IRNAS/SWDR001-Zephyr/tree/v1.4.0/samples/firmware_update)
sample from [SWDR001-Zephyr](https://github.com/IRNAS/SWDR001-Zephyr) with small modifications to
print the chipEUI.

## Building and flashing

```bash
east build -b board_name
east flash
```

## Output

Sample output on successful update:

```text
*** Booting Zephyr OS build v3.2.99-ncs1 ***
[00:00:00.590,393] <inf> lr11xx_board: Event trigger set in global thread.
[00:00:00.605,041] <inf> lr11xx_common: Reset system
[00:00:00.843,536] <inf> lr11xx_common: LR init errors: 0
[00:00:00.843,780] <inf> main: Updating LR1110 to transceiver firmware 0x0307
OK: LR11XX firmware was updated successfully!
ChipEUI: 00 16 C0 01 F0 00 43 2A
```

Sample output on failure:

```text
[00:00:00.610,076] <inf> lr11xx_common: Reset system
[00:00:00.848,571] <inf> lr11xx_common: LR init errors: 0
[00:00:00.848,815] <inf> main: Updating LR1110 to transceiver firmware 0x0307
ERR: LR11XX did not enter bootloader mode!
```
