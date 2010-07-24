/*  clump - program that builds C programs intelligently
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"
#include "bufd.h"
#include "strq.h"
#include "clump.h"

#if USE_WINDOWS
#include <windows.h>
#include "getopt.h"
#else
#if USE_GETOPT_H
#include <getopt.h>
#endif
#include <dirent.h>
#include <unistd.h>
#endif

#include "dir.h"

#define VERSION "06"

#if USE_WINDOWS
const char dir_separator = '\\';
#else
const char dir_separator = '/';
#endif

struct clist_entry
	{
	struct clist_entry *next;
	struct bufd cfile;
	struct bufd ofile;
	struct bufd efile;
	struct strq sys_include;
	struct strq loc_include;
	struct strq link_objects;
	struct strq libs;
	int is_main;
	int need_compile;
	int need_link;
	};

struct clist
	{
	struct clist_entry *beg;
	struct clist_entry *end;
	};

static FILE *fp = 0;
static int ch = 0;
static struct bufd out;
struct strq syshdr;
struct clist clist;
struct dir_iterator iterator;

static int opt_analyze = 0;
static int opt_build = 0;
static int opt_showcmd = 0;
static int opt_force = 0;
static int opt_clean = 0;
static struct bufd opt_objdir;
static struct bufd opt_bindir;
static struct bufd opt_compile;
static struct bufd opt_link;

void clist_start(struct clist *clist)
	{
	clist->beg = 0;
	clist->end = 0;
	}

struct clist_entry *clist_push(struct clist *clist)
	{
	struct clist_entry *entry =
		(struct clist_entry *)malloc(sizeof(struct clist_entry));
	entry->next = 0;

	entry->is_main = 0;
	bufd_start(&entry->cfile);
	strq_start(&entry->sys_include);
	strq_start(&entry->loc_include);
	strq_start(&entry->link_objects);
	strq_start(&entry->libs);
	bufd_start(&entry->ofile);
	bufd_start(&entry->efile);
	entry->need_compile = 0;
	entry->need_link = 0;

	if (clist->end)
		clist->end->next = entry;
	else
		clist->beg = entry;

	clist->end = entry;

	return entry;
	}

struct clist_entry *clist_find(struct clist *clist, const char *cfile)
	{
	struct clist_entry *entry;
	for (entry = clist->beg; entry; entry = entry->next)
		if (strcmp(cfile, entry->cfile.beg) == 0)
			return entry;

	return 0;
	}

void clist_shift(struct clist *clist)
	{
	if (clist->beg)
		{
		struct clist_entry *entry = clist->beg;
		struct clist_entry *next = entry->next;

		bufd_finish(&entry->cfile);
		strq_finish(&entry->sys_include);
		strq_finish(&entry->loc_include);
		strq_finish(&entry->link_objects);
		strq_finish(&entry->libs);
		bufd_finish(&entry->ofile);
		bufd_finish(&entry->efile);
		free(entry);

		clist->beg = next;
		if (!clist->beg)
			clist->end = 0;
		}
	}

void clist_finish(struct clist *clist)
	{
	while (clist->beg)
		clist_shift(clist);
	}

int getch() { return fp ? (ch = fgetc(fp)) : EOF; }

const char *upto(char termch)
	{
	bufd_clear(&out);
	while (getch() != termch && ch != '\n')
		bufd_putc(&out, (char)ch);
	return out.beg;
	}

int at(const char *s)
	{
	int match = 1;

	while (match && *s)
		{
		if (ch == *s++)
			getch();
		else
			match = 0;
		}

	return match;
	}

int skipspace()
	{
	while (isspace(ch))
		getch();

	return 1;
	}

void statfile(const char *file, struct stat *statbuf)
	{
	statbuf->st_mtime = 0;
	stat(file, statbuf);
	}

int cmp_time_t(time_t x, time_t y)
	{
	return x > y ? 1 : x < y ? -1 : 0;
	}

int cmp_stat(struct stat *sa, struct stat *sb)
	{
	return cmp_time_t(sa->st_mtime, sb->st_mtime);
	}

void clist_entry_print(struct clist_entry *cdata)
	{
	struct strq_entry *pos;

	printf("---\n");
	printf("cfile: %s\n", cdata->cfile.beg);
	printf("ofile: %s\n", cdata->ofile.beg);
	printf("need_compile: %d\n", cdata->need_compile);
	printf("is_main: %d\n", cdata->is_main);

	printf("sys_include:\n");
	for (pos = cdata->sys_include.beg; pos; pos = pos->next)
		printf("  - %s\n", pos->str);

	printf("loc_include:\n");
	for (pos = cdata->loc_include.beg; pos; pos = pos->next)
		printf("  - %s\n", pos->str);

	printf("libs:\n");
	for (pos = cdata->libs.beg; pos; pos = pos->next)
		printf("  - %s\n", pos->str);
	}

void bufd_dirslash(struct bufd *buf)
	{
	if (buf->end > buf->beg && *(buf->end - 1) != dir_separator)
		bufd_putc(buf, dir_separator);
	}

/* If we see "int main(" then it's probably a main program. */
void scan_cfile(const char *cfile)
	{
	struct stat cstat;
	struct stat ostat;

	struct clist_entry *cdata = clist_push(&clist);

	cdata->need_compile = 0;
	bufd_put(&cdata->cfile, cfile);
	bufd_put(&cdata->ofile, opt_objdir.beg);
	bufd_dirslash(&cdata->ofile);
	bufd_put(&cdata->ofile, cfile);
	bufd_clip(&cdata->ofile, 1);
	bufd_putc(&cdata->ofile, 'o');

	statfile(cdata->cfile.beg, &cstat);
	statfile(cdata->ofile.beg, &ostat);

	if (opt_force || cmp_stat(&cstat, &ostat) > 0)
		cdata->need_compile = 1;

	fp = fopen(cfile,"r");

	while (getch() != EOF)
		{
		if (at("#""include"))
			{
			skipspace();
			if (ch == '"')
				{
				const char *hfile = upto('"');
				if (!cdata->need_compile)
					{
					struct stat hstat;
					statfile(hfile, &hstat);
					if (cmp_stat(&hstat, &ostat) > 0)
						cdata->need_compile = 1;
					}
				strq_push(&cdata->loc_include, hfile);
				}
			else if (ch == '<')
				{
				const char *hfile = upto('>');
				const char *lib = strq_assoc(&syshdr, hfile);
				if (lib)
					strq_push(&cdata->libs, lib);
				strq_push(&cdata->sys_include, hfile);
				}
			}
		else if (at("int") && isspace(ch) && skipspace() &&
			at("main") && skipspace() && ch == '(')
			{
			cdata->is_main = 1;
			}
		}

	if (fp)
		{
		fclose(fp);
		fp = 0;
		}

	if (opt_analyze)
		clist_entry_print(cdata);
	}

void add_link_objects(struct clist_entry *cdata, struct clist_entry *cmain)
	{
	struct strq_entry *pos;
	for (pos = cdata->loc_include.beg; pos; pos = pos->next)
		{
		const char *hfile = pos->str;

		struct bufd cfile;
		bufd_start(&cfile);
		bufd_put(&cfile,hfile);
		bufd_clip(&cfile,1);
		bufd_putc(&cfile,'c');

		strq_include(&cmain->link_objects, cfile.beg);
		bufd_finish(&cfile);
		}
	}

void add_libs(struct clist_entry *cdata, struct clist_entry *cmain)
	{
	struct strq_entry *pos;
	for (pos = cdata->libs.beg; pos; pos = pos->next)
		strq_include(&cmain->libs, pos->str);
	}

void extend_link_objects(struct clist_entry *cmain)
	{
	struct strq_entry *pos;
	for (pos = cmain->link_objects.beg; pos; pos = pos->next)
		{
		struct clist_entry *cdata = clist_find(&clist, pos->str);
		if (cdata)
			{
			if (cdata->need_compile)
				cmain->need_link = 1;
			add_link_objects(cdata, cmain);
			add_libs(cdata, cmain);
			}
		}
	}

void compute_link_dependencies(void)
	{
	struct clist_entry *cdata;
	for (cdata = clist.beg; cdata; cdata = cdata->next)
		{
		if (cdata->is_main)
			{
			bufd_start(&cdata->efile);
			bufd_put(&cdata->efile, opt_bindir.beg);
			bufd_dirslash(&cdata->efile);
			bufd_put(&cdata->efile,cdata->cfile.beg);
			bufd_clip(&cdata->efile,2);

			strq_push(&cdata->link_objects,cdata->cfile.beg);
			extend_link_objects(cdata);

			if (!cdata->need_link)
				{
				/* Check to see if the executable file does not exist at all.
				In that case we definitely need to link.
				*/
				struct stat estat;
				if (stat(cdata->efile.beg, &estat) == -1)
					cdata->need_link = 1;
				}

			if (opt_analyze)
				{
				struct strq_entry *pos;
				printf("---\n");
				printf("main: %s\n", cdata->cfile.beg);
				printf("efile: %s\n", cdata->efile.beg);
				printf("need_link: %d\n", cdata->need_link);
				printf("link_objects:\n");
				for (pos = cdata->link_objects.beg; pos; pos = pos->next)
					printf("  - %s\n", pos->str);
				printf("libs:\n");
				for (pos = cdata->libs.beg; pos; pos = pos->next)
					printf("  - %s\n", pos->str);
				}
			}
		}
	}

void resolve_embedded_var(struct clist_entry *cdata, struct bufd *cmd)
	{
	if (strcmp(out.beg,"cfile")==0)
		bufd_put(cmd, cdata->cfile.beg);
	else if (strcmp(out.beg,"ofile")==0)
		bufd_put(cmd, cdata->ofile.beg);
	else if (strcmp(out.beg,"efile")==0)
		bufd_put(cmd, cdata->efile.beg);
	else if (strcmp(out.beg,"objects")==0)
		{
		int first = 1;
		struct strq_entry *pos;
		for (pos = cdata->link_objects.beg; pos; pos = pos->next)
			{
			struct clist_entry *entry = clist_find(&clist, pos->str);
			if (entry)
				{
				if (first)
					first = 0;
				else
					bufd_putc(cmd, ' ');
				bufd_put(cmd, entry->ofile.beg);
				}
			}
		for (pos = cdata->libs.beg; pos; pos = pos->next)
			{
			if (first)
				first = 0;
			else
				bufd_putc(cmd, ' ');
			bufd_put(cmd, pos->str);
			}
		}
	else
		fprintf(stderr,"Unrecognized embedded var '%s'\n", out.beg);
	}

void fmt_cmd(const char *template, struct clist_entry *cdata,
	struct bufd *cmd)
	{
	const char *s;
	bufd_clear(cmd);
	for (s = template; *s; s++)
		{
		if (*s == '$')
			{
			s++;
			if (*s)
				{
				if (*s == '(')
					{
					bufd_clear(&out);
					for (s++; *s && *s != ')'; s++)
						bufd_putc(&out, *s);

					if (*s == ')')
						resolve_embedded_var(cdata, cmd);
					else
						fprintf(stderr,"Embedded var '%s' not terminated "
							"with ')'\n", out.beg);
					}
				else
					{
					bufd_putc(cmd, '$');
					bufd_putc(cmd, *s);
					}
				}
			}
		else
			bufd_putc(cmd, *s);
		}
	}

/* Run a system command but ignore any errors. 
*/
void do_system(const char *cmd)
	{
	int result = system(cmd);
	(void)result;
	}

void do_build(void)
	{
	struct bufd cmd;
	struct clist_entry *cdata;

	bufd_start(&cmd);

	mkdir(opt_objdir.beg, 0755);
	mkdir(opt_bindir.beg, 0755);

	for (cdata = clist.beg; cdata; cdata = cdata->next)
		{
		if (cdata->need_compile)
			{
			fmt_cmd(opt_compile.beg, cdata, &cmd);

			if (opt_showcmd)
				printf("%s\n", cmd.beg);
			if (opt_clean)
				unlink(cdata->ofile.beg);
			if (opt_build)
				do_system(cmd.beg);
			}
		}

	for (cdata = clist.beg; cdata; cdata = cdata->next)
		{
		if (cdata->need_link)
			{
			fmt_cmd(opt_link.beg, cdata, &cmd);

			if (opt_showcmd)
				printf("%s\n", cmd.beg);
			if (opt_clean)
				unlink(cdata->efile.beg);
			if (opt_build)
				do_system(cmd.beg);
			}
		}

	bufd_finish(&cmd);
	}

void get_token()
	{
	bufd_clear(&out);

	skipspace();

	if (ch == '"')
		{
		getch();
		while (ch != EOF && ch != '"')
			{
			if (ch == '\\')
				getch();

			if (ch != EOF)
				{
				bufd_putc(&out, (char)ch);
				getch();
				}
			}
		}
	else
		{
		while (ch != EOF && !isspace(ch))
			{
			bufd_putc(&out, (char)ch);
			getch();
			}
		}
	}

void read_config(const char *filename)
	{
	fp = fopen(filename, "r");
	if (fp)
		{
		while (getch() != EOF)
			{
			while (ch != EOF && (isspace(ch) || ch == '#'))
				{
				skipspace();
				if (ch == '#')
					while (getch() != '\n' && ch != EOF)
						;
				}

			get_token();
			if (out.beg[0])
				{
				if (strcmp(out.beg,"syshdr") == 0)
					{
					get_token();
					strq_push(&syshdr, out.beg);
					get_token();
					strq_push(&syshdr, out.beg);
					}
				else if (strcmp(out.beg,"objdir") == 0)
					{
					get_token();
					bufd_clear(&opt_objdir);
					bufd_put(&opt_objdir, out.beg);
					}
				else if (strcmp(out.beg,"bindir") == 0)
					{
					get_token();
					bufd_clear(&opt_bindir);
					bufd_put(&opt_bindir, out.beg);
					}
				else if (strcmp(out.beg,"compile") == 0)
					{
					get_token();
					bufd_clear(&opt_compile);
					bufd_put(&opt_compile, out.beg);
					}
				else if (strcmp(out.beg,"link") == 0)
					{
					get_token();
					bufd_clear(&opt_link);
					bufd_put(&opt_link, out.beg);
					}
				else
					fprintf(stderr,"Unknown directive %s\n", out.beg);
				}
			}

		fclose(fp);
		fp = 0;
		}
	}

void version(const char *prog)
	{
	printf("This is %s version %s.\n", prog, VERSION);
	}

void usage(const char *prog)
	{
	printf(
"Usage: %s [-a | -c | -h | -q | -s | -v] [-f]\n\n"
"To compile and build your C programs, just run 'clump' with no options.\n"
"This compiles and links your C code in the most intelligent way possible.\n"
"See the README file for more details on what this means.\n"
"\n"
"Other options are:\n"
"\n"
"  clump -a\n"
"    Analyze the C files in the current directory, printing a detailed report\n"
"    showing all the header includes and exactly what needs to be recompiled\n"
"    and linked.  The report is in YAML format (http://yaml.org).\n"
"\n"
"  clump -c\n"
"    Clean out (delete) all target files, but do not do a build.\n"
"\n"
"  clump -f\n"
"    Force the program to consider all targets out of date and in need of\n"
"    rebuilding.  Can be used in conjunction with the -a, -q, or -s options.\n"
"\n"
"  clump -h\n"
"    Show this help screen.\n"
"\n"
"  clump -q\n"
"    Do the build but be quiet about it, not showing the commands being\n"
"    executed.\n"
"\n"
"  clump -s\n"
"    Show the commands that would be used to do the build without actually\n"
"    executing them.\n"
"\n"
"  clump -v\n"
"    Show the version number.\n"
"\n"
"  (COMING SOON)\n"
"  clump -m\n"
"    Create a Makefile.\n"
"\n"
"Normally you'll just run 'clump' with no command-line options whatsoever,\n"
"and this will compile and link your C code in the most intelligent way\n"
"possible.\n"
"\n"

	,prog);

	version(prog);
	}

void run(void)
	{
	bufd_start(&out);
	bufd_put(&out,"");  /* force allocation */
	strq_start(&syshdr);
	clist_start(&clist);

	read_config("clump.ini");

	dir_start(&iterator);

	while (!iterator.dir_done)
		{
		int len = strlen(iterator.file_name);
		if (len > 2 && strcmp(iterator.file_name+len-2, ".c") == 0)
			scan_cfile(iterator.file_name);

		dir_next(&iterator);
		}

	dir_end(&iterator);

	compute_link_dependencies();
	do_build();

	bufd_finish(&out);
	strq_finish(&syshdr);
	clist_finish(&clist);
	}

int main(int argc, char *argv[])
	{
	int retval = 0;
	int ch;

	opt_analyze = 0;
	opt_build = 1;
	opt_clean = 1;
	opt_showcmd = 1;
	opt_force = 0;

	bufd_start(&opt_objdir);
	bufd_start(&opt_bindir);
	bufd_start(&opt_compile);
	bufd_start(&opt_link);

	bufd_put(&opt_objdir, default_objdir);
	bufd_put(&opt_bindir, default_bindir);
	bufd_put(&opt_compile, default_compile);
	bufd_put(&opt_link, default_link);

	while ((ch = getopt(argc, argv, "acfhqsv")) != -1)
		switch (ch)
		{
		case 'a':
			/* Show a detailed analysis in YAML format. */
			opt_analyze = 1;
			opt_build = 0;
			opt_clean = 0;
			opt_showcmd = 0;
			break;
		case 'c':
			opt_analyze = 0;
			opt_build = 0;
			opt_clean = 1;
			opt_showcmd = 0;
			opt_force = 1;
			break;
		case 'f':
			/* Force build all targets. */
			if (!opt_analyze && !opt_showcmd)
				{
				opt_build = 1;
				opt_clean = 1;
				}
			opt_force = 1;
			break;
		case 'q':
			/* Build without showing commands. */
			opt_analyze = 0;
			opt_build = 1;
			opt_clean = 1;
			opt_showcmd = 0;
			break;
		case 's':
			/* Show compile and link commands only; do not build. */
			opt_analyze = 0;
			opt_build = 0;
			opt_clean = 0;
			opt_showcmd = 1;
			break;
		case 'v':
			version(argv[0]);
			retval = 2;
			break;
		case 'h':
		default:
			usage(argv[0]);
			retval = 2;
		}

	if (retval == 0)
		run();

	bufd_finish(&opt_objdir);
	bufd_finish(&opt_bindir);
	bufd_finish(&opt_compile);
	bufd_finish(&opt_link);

	return retval;
	}
