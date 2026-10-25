// Fork (S14, G6): one pass over WMTMP to delete the NTFS cache files, at the start of prepNTFS.
//
// prepNTFS called scan() once per cache extension. scan() stats every entry (isDir) before it looks at the name
// and, for a delete, lists the folder twice, so with a few hundred cached covers and SFOs in WMTMP the 4 calls
// cost ~0.7 s on every refresh, with or without an NTFS disk. The directory entries already carry the file type.
// Same files are deleted: the names that contain one of the 4 extensions, not folders, not recursive.

static void delete_ntfs_cache_files(void)
{
	if(!sys_admin || !working) return; // same condition as scan()

	const char *cache_ext[4] = { ".ntfs[PS3ISO]", ".ntfs[PSXISO]", ".ntfs[DVDISO]", ".ntfs[BDISO]" };

	char path[STD_PATH_LEN];
	u16 len = strcopy(path, WMTMP "/");

	// like scan(), do not trust the listing while deleting from it: list again until a pass deletes nothing
	for(bool deleted = true; deleted && working; )
	{
		deleted = false;

		int fd;
		if(cellFsOpendir(WMTMP, &fd) != CELL_FS_SUCCEEDED) return;

		CellFsDirectoryEntry entry; u32 read_e;
		while(working && !cellFsGetDirectoryEntries(fd, &entry, sizeof(entry), &read_e) && read_e)
		{
			if(entry.attribute.st_mode & CELL_FS_S_IFDIR) continue;

			for(u8 n = 0; n < 4; n++)
				if(strstr(entry.entry_name.d_name, cache_ext[n]))
				{
					strncopy(path + len, sizeof(path) - len, entry.entry_name.d_name);
					if(cellFsUnlink(path) == CELL_FS_SUCCEEDED) deleted = true;
					break;
				}
		}
		cellFsClosedir(fd);
	}
}
