/*
Copyright 2006 Aiko Barz

This file is part of torrentkino.

torrentkino is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

torrentkino is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with torrentkino.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <netinet/in.h>

#include "../shr/config.h"
#include "../shr/str.h"
#include "../shr/list.h"
#include "../shr/file.h"
#include "hex.h"
#include "conf.h"
#include "../shr/malloc.h"
#include "../shr/log.h"

void hex_hash_encode(char *out, const UCHAR * in)
{
	UCHAR *p0 = (UCHAR *) in;
	char *p1 = out;
	long int i = 0;

	memset(out, '\0', HEX_LEN);

	for (i = 0; i < SHA1_SIZE; i++) {
		snprintf(p1, 3, "%02x", *p0);
		p0++;
		p1 += 2;
	}
}
