# T5838 Microphone sample

This sample shows how to capture and display sound data from the t5838 microphone using a single
(mono) or dual (stereo) channels.

## User configurable settings

User configurable sampling parameters can be changed at the top of `main.c`. Acceptable values and
definitions: | Name | Acceptable values | Comment | |----|----|----| |SAMPLE_RATE |u_int [12400
50000]| Set the microphone sample rate| |SAMPLE_BIT_WIDTH |u_int [1 16]| Set the microphone sample
bit width| |TOTAL_RECORDING_DURATION_MS|u_int [10 3900]|Set the total recording duration in
milliseconds| |BLOCK_DURATION_MS|u_int [1 200]| Set the duration of a block of data milliseconds|

## Building and flashing

This sample was created and tested for the board `rangeredge_nrf52480` version:`@1.7.0`

```bash
❯ east build -b rangeredge_nrf52840@1.7.0 && east flash
```
