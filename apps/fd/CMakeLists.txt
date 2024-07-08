# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(SDK_ROOT_PATH  $ENV{ZEPHYR_BASE}/../duomotai_ap)

list(APPEND ZEPHYR_EXTRA_MODULES ${SDK_ROOT_PATH}/components)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(moss)

target_sources(app PRIVATE
  src/main.c
  src/comp_fd.c
  src/indicator.c
)

target_sources_ifdef(CONFIG_DISPLAY_DEBUG app PRIVATE
  src/display.c
  src/screen.c
)

target_link_libraries(app PRIVATE lsf_client)

add_subdirectory(resources)
