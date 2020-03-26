// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * Copyright (C) 2020  Alexey Gladkov <gladkov.alexey@gmail.com>
 */
#include <stdio.h>
#include <stdarg.h>

#include "logging.h"

void
message(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}
