	if(is_binary == BINARY_FILE) // binary file
	{
		// fork (W1): ETag + Cache-Control; 304 without body if the browser already has this version
		char etag[40];
		bool not_modified = http_etag(etag, c_mtime, c_len) && IS(if_none_match, etag);
		if(*etag) header_len += sprintf(header + header_len, "ETag: %s\r\nCache-Control: %s\r\n", etag, http_cache_control(param));

		size_t buffer_size = 0; if(sysmem) sys_memory_free(sysmem); sysmem = NULL;

		// fork (6.4): a single byte range gets 206 with only that part of the file; outside the file, 416
		u64 r_start = 0, r_end = c_len ? c_len - 1 : 0;
		int ranged = (!not_modified && *range) ? http_range(range, c_len, &r_start, &r_end) : 0;
		u64 r_len = (ranged > 0) ? (r_end - r_start + 1) : c_len;
		header_len += sprintf(header + header_len, "Accept-Ranges: bytes\r\n");

		if(ranged < 0)
		{
			header_len = http_status(header, header_len, "416 Range Not Satisfiable");
			header_len += sprintf(header + header_len, "Content-Range: bytes */%llu\r\nContent-Length: 0\r\n\r\n", (unsigned long long)c_len);
			send(conn_s, header, header_len, 0);
			goto exit_handleclient_www;
		}
		if(ranged > 0)
		{
			header_len = http_status(header, header_len, "206 Partial Content");
			header_len += sprintf(header + header_len, "Content-Range: bytes %llu-%llu/%llu\r\n", (unsigned long long)r_start, (unsigned long long)r_end, (unsigned long long)c_len);
		}

		if(not_modified)
			header_len = http_status(header, header_len, "304 Not Modified");
		else
		{
			// fork (W8): allocate before sending the headers, so a failed allocation gets a clean 503
			u8 npages = r_len ? (u8)MIN(MAX_PAGES, 1 + ((r_len - 1) / _64KB_)) : 1;

			for(u8 n = npages; n > 0; n--)
				if(sys_memory_allocate(n * _64KB_, SYS_MEMORY_PAGE_SIZE_64K, &sysmem) == CELL_OK) {buffer_size = n * _64KB_; if(IS_INGAME) sys_ppu_thread_usleep(5000); break;} // fork (6.3): upstream pause kept in-game only

			if(!sysmem)
			{
				keep_alive = http_response(conn_s, header, param, CODE_SERVER_BUSY, STR_ERROR);
				goto exit_handleclient_www;
			}
		}

		if(keep_alive)
		{
			header_len += sprintf(header + header_len,  "Keep-Alive: timeout=3,max=250\r\n"
														"Connection: keep-alive\r\n");
		}

		header_len += sprintf(header + header_len, "Content-Length: %llu\r\n\r\n", (unsigned long long)r_len);
		send(conn_s, header, header_len, 0);

		if(not_modified) goto exit_handleclient_www;

		if(islike(param, "/dev_bdvd"))
			sysLv2FsBdDecrypt(); // decrypt dev_bdvd files

		char *buffer = (char*)sysmem;
		int fd;
		#ifdef USE_NTFS
		if(is_ntfs)
		{
			fd = ps3ntfs_open(ntfs_path(param), O_RDONLY, 0);
			if(fd <= 0) is_ntfs = false;
		}
		#endif
		if(is_ntfs || cellFsOpen(param, CELL_FS_O_RDONLY, &fd, NULL, 0) == CELL_FS_SUCCEEDED)
		{
			u64 read_e = 0, pos, remaining = r_len;

			// fork (6.7): a client that stops reading releases the thread after 60 s (with Range it can resume)
			struct timeval tv; tv.tv_sec = 60; tv.tv_usec = 0;
			setsockopt(conn_s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

			#ifdef USE_NTFS
			if(is_ntfs) ps3ntfs_seek64(fd, r_start, SEEK_SET);
			else
			#endif
			cellFsLseek(fd, r_start, CELL_FS_SEEK_SET, &pos);

			while(working && remaining)
			{
				size_t chunk = (size_t)MIN((u64)buffer_size, remaining);
				#ifdef USE_NTFS
				if(is_ntfs) read_e = ps3ntfs_read(fd, (void *)buffer, chunk);
				#endif
				if(is_ntfs || cellFsRead(fd, (void *)buffer, chunk, &read_e) == CELL_FS_SUCCEEDED)
				{
					if(read_e > 0)
					{
						#ifdef UNLOCK_SAVEDATA
						if(webman_config->unlock_savedata && !ranged) unlock_param_sfo(param, (unsigned char*)buffer, (u16)read_e);
						#endif
						if(send(conn_s, buffer, (size_t)read_e, 0) != (int)read_e) {keep_alive = 0; break;} // fork (6.7): a partial send is an error
						remaining -= read_e;
					}
					else
						break;
				}
				else
					break;

				//sys_ppu_thread_usleep(1668);
			}
			#ifdef USE_NTFS
			if(is_ntfs) ps3ntfs_close(fd);
			else
			#endif
			cellFsClose(fd);
		}

		goto exit_handleclient_www;
	}
