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

struct bufd
	{
	char *beg;
	char *end;
	char *pos;
	};

extern void bufd_start(struct bufd *buf);
extern void bufd_finish(struct bufd *buf);
extern void bufd_clear(struct bufd *buf);
extern int bufd_len(struct bufd *buf);
extern void bufd_need(struct bufd *buf, int len);
extern void bufd_put(struct bufd *buf, const char *s);
extern void bufd_putc(struct bufd *buf, char ch);
extern void bufd_fixlen(struct bufd *buf);
extern void bufd_clip(struct bufd *buf, int n);
