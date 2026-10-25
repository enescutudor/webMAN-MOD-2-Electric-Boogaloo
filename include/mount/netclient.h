#ifdef NET_SUPPORT
static int  g_socket = NONE;
static u16 net_copy_in_progress = 0;
#endif

#ifdef NET3NET4
	static const u8 netsrvs = 5;
#else
	static const u8 netsrvs = 3;
#endif

static u16 normalized_len(const char *path);

static u32 CD_SECTOR_SIZE_2352 = 2352;

typedef struct
{
	char server[0x40];
	char path[0x420];
	u32 emu_mode;
	u32 num_tracks;
	u16 port;
	u8 pad[6];
	ScsiTrackDescriptor tracks[MAX_TRACKS];
} __attribute__((packed)) _netiso_args;

_netiso_args netiso_args;

#define MAX_RETRIES    3

#define TEMP_NET_PSXCUE  WMTMP "/~netpsx.cue"

static int8_t netiso_svrid = NONE;

static int read_remote_file(int s, void *buf, u64 offset, u32 size, int *abort_connection)
{
	*abort_connection = 1;

	netiso_read_file_cmd cmd;
	netiso_read_file_result res;

	_memset(&cmd, sizeof(cmd));
	cmd.opcode = (NETISO_CMD_READ_FILE);
	cmd.offset = (offset);
	cmd.num_bytes = (size);

	if(send(s, &cmd, sizeof(cmd), 0) != sizeof(cmd))
	{
		//DPRINTF("send failed (read_remote_file) (errno=%d)!\n", get_network_error());
		return FAILED;
	}

	if(recv(s, &res, sizeof(res), MSG_WAITALL) != sizeof(res))
	{
		//DPRINTF("recv failed (read_remote_file) (errno=%d)!\n", get_network_error());
		return FAILED;
	}

	*abort_connection = 0;

	int bytes_read = (res.bytes_read);
	if(bytes_read <= 0)
		return bytes_read;

	if(recv(s, buf, bytes_read, MSG_WAITALL) != bytes_read)
	{
		//DPRINTF("recv failed (read_remote_file) (errno=%d)!\n", get_network_error());
		*abort_connection = 1;
		return FAILED;
	}

	return bytes_read;
}

static s64 open_remote_file(int s, const char *path, int *abort_connection)
{
	*abort_connection = 1;

	s32 net_enabled = 0;
	xnet()->GetSettingNet_enable(&net_enabled);

	if(!net_enabled) return FAILED;

	netiso_open_cmd cmd;
	netiso_open_result res;

	int len = normalized_len(path);
	_memset(&cmd, sizeof(cmd));
	cmd.opcode = BE16(NETISO_CMD_OPEN_FILE);
	cmd.fp_len = BE16(len);

	if(send(s, &cmd, sizeof(cmd), 0) != sizeof(cmd))
	{
		//DPRINTF("send failed (open_remote_file) (errno=%d)!\n", get_network_error());
		return FAILED;
	}

	if(send(s, path, len, 0) != len)
	{
		//DPRINTF("send failed (open_remote_file) (errno=%d)!\n", get_network_error());
		return FAILED;
	}

	if(recv(s, &res, sizeof(res), MSG_WAITALL) != sizeof(res))
	{
		//DPRINTF("recv failed (open_remote_file) (errno=%d)!\n", get_network_error());
		return FAILED;
	}

	if(res.file_size <= NONE)
	{
		//DPRINTF("Remote file %s doesn't exist!\n", path);
		return FAILED;
	}

	// detect CD sector size
	#ifdef USE_INTERNAL_NET_PLUGIN
	int emu_mode = *abort_connection; // called by netiso_thread()
	if((emu_mode == EMU_PSX) && (BETWEEN(_64KB_, res.file_size, 870 * _1MB_)))
	{
		CD_SECTOR_SIZE_2352 = default_cd_sector_size(res.file_size);

		sys_addr_t sysmem = sys_mem_allocate(_64KB_);
		if(sysmem)
		{
			char *chunk = (char*)sysmem;

			int bytes_read = read_remote_file(s, (char*)chunk, 0, _64KB_, abort_connection);
			if(bytes_read)
			{
				CD_SECTOR_SIZE_2352 = detect_cd_sector_size(chunk);
			}

			sys_memory_free(sysmem);
		}
	}
	#endif

	*abort_connection = 0;

	return (res.file_size);
}

static int remote_stat(int s, const char *path, int *is_directory, s64 *file_size, u64 *mtime, u64 *ctime, u64 *atime, int *abort_connection)
{
	*abort_connection = 1;

	s32 net_enabled = 0;
	xnet()->GetSettingNet_enable(&net_enabled);

	if(!net_enabled) return FAILED;

	netiso_stat_cmd cmd;
	netiso_stat_result res;

	int len = normalized_len(path);
	_memset(&cmd, sizeof(cmd));
	cmd.opcode = (NETISO_CMD_STAT_FILE);
	cmd.fp_len = (len);

	if(send(s, &cmd, sizeof(cmd), 0) != sizeof(cmd))
	{
		//DPRINTF("send failed (remote_stat) (errno=%d)!\n", get_network_error());
		return FAILED;
	}

	if(send(s, path, len, 0) != len)
	{
		//DPRINTF("send failed (remote_stat) (errno=%d)!\n", get_network_error());
		return FAILED;
	}

	if(recv(s, &res, sizeof(res), MSG_WAITALL) != sizeof(res))
	{
		//DPRINTF("recv failed (remote_stat) (errno=%d)!\n", get_network_error());
		return FAILED;
	}

	*abort_connection = 0;

	*file_size = (res.file_size);
	if(*file_size == NONE)
		return FAILED;

	*is_directory = res.is_directory;
	*mtime = (res.mtime);
	*ctime = (res.ctime);
	*atime = (res.atime);

	return CELL_OK;
}

static bool remote_file_exists(int ns, const char *remote_file)
{
	s64 size = 0; int abort_connection = 0;
	int is_directory = 0; u64 mtime, ctime, atime;

	if(remote_stat(ns, remote_file, &is_directory, &size, &mtime, &ctime, &atime, &abort_connection) == FAILED)
		return false;

	if(is_directory || (size > 0))
		return true;

	return false;
}

static int copy_net_file(const char *local_file, const char *remote_file, int ns)
{
	copy_aborted = false;

	if(ns < 0) return FAILED;

	if(file_exists(local_file)) return CELL_OK; // local file already exists

	// check invalid characters
	for(u16 c = 0; remote_file[c]; c++)
	{
		if(strchr("\"<|>:*?", remote_file[c])) return FAILED;
	}

	if(remote_file_exists(ns, remote_file) == false) return FAILED;

	// copy remote file
	int ret = FAILED;
	int abort_connection = 0;

	s64 size = open_remote_file(ns, remote_file, &abort_connection);

	u64 file_size = size; current_size = size;

	if(file_size > 0)
	{
		u32 chunk_size = _64KB_; sys_addr_t sysmem = sys_mem_allocate(chunk_size);
		if(sysmem)
		{
			char *chunk = (char*)sysmem; int fdw;

			show_progress(remote_file, OV_COPY);

			if(cellFsOpen(local_file, CELL_FS_O_CREAT | CELL_FS_O_WRONLY | CELL_FS_O_TRUNC, &fdw, NULL, 0) == CELL_FS_SUCCEEDED)
			{
				if(chunk_size > file_size) chunk_size = (u32)file_size;

				int bytes_read; u64 boff = 0;
				while(boff < file_size)
				{
					if(copy_aborted) break;

					bytes_read = read_remote_file(ns, (char*)chunk, boff, chunk_size, &abort_connection);
					if(bytes_read)
						cellFsWrite(fdw, (char*)chunk, bytes_read, NULL);

					boff += bytes_read;
					if(((u64)bytes_read < chunk_size) || abort_connection) break;
				}
				cellFsClose(fdw);
				cellFsChmod(local_file, MODE);

				ret = CELL_OK; copied_size += boff;
			}
			sys_memory_free(sysmem);
		}
	}

	//open_remote_file(ns, "/CLOSEFILE", &abort_connection); // <- cause of bug: only 1 remote file is copied during refresh xml

	return ret;
}

static bool is_netsrv_enabled(u8 server_id)
{
	server_id &= 0x0F; // change '0'-'4' to  0..4

	if(netiso_svrid == server_id) return true;

	#ifdef NET3NET4
	if(server_id > 4) return false;
	#else
	if(server_id > 2) return false;
	#endif

	s32 net_enabled = 0;
	xnet()->GetSettingNet_enable(&net_enabled);

	if(!net_enabled) return false;

	return( (webman_config->netd[server_id] == 1) && // is enabled
			(webman_config->neth[server_id][0])  && // has host
			(webman_config->netp[server_id] > 0) && // has port
			!islike(webman_config->neth[server_id], "127.") && !islike(webman_config->neth[server_id], "localhost") // not a loopback
		);
}

static int connect_to_remote_server(u8 server_id)
{
	int ns = FAILED;

	server_id &= 0x0F; // change '0'-'4' to  0..4

	if( is_netsrv_enabled(server_id) )
	{
		// check duplicated connections
		for(u8 n = 0; n < server_id; n++)
			if((webman_config->netd[n] == 1) && IS(webman_config->neth[n], webman_config->neth[server_id]) && webman_config->netp[n] == webman_config->netp[server_id]) return FAILED;

		u8 retries = 0, rcv_timeout = 30, max_tries = MAX_RETRIES;

		if(refreshing_xml)
		{
			rcv_timeout = 5, max_tries = 1;
		}

	reconnect:

		ns = connect_to_server_ex(webman_config->neth[server_id], webman_config->netp[server_id], rcv_timeout);

		if(ns < 0)
		{
			if(retries < max_tries)
			{
				retries++;
				sys_ppu_thread_sleep(1);
				goto reconnect;
			}

			netiso_svrid = NONE;
			if(refreshing_xml && (webman_config->refr))
				webman_config->netd[server_id] = 0; // disable connection to offline servers (only when content scan on startup is disabled)

			if(server_id > 0 || !webman_config->netd[0] || islike(webman_config->allow_ip, "127.") || IS(webman_config->allow_ip, "localhost")) return ns;

			for(u8 n = 1; n < netsrvs; n++)
				if(IS(webman_config->neth[n], webman_config->allow_ip)) return ns;

			// retry using IP of client (/net0 only) - update IP in neth[0] if connection is successful
			ns = connect_to_server_ex(webman_config->allow_ip, webman_config->netp[0], rcv_timeout);
			if(ns >= 0) strcpy(webman_config->neth[0], webman_config->allow_ip);
		}
	}

	if(ns >= 0) set_tcp_nodelay(ns); // fork (S14, N5)
	return ns;
}

static bool remote_is_dir(int ns, const char *path)
{
	if(*path && (ns >= 0))
	{
		s64 size = 0; int abort_connection = 0;
		int is_directory = 0; u64 mtime, ctime, atime;

		if(remote_stat(ns, path, &is_directory, &size, &mtime, &ctime, &atime, &abort_connection) == FAILED)
			return false;

		if(is_directory)
			return true;
	}
	return false;
}

#ifdef NET_SUPPORT
static int open_remote_dir(int s, const char *path, int *abort_connection, bool subdirs)
{
	*abort_connection = 1;

	s32 net_enabled = 0;
	xnet()->GetSettingNet_enable(&net_enabled);

	if(!net_enabled)
	{
		return FAILED;
	}

	netiso_open_dir_cmd cmd;
	netiso_open_dir_result res;

	char *_path = (char *)path;

	if(webman_config->nsd)
		subdirs = false;

	int len = normalized_len(path);

	if(subdirs)
		{strcat(_path, "//"); len += 2;}
	else if(!*path || (len <= 1))
		{_path = (char*)"/.", len = 2;}

	if(remote_is_dir(s, _path) == false)
		return FAILED;

	_memset(&cmd, sizeof(cmd));
	cmd.opcode = (NETISO_CMD_OPEN_DIR);
	cmd.dp_len = (len);

	if(send(s, &cmd, sizeof(cmd), 0) != sizeof(cmd))
	{
		//DPRINTF("send failed (open_remote_dir) (errno=%d)!\n", get_network_error());
		return FAILED;
	}

	if(send(s, _path, len, 0) != len)
	{
		//DPRINTF("send failed (open_remote_dir) (errno=%d)!\n", get_network_error());
		return FAILED;
	}

	if(subdirs) _path[len - 2] = '\0';

	if(recv(s, &res, sizeof(res), MSG_WAITALL) != sizeof(res))
	{
		//DPRINTF("recv failed (open_remote_dir) (errno=%d)!\n", get_network_error());
		return FAILED;
	}

	*abort_connection = 0;

	return (res.open_result);
}

static int read_remote_dir(int s, sys_addr_t *data /*netiso_read_dir_result_data **data*/, int *abort_connection)
{
	*abort_connection = 1;

	netiso_read_dir_entry_cmd cmd;
	netiso_read_dir_result res;

	_memset(&cmd, sizeof(cmd));
	cmd.opcode = (NETISO_CMD_READ_DIR);

	//MM_LOG("Sending request...(%i) ", s);
	if(send(s, &cmd, sizeof(cmd), 0) != sizeof(cmd))
	{
	//MM_LOG("FAILED!\n");
		return FAILED;
	}
	//MM_LOG("Receiving response...");
	if(recv(s, &res, sizeof(res), MSG_WAITALL) != sizeof(res))
	{
	//MM_LOG("FAILED!\n");
		return FAILED;
	}

	//MM_LOG("OK (%i entries)\n", res.dir_size );
	if(res.dir_size > 0)
	{
		int len;
		sys_addr_t sysmem = NULL;
		for(int retry = 25; retry > 0; retry--)
		{
			if(res.dir_size > (retry * 123)) res.dir_size = retry * 123;
			len = (sizeof(netiso_read_dir_result_data) * res.dir_size);

			int len2 = ((len + _64KB_) / _64KB_) * _64KB_;

			if(webman_config->vsh_mc)
			{
				sysmem = sys_mem_allocate(_3MB_);
			}

			if(sysmem || sys_memory_allocate(len2, SYS_MEMORY_PAGE_SIZE_64K, &sysmem) == CELL_OK)
			{
				u8 *data2 = (u8*)sysmem; *data = sysmem;

				if(recv(s, data2, len, MSG_WAITALL) != len)
				{
					sys_memory_free(sysmem);
					*data = NULL;
					return FAILED;
				}
				break;
			}
			else
				*data = NULL;
		}
	}
	else
		*data = NULL;

	*abort_connection = 0;

	return (res.dir_size);
}
#endif

//#endif //#ifdef NET_SUPPORT
