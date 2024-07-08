# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0
#
# This CMake file is picked by the Zephyr build system because it is defined
# as the module CMake entry point (see zephyr/module.yml).

# csk did not expose the header file 'display/display_csk6_lcd.h',
# so can only use this alternatives
zephyr_include_directories(.sdk/csk/drivers/display)

add_subdirectory(drivers)