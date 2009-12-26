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

#include <stdlib.h>
#include <string.h>
#include "strq.h"

void strq_start(struct strq *strq)
	{
	strq->beg = 0;
	strq->end = 0;
	}

void strq_push(struct strq *strq, const char *s)
	{
	struct strq_entry *node =
		(struct strq_entry *)malloc(sizeof(struct strq_entry));

	node->str = malloc(strlen(s)+1);
	strcpy(node->str, s);
	node->next = 0;

	if (strq->end)
		strq->end->next = node;
	else
		strq->beg = node;

	strq->end = node;
	}

void strq_shift(struct strq *strq)
	{
	if (strq->beg)
		{
		struct strq_entry *next = strq->beg->next;
		free(strq->beg->str);
		free(strq->beg);
		strq->beg = next;
		if (!strq->beg)
			strq->end = 0;
		}
	}

int strq_find(struct strq *q, const char *s)
	{
	struct strq_entry *pos;
	for (pos = q->beg; pos; pos = pos->next)
		if (strcmp(pos->str, s) == 0)
			return 1;

	return 0;
	}

void strq_include(struct strq *q, const char *s)
	{
	if (!strq_find(q,s))
		strq_push(q,s);
	}

/*
Treat string queue as cheap associative array [key,val,key,val ...].
*/
const char *strq_assoc(struct strq *q, const char *s)
	{
	struct strq_entry *pos;
	for (pos = q->beg; pos; pos = pos->next)
		{
		if (strcmp(pos->str, s) == 0)
			{
			pos = pos->next;
			return pos ? pos->str : 0;
			}
		else
			pos = pos->next;
		}

	return 0;
	}

void strq_finish(struct strq *strq)
	{
	while (strq->beg)
		strq_shift(strq);
	}
