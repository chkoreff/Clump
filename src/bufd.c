/*  bufd - simple dynamic char buffer for safe string operations
 *  Copyright (C) 2003  Patrick Chkoreff
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <string.h>
#include "bufd.h"

#define BUFD_BYTES_PER_BLOCK 32

void bufd_start(struct bufd *buf)
	{
	buf->beg = 0;
	buf->end = 0;
	buf->pos = 0;
	}

void bufd_finish(struct bufd *buf)
	{
	if (buf->beg)
		{
		free(buf->beg);
		bufd_start(buf);
		}
	}

void bufd_clear(struct bufd *buf)
	{
	if (buf->beg)
		{
		buf->pos = buf->beg;
		*buf->pos = '\0';
		}
	}

int bufd_len(struct bufd *buf)
	{
	return buf->pos - buf->beg;
	}

void bufd_need(struct bufd *buf, int len)
	{
	if (buf->beg)
		{
		int room = buf->end - buf->pos;
		if (room < len)
			{
			int size = buf->end - buf->beg + 1;
			int grow = len - room;
			int blocks = ((grow-1) / BUFD_BYTES_PER_BLOCK) + 1;
			char *mem;

			grow = BUFD_BYTES_PER_BLOCK * blocks;
			size += grow;

			mem = malloc(size);
			strcpy(mem, buf->beg);
			buf->pos = mem + (buf->pos - buf->beg);
			buf->end = mem + size - 1;
			free(buf->beg);
			buf->beg = mem;
			}
		}
	else
		{
		int size = len + 1;
		buf->beg = malloc(size);
		buf->end = buf->beg + size - 1;
		buf->pos = buf->beg;
		*buf->pos = '\0';
		}
	}

void bufd_put(struct bufd *buf, const char *s)
	{
	int len = strlen(s);
	bufd_need(buf, len);
	strcpy(buf->pos, s);
	buf->pos += len;
	}

void bufd_putc(struct bufd *buf, char ch)
	{
	bufd_need(buf, 1);
	*buf->pos++ = ch;
	*buf->pos = '\0';
	}

void bufd_fixlen(struct bufd *buf)
	{
	if (buf->beg)
		buf->pos = buf->beg + strlen(buf->beg);
	}

void bufd_clip(struct bufd *buf, int n)
	{
	if (buf->beg)
		{
		int len = buf->pos - buf->beg;
		len = (len >= n ? len - n : 0);
		buf->pos = buf->beg + len;
		*buf->pos = '\0';
		}
	}
