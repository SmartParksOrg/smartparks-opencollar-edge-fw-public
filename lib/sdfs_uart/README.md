# AK-SDFS-UART communication lib

This module contains the fprot.h library for the AK-SDFS-UART external sd card module, used for
writing to a external SD card.

## Enabling the module

To enable the module, add the below child node to your DTS / overlay files and enable the
configuration in your configuration files.

### DTS

Add the following child node to your preferred UART node inside of your `device tree` / `overlay`
files:

```dts
&uart0 {
    status = "okay";
    compatible = "nordic,nrf-uarte";
    current-speed = <9600>;
    pinctrl-0 = <&uart0_default>;
    pinctrl-1 = <&uart0_sleep>;
    pinctrl-names = "default", "sleep";

    /* AK-SDFS-UART (SD card reader) */
    sdfs: sdfs {
        status = "okay";
        compatible = "irnas,ak-sdfs-uart";
        reset-gpios = <&gpio0 5 GPIO_ACTIVE_LOW>;
    };
};
```

> [!NOTE] The AK-SDFS-UART module allows for baud rates ranging from 9600 to 115200 (factory
> default: 9600). The sdfs_uart_handler module uses a baud rate of 9600, so there is no special
> configuration needed when using the module "out of the box". Support for selectable baud rates
> will be added as needed in the future.

### Configuration

Enable the module by setting the `CONFIG_SDFS_UART_MODULE=y`.

> [!IMPORTANT] If using the automatic initialization method, the `CONFIG_UART_ASYNC_API` must not be
> enabled! It is however possible to use uart async if you decide on using the manual initialization
> method.

## SD card

The module supports only micro SD cards.

### SD card file system

The module supports SD cards with FAT16 and FAT32 file systems.

### SD card removal

> [!WARNING] If the SD card is removed during the writing process, the file that was written can
> become corrupt. Wait for the writing process to finish prior to removing the card! Writing can
> also be paused by using the `int sdfs_uart_pause_write(k_timeout_t delay)` function, during which
> you can safely remove the SD card.
