/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <string.h>

#include "../slstatus.h"
#include "../util.h"

const char *
separator(const char *sep)
{
	return sep ? sep : " | ";
}
