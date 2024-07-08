/*
 * Copyright (c) 2023 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <listenai/app-res.h>

APP_RES_DECLARE(venus_cp) = {
#include "venus_cp.inc"
};

#if defined(CONFIG_CAPABILITY_FD)
APP_RES_DECLARE(fd_face_detect) = {
#include "fd_face_detect.inc"
};

APP_RES_DECLARE(fd_face_align) = {
#include "fd_face_align.inc"
};

APP_RES_DECLARE(fd_face_verify) = {
#include "fd_face_verify.inc"
};

APP_RES_DECLARE(fd_anti_spoofing) = {
#include "fd_anti_spoofing.inc"
};

APP_RES_DECLARE(fd_face_depth) = {
#include "fd_face_depth.inc"
};

APP_RES_DECLARE(fd_anti_bilinear) = {
#include "fd_anti_bilinear.inc"
};
#endif /* CONFIG_CAPABILITY_FD */
