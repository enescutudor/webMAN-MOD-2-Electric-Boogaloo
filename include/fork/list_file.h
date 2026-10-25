// Fork (S15, F5): the list file of a wildcard/recursive listing stays open, with a 4 KB buffer.
//
// scan() wrote every line with save_file(): chmod, open, 2 writes, close, chmod, ~6 file system calls per line.
// Now the outermost list scan opens the file once and the lines go through a buffer. The bytes in the file are the
// same (each line followed by "\r\n"). Only the thread that started the listing uses the buffer; a listing that
// starts while another one runs (script and web at the same time) keeps the upstream save_file() path.

#define LIST_FILE_BUFFER	_4KB_

static struct
{
	sys_ppu_thread_t owner;
	int  fd;
	u16  len;
	char buf[LIST_FILE_BUFFER];
} list_file = {0, -1, 0, {0}};

static u32 list_file_busy = 0;

static bool list_file_mine(void)
{
	sys_ppu_thread_t me; sys_ppu_thread_get_id(&me);
	return list_file_busy && (list_file.owner == me);
}

static void list_file_flush(void)
{
	if(list_file.len) cellFsWrite(list_file.fd, list_file.buf, list_file.len, NULL);
	list_file.len = 0;
}

// Returns true when this call owns the list file and must call list_file_end().
static bool list_file_begin(enum scan_operations fop, const char *dest)
{
	if(((fop != SCAN_LIST) && (fop != SCAN_LIST_SIZE) && (fop != SCAN_LIST_JSON)) || !dest || (*dest != '/')) return false;
	if(list_file_mine()) return false; // nested call of the same listing
	if(!__sync_bool_compare_and_swap(&list_file_busy, 0, 1)) return false;

	cellFsChmod(dest, MODE); // like save_file()
	if(cellFsOpen(dest, CELL_FS_O_CREAT | CELL_FS_O_WRONLY | CELL_FS_O_APPEND, &list_file.fd, NULL, 0) != CELL_FS_SUCCEEDED)
	{
		list_file.fd = -1;
		__sync_lock_release(&list_file_busy);
		return false;
	}
	sys_ppu_thread_get_id(&list_file.owner);
	list_file.len = 0;
	return true;
}

static void list_file_end(const char *dest)
{
	list_file_flush();
	cellFsClose(list_file.fd); list_file.fd = -1;
	cellFsChmod(dest, MODE);
	__sync_lock_release(&list_file_busy);
}

// Same as save_file(dest, line, APPEND_TEXT).
static void list_file_line(const char *dest, const char *line)
{
	if(!list_file_mine()) {save_file(dest, line, APPEND_TEXT); return;}

	u32 n = strlen(line);
	if((u32)list_file.len + n + 2 > LIST_FILE_BUFFER) list_file_flush();
	if(n + 2 > LIST_FILE_BUFFER) {cellFsWrite(list_file.fd, line, n, NULL); cellFsWrite(list_file.fd, "\r\n", 2, NULL); return;}

	memcpy(list_file.buf + list_file.len, line, n); list_file.len += n;
	memcpy(list_file.buf + list_file.len, "\r\n", 2); list_file.len += 2;
}
