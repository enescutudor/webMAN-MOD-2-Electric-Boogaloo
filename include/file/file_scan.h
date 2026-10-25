enum scan_operations
{
	SCAN_LIST   = 0,
	SCAN_DELETE = 1,
	SCAN_COPY   = 2,	// copy (don't replace if file exists with same size)
	SCAN_FCOPY  = 3,	// copy (force replace)
	SCAN_MOVE   = 4,
	SCAN_RENAME = 5,
	SCAN_COPYBK = 6,	// rename source to source + .bak after copy
	SCAN_UNLOCK_SAVE = 7,
	SCAN_LIST_SIZE = 8,
	SCAN_TRUNCATE  = 9,
	SCAN_LIST_JSON = 10
};

#include "../fork/list_file.h"

static void rename_file(char *source, char *dest);

static int scan(const char *path, u8 recursive, const char *wildcard, enum scan_operations fop, const char *dest);

static int scan_tree(const char *path, u8 recursive, const char *wildcard, enum scan_operations fop, const char *dest)
{
	// fop: 0 = scan to file, 1 = del, 2 = copy, 3 = force copy, 4 = move, 5 = rename/move in same fs, 6 = copy bak

	if(recursive == RECURSIVE_DELETE) ; else
	if(!sys_admin || !working) return FAILED;

	#ifdef USE_NTFS
	if((fop == SCAN_DELETE) && !wildcard && !isDir(path))
	{
		if(is_ntfs_path(path))
			return ps3ntfs_unlink(ntfs_path(path));
		else
			return cellFsUnlink(path);
	}
	#else
	if((fop == SCAN_DELETE) && !wildcard && !isDir(path)) return cellFsUnlink(path);
	#endif

	if((fop == SCAN_DELETE || fop == SCAN_TRUNCATE) && (strlen(path) < 11 || islike(path, "/dev_bdvd") || islike(path, "/dev_flash") || islike(path, "/dev_blind"))) return FAILED;

	bool use_dest = BETWEEN(2, fop, 6); // fop: 2 = copy, 3 = force copy, 4 = move, 5 = rename/move in same fs, 6 = copy bk

	if(use_dest) {mkdir_tree(dest); if(!isDir(dest)) return FAILED;}

	char *wildcard1 = NULL, *wildcard2 = NULL;
	char *(*instr)(const char *, const char *) = &strstr; bool wfound1 = true, wfound2 = true;
	if(wildcard)
	{
		wildcard1 = (char*)wildcard;
		if(*wildcard1 == '~') {wildcard1++, wfound1 = false;}					// *~TEXT = exclude files
		if(*wildcard1 == '^') {wildcard1++, instr = &strcasestr;}				// *^TEXT = case insensitive search
		if( wfound1 && (*wildcard1 == '~')) {wildcard1++, wfound1 = false;}		// <-- accept prefixes: ~^ or ^~

		wildcard2 = strchr(wildcard1, '*'); if(wildcard2) *wildcard2++ = NULL;	// *TEXT1*TEXT2 = text1 and text2
		if(wildcard2 && (*wildcard2 == '~')) {wildcard2++, wfound2 = false;}
	}

	int fd; bool is_ntfs = false;

	copy_aborted = false;

	show_progress(path, (fop == SCAN_LIST)   ? OV_SCAN :
						(fop == SCAN_DELETE) ? OV_DELETE :
						(fop <= SCAN_COPYBK) ? OV_COPY : OV_CLEAR);

	int counter = 0;
	bool is_root, prescan = (fop == SCAN_DELETE) | (fop == SCAN_MOVE) | (fop == SCAN_RENAME);
	bool is_json = (fop == SCAN_LIST_JSON);
	bool is_list = (fop == SCAN_LIST) || (fop == SCAN_LIST_SIZE) || is_json;

	#ifdef USE_NTFS
	struct stat bufn;
	DIR_ITER *pdir = NULL;

rescan:

	if(is_ntfs_path(path))
	{
		pdir = ps3ntfs_opendir(ntfs_path(path));
		if(pdir) is_ntfs = true;
	}
	#else
rescan:
	#endif

	is_root = IS(path, "/");

	if(is_ntfs || cellFsOpendir(path, &fd) == CELL_FS_SUCCEEDED)
	{
		CellFsDirent dir; u64 read_e;
		CellFsDirectoryEntry entry_d; u32 read_f;
		char *entry_name = (is_root) ? dir.d_name : entry_d.entry_name.d_name;

		char entry[STD_PATH_LEN], dest_entry[STD_PATH_LEN];

		u16 path_len = strcopy(entry, path);
		bool p_slash = (path_len > 1) && (path[path_len - 1] == '/');
		char *pentry = entry + path_len;

		u16 dest_len = strcopy(dest_entry, dest);
		char *pdest = dest_entry + dest_len;

		while(working)
		{
			#ifdef USE_NTFS
			if(is_ntfs)
			{
				if(ps3ntfs_dirnext(pdir, entry_name, &bufn)) break;
				if(entry_name[0]=='$' && path[12] == 0) continue;
			}
			else
			#endif
			if(is_root && ((cellFsReaddir(fd, &dir, &read_e) != CELL_FS_SUCCEEDED) || (read_e == 0))) break;
			else
			if(cellFsGetDirectoryEntries(fd, &entry_d, sizeof(entry_d), &read_f) || !read_f) break;

			if(copy_aborted) break;
			if(entry_name[0] == '.' && (entry_name[1] == '.' || entry_name[1] == '\0')) continue;

			if(p_slash) strcopy(pentry, entry_name); else sprintf(pentry, "/%s", entry_name);

			if(use_dest) strcopy(pdest, entry_name);

			// fork (S15, F4): listings take type and size from the entry (delete, move, rename, truncate are unchanged)
			bool from_entry = is_list && !is_ntfs && !is_root && ENTRY_TYPE_KNOWN(entry_d);

			if(from_entry ? ENTRY_IS_DIR(entry_d) : isDir(entry))
			{
				if(is_json && (*dest == '/') && !wildcard)
				{
					sprintf(dest_entry, "\t{\"path\":\"%s\",\"size\":0},", entry);
					list_file_line(dest, dest_entry);
				}

				if(recursive) scan(entry, recursive, wildcard, fop, dest);
			}

			else if(wildcard1 && (*wildcard1!=NULL) && ((!instr(pentry, wildcard1)) == wfound1)) continue;
			else if(wildcard2 && (*wildcard2!=NULL) && ((!instr(pentry, wildcard2)) == wfound2)) continue;

			else if(fop == SCAN_LIST || fop == SCAN_LIST_SIZE || fop == SCAN_LIST_JSON)
			{
				if(!dest || *dest != '/') break;

				// add size column if fop == SCAN_LIST_SIZE or is_json
				if(fop)
				{
					if(is_json)
					{
						sprintf(dest_entry, "\t{\"path\":\"%s\",\"size\":%llu},", entry, from_entry ? entry_d.attribute.st_size : file_size(entry));
						list_file_line(dest, dest_entry);
						continue;
					}
					else
						sprintf(entry_name, "\t%'llu", from_entry ? entry_d.attribute.st_size : file_size(entry));
					
					strcat(pentry, entry_name);
				}

				list_file_line(dest, entry);
			}
			#ifdef UNLOCK_SAVEDATA
			else if(fop == SCAN_UNLOCK_SAVE)
			{
				char sfo[_4KB_];
				u16 sfo_size = read_sfo(entry, sfo);
				if(unlock_param_sfo(entry, (unsigned char *)sfo, sfo_size))
				{
					save_file(entry, (void*)sfo, sfo_size);
				}
			}
			#endif
			else if(fop == SCAN_TRUNCATE)
			{
				create_file(entry);
			}
			else if(fop == SCAN_COPY || fop == SCAN_FCOPY || fop == SCAN_COPYBK)
			{
				if(fop == SCAN_FCOPY)
					force_copy(entry, dest_entry); // force copy ntfs & cellFS
				else
					file_copy(entry, dest_entry); // copy ntfs & cellFS

				if((fop == SCAN_COPYBK) && file_exists(dest_entry))
					{concat2(dest_entry, entry, ".bak"); rename_file(entry, dest_entry);}
			}
			else if(prescan)
				counter++; // pre-count move/rename/delete operations
			#ifdef USE_NTFS
			else if(is_ntfs)
			{
				counter--;

				char *ntfs_entry = (char*)ntfs_path(entry);
				if(fop == SCAN_DELETE) {ps3ntfs_unlink(ntfs_entry);} else
				if(fop == SCAN_RENAME) {ps3ntfs_rename(ntfs_entry, dest_entry);} else
				if(fop == SCAN_MOVE  )
				{
					if(is_same_dev(entry, dest_entry))
						rename_file(entry, dest_entry);
					else if(file_copy(entry, dest_entry) >= CELL_OK)
						ps3ntfs_unlink(ntfs_entry);
				}
			}
			#endif
			else
			{
				counter--;

				if(fop == SCAN_DELETE) {cellFsUnlink(entry);} else
				if(fop == SCAN_RENAME) {cellFsRename(entry, dest_entry);} else
				if(fop == SCAN_MOVE  )
				{
					if(is_same_dev(entry, dest_entry))
						rename_file(entry, dest_entry);
					else if(file_copy(entry, dest_entry) >= CELL_OK)
						cellFsUnlink(entry);
				}
			}
		}

		#ifdef USE_NTFS
		if(is_ntfs) ps3ntfs_dirclose(pdir);
		else
		#endif
		cellFsClosedir(fd);

		if(prescan || (counter > 0)) {prescan = false; goto rescan;}

		if(copy_aborted) return FAILED;
	}
	else
		return FAILED;

	if((recursive > 0) && (fop == SCAN_DELETE))
	{
		const char *pname = strrchr(path, '/');
		if(pname)
		{
			++pname;
			if(wildcard1 && (*wildcard1!=NULL) && ((!instr(pname, wildcard1)) == wfound1)) return CELL_FS_SUCCEEDED;
			if(wildcard2 && (*wildcard2!=NULL) && ((!instr(pname, wildcard2)) == wfound2)) return CELL_FS_SUCCEEDED;
		}

#ifdef USE_NTFS
		if(is_ntfs)
			ps3ntfs_unlink(ntfs_path(path));
		else
#endif
			cellFsRmdir(path);
	}

	disable_progress();
	return CELL_FS_SUCCEEDED;
}

// fork (S15, F5): the outermost listing keeps its list file open (fork/list_file.h)
static int scan(const char *path, u8 recursive, const char *wildcard, enum scan_operations fop, const char *dest)
{
	if(!list_file_begin(fop, dest)) return scan_tree(path, recursive, wildcard, fop, dest);

	int ret = scan_tree(path, recursive, wildcard, fop, dest);
	list_file_end(dest);
	return ret;
}
