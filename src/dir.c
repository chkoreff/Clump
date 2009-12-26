#include "config.h"
#if USE_WINDOWS
#include <windows.h>
#else
#include <stdio.h>
#include <dirent.h>
#endif
#include "dir.h"

void dir_start(struct dir_iterator *iterator)
	{
#if USE_WINDOWS
	iterator->dir_entry = FindFirstFile("*", &iterator->find_data);

	iterator->dir_done = (iterator->dir_entry == 0);
	if (!iterator->dir_done)
		iterator->file_name = iterator->find_data.cFileName;
#else
	iterator->dir = opendir(".");

	if (iterator->dir)
		{
		iterator->entry = readdir(iterator->dir);
		iterator->dir_done = (iterator->entry == NULL);
		}
	else
		{
		iterator->dir_done = 1;
		}

	if (!iterator->dir_done)
		iterator->file_name = iterator->entry->d_name;
#endif
	}

void dir_next(struct dir_iterator *iterator)
	{
#if USE_WINDOWS
	iterator->dir_done = !FindNextFile(iterator->dir_entry, &iterator->find_data);
	if (!iterator->dir_done)
		iterator->file_name = iterator->find_data.cFileName;
#else
	iterator->entry = readdir(iterator->dir);
	iterator->dir_done = (iterator->entry == NULL);
	if (!iterator->dir_done)
		iterator->file_name = iterator->entry->d_name;
#endif
	}

void dir_end(struct dir_iterator *iterator)
	{
#if USE_WINDOWS
	FindClose(iterator->dir_entry);
#else
	if (iterator->dir)
		(void)closedir(iterator->dir);
#endif
	}
