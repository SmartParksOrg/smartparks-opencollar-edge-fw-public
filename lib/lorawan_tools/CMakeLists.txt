zephyr_library()
zephyr_include_directories(. ./lorawan_tools_common)

zephyr_library_sources(lorawan_tools.c Encrypt_V20.c AES-128_V10.c)
