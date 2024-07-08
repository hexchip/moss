/*
 * Copyright (c) 2023 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef CONFIG_CAPABILITY_FD
int comp_fd_init(void);
int comp_fd_capture(void);
int comp_fd_compare(void);
#else
static inline int comp_fd_init(void)
{
	return 0;
}

static inline int comp_fd_capture(void)
{
	return 0;
}

static inline int comp_fd_compare(void)
{
	return 0;
}
#endif
