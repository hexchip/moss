# Copyright (c) 2023 Anhui Listenai Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_GCL_COMP_RES)
  target_sources(app PRIVATE resource_load.c)
  target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif() # CONFIG_GCL_COMP_RES

if(CONFIG_APLICATION_PACK_IMAGES)
  set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated)

  generate_inc_file_for_target(
    app
    ${CMAKE_CURRENT_SOURCE_DIR}/images/venus_cp.bin
    ${gen_dir}/venus_cp.inc
  )

  generate_inc_file_for_target(
    app
    ${CMAKE_CURRENT_SOURCE_DIR}/images/fd/face_detect.bin
    ${gen_dir}/fd_face_detect.inc
  )

  generate_inc_file_for_target(
    app
    ${CMAKE_CURRENT_SOURCE_DIR}/images/fd/face_align.bin
    ${gen_dir}/fd_face_align.inc
  )

  generate_inc_file_for_target(
    app
    ${CMAKE_CURRENT_SOURCE_DIR}/images/fd/face_verify.bin
    ${gen_dir}/fd_face_verify.inc
  )

  generate_inc_file_for_target(
    app
    ${CMAKE_CURRENT_SOURCE_DIR}/images/fd/anti_fasnet.bin
    ${gen_dir}/fd_anti_spoofing.inc
  )

  generate_inc_file_for_target(
    app
    ${CMAKE_CURRENT_SOURCE_DIR}/images/fd/anti_depthnet.bin
    ${gen_dir}/fd_face_depth.inc
  )

  generate_inc_file_for_target(
    app
    ${CMAKE_CURRENT_SOURCE_DIR}/images/fd/anti_bilinear.bin
    ${gen_dir}/fd_anti_bilinear.inc
  )

  zephyr_linker_sources(SECTIONS images.ld)
  target_sources(app PRIVATE images.c)
endif() # CONFIG_APLICATION_PACK_IMAGES
