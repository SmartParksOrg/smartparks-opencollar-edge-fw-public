# LR11xx Almanac update

## Description

Wrapper for Almanac update. Update function, when called, prints current almanac age and available
update age. If update is more recent, almanac is updated.

## Almanacs

Almanac is obtained from server and stored into `lr11xx_almanac.c` file by calling `get_almanac.py`
[script](../../../../scripts/update_almanac/get_almanac.py) from the [scripts](../../../../scripts)
folder. Run script before attempting almanac update.
