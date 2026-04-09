# Scripts

This folder contains various scripts connected with this project and various TTN, Node-RED
integrations

Contents of this folder are:

- `ttn_decoder.js` - Decoder code that is used in TTN application. Effort should be made that
  decoder on TTN is up to date with the one in scripts folder.

- `ublox_lr1110_comparison.flow` - Flow used in Node-RED. It parses TTN messages and prepares LR11XX
  and Ublox data to be injected into Influx data base.

- `settings` folder contains scripts for auto-generated settings. Find more information
  [by following this link](settings/README.md).

## Update almanac

The `get_almanac.py` script updates the currently saved almanac file
`lorawan/almanac/lr11xx_almanac.c and *.h` which is used for more efficient communication.

This script should be run when testing LoRaWAN communication.

In the update almanac folder, the almanac update script takes the environment set
`LR_ALMANAC_AUTH_TOKEN` variable, inside of which the authentication token must be saved.

If the token is not present in your environment, the script will ask you to provide it.

### Running update almanac script

To run the update almanac script using the terminal, move to the project directory from were you can
then run the following command:

```bash
python3 scripts/update_almanac/get_almanac.py
```
