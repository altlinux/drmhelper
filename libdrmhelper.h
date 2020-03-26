// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * Copyright (C) 2020  Alexey Gladkov <gladkov.alexey@gmail.com>
 */
#ifndef _LIBDRMHELPER_H_
#define _LIBDRMHELPER_H_

#ifdef __cplusplus
extern  "C" {
#endif

int drmhelper_open(const char *path);
int drmhelper_set_master(int fd);
int drmhelper_drop_master(int fd);
int drmhelper_have_permissions(void);

#ifdef __cplusplus
}
#endif

#endif /* _LIBDRMHELPER_H_ */
