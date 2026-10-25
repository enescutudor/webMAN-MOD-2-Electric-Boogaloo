#define SC_FS_LINK						(810)

static int sysLv2FsLink(const char *oldpath, const char *newpath)
{
	system_call_2(SC_FS_LINK, (u64)(u32)oldpath, (u64)(u32)newpath);
	return_to_user_prog(int);
}

static void free_size(const char *dev_name, char *out)
{
	const char *str_free = (char *)STR_MBFREE;
	int dev_free = (int)(get_free_space(dev_name)>>20), dm = (dev_free % KB) / 100;
	if(dev_free > 1024) {dev_free /= KB, str_free = (char *)STR_GBFREE;}
	sprintf(out, " %'d.%i %s", dev_free, dm, str_free);
}

static s64 file_ssize(const char *path)
{
	if(!path || *path != '/') return FAILED;

#ifdef USE_NTFS
	if(is_ntfs_path(path))
	{
		char tmp[STD_PATH_LEN];
		strcpy(tmp, ntfs_path(path));
		struct stat bufn;
		if(ps3ntfs_stat(tmp, &bufn) < 0) return FAILED;
		return bufn.st_size;
	}
#endif

	struct CellFsStat s;
	if(cellFsStat(path, &s) != CELL_FS_SUCCEEDED) return FAILED;
	return s.st_size;
}

static u64 file_size(const char *path)
{
	s64 fs = file_ssize(path);
	if(fs <= FAILED) return 0;
	return fs;
}

#ifdef COPY_PS3
static u8  do_chmod = 0;
static u16 new_mode = 0777;
static u8  check_md5 = 0;
static u32 dir_count = 0;
static u32 file_count = 0;
static char *stitle_id = NULL;

static u64 size_link(const char *source) // fork (#19): was size_t (32 bits), files over 4 GB were counted modulo 4 GB
{
	u64 size = file_size(source);

	if(!stitle_id) {file_count++; return size;}

	if(islike(source, INT_HDD_ROOT_PATH))
	{
		save_file(FILE_LIST_TXT, current_file, APPEND_TEXT);

		char *usrdir = strstr(source, "/USRDIR/");
		if(usrdir)
		{
			if(!islike(usrdir + 8, "EBOOT.BIN"))
			{
				char game_path[MAX_PATH_LEN];
				concat3(game_path, _HDD0_GAME_DIR, stitle_id, usrdir);
				if(file_size(game_path) == size)
				{
					if(copy_aborted) return 0;
					strcpy(current_file, source);

					#ifdef CALC_MD5
					if(check_md5)
					{
						char *hash1 = cp_path;
						char *hash2 = cp_path + 40;
						calc_md5(source, hash1);
						if(copy_aborted) return 0;
						calc_md5(game_path, hash2);
						if(!IS(hash1, hash2)) return 0;
					}
					#endif
					if(copy_aborted) return 0;

					cellFsUnlink(game_path);
					sysLv2FsLink(source, game_path);

					strncpy(game_path, "lnk->", 5);

					save_file(FILE_LIST_TXT, game_path, APPEND_TEXT);
					file_count++; return size;
				}
			}
		}
	}
	return 0;
}

static u64 folder_size(const char *path)
{
	if(!path) return 0; // sanity check

	if(!isDir(path))
	{
		if(do_chmod) cellFsChmod(path, new_mode);

		return file_size(path);
	}

	int fd; bool is_ntfs = false;

	u64 dir_size = 0;

	copy_aborted = false;

#ifdef USE_NTFS
	struct stat bufn;
	DIR_ITER *pdir = NULL;

	if(is_ntfs_path(path))
	{
		pdir = ps3ntfs_opendir(ntfs_path(path));
		if(pdir) is_ntfs = true;
	}
#endif

	if(is_ntfs || cellFsOpendir(path, &fd) == CELL_FS_SUCCEEDED)
	{
		CellFsDirectoryEntry entry; u32 read_f;
		char *entry_name = entry.entry_name.d_name;

		char source[STD_PATH_LEN];
		u16 plen1 = strcopy(source, path);
		char *psource = source + plen1;

		while(working)
		{
#ifdef USE_NTFS
			if(is_ntfs)
			{
				if(ps3ntfs_dirnext(pdir, entry_name, &bufn)) break;
			}
			else
#endif
			if(cellFsGetDirectoryEntries(fd, &entry, sizeof(entry), &read_f) || !read_f) break;

			if(entry_name[0] == '.' && (entry_name[1] == '.' || entry_name[1] == '\0')) continue;

			if(copy_aborted) break;

			sprintf(psource, "/%s", entry_name); if(do_chmod) cellFsChmod(source, new_mode);

			// fork (S15, F4): the entry already has type and size; /stat.ps3 used 2 stats per file
			if(!is_ntfs && !stitle_id && !IS(path, "/") && ENTRY_TYPE_KNOWN(entry))
			{
				if(ENTRY_IS_DIR(entry)) {dir_count++; dir_size += folder_size(source);}
				else {file_count++; dir_size += entry.attribute.st_size;}
				continue;
			}

			if(isDir(source))
			{
				dir_count++;
				dir_size += folder_size(source);
			}
			else
			{
				dir_size += size_link(source);
			}
		}

#ifdef USE_NTFS
		if(is_ntfs) ps3ntfs_dirclose(pdir);
		else
#endif
		cellFsClosedir(fd);
	}

	return dir_size;
}
#endif // #ifdef COPY_PS3

static int folder_count(const char *path, s8 max_items)
{
	int fd;

	int dir_items = 0;

	if(cellFsOpendir(path, &fd) == CELL_FS_SUCCEEDED)
	{
		CellFsDirectoryEntry entry; u32 read_f;
		char *entry_name = entry.entry_name.d_name;

		while(working)
		{
			if(cellFsGetDirectoryEntries(fd, &entry, sizeof(entry), &read_f) || !read_f) break;

			if(entry_name[0] == '.') continue;

			if((max_items == 2) && !islike(entry_name, "0000")) continue;

			if(++dir_items == max_items) break; // max_items: 1=is_dir_empty, 2=has_one_user_dir (2 or more), -1=folder count
		}

		cellFsClosedir(fd);
	}

	return dir_items;
}
