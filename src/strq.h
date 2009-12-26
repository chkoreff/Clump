/*  strq - simple queue of string values and cheap associative array
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

struct strq_entry
	{
	struct strq_entry *next;
	char *str;
	};

struct strq
	{
	struct strq_entry *beg;
	struct strq_entry *end;
	};

extern void strq_start(struct strq *strq);
extern void strq_finish(struct strq *strq);
extern void strq_push(struct strq *strq, const char *s);
extern void strq_shift(struct strq *strq);
extern int strq_find(struct strq *q, const char *s);
extern void strq_include(struct strq *q, const char *s);
extern const char *strq_assoc(struct strq *q, const char *s);
