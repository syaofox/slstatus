/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <string.h>

#include "../slstatus.h"
#include "../util.h"

static const char *cached = NULL;

const char *
separator(const char *sep)
{
	if (!cached)
		cached = sep ? sep : " | ";

	return cached;
}
