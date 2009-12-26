struct dir_iterator
	{
#if USE_WINDOWS
	HANDLE *dir_entry;
	WIN32_FIND_DATA find_data;
#else
	DIR *dir;
	struct dirent *entry;
#endif
	int dir_done;
	const char *file_name;
	};

extern void dir_start(struct dir_iterator *iterator);
extern void dir_next(struct dir_iterator *iterator);
extern void dir_end(struct dir_iterator *iterator);
