// Fork (S14): the game scan calls cellFsOpendir() on every entry of an ISO folder, to find the subfolders. A failed
// call costs ~0.5 ms, and for NTFS games the scan lists WMTMP once per ISO type, so a few hundred cached covers and
// SFOs cost ~0.6 s per refresh. cellFsGetDirectoryEntries already returns the type: regular files skip the call
// (it fails on a file anyway). Entries of any other or unknown type are still probed.

#define IS_REGULAR_FILE(entry)	(((entry).attribute.st_mode & CELL_FS_S_IFMT) == CELL_FS_S_IFREG)
