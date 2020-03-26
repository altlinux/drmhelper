// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * Copyright (C) 2020  Alexey Gladkov <gladkov.alexey@gmail.com>
 */
#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <stdlib.h>

void message(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define fatal(format, arg...) \
	do { \
		message("%s(%d): " format, __FILE__, __LINE__, ##arg); \
		exit(EXIT_FAILURE); \
	} while (0)

#define err(format, arg...) \
	do { \
		message("%s(%d): " format, __FILE__, __LINE__, ##arg); \
	} while (0)

#endif /* _LOGGING_H_ */
