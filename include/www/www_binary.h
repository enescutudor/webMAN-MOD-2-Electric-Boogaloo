	if(is_binary == BINARY_FILE) // binary file
	{
		// fork (W1): ETag + Cache-Control; 304 without body if the browser already has this version
		char etag[40];
		bool not_modified = http_etag(etag, c_mtime, c_len) && IS(if_none_match, etag);
		if(*etag) header_len += sprintf(header + header_len, "ETag: %s\r\nCache-Control: %s\r\n", etag, http_cache_control(param));

		size_t buffer_size = 0; if(sysmem) sys_memory_free(sysmem); sysmem = NULL;

		if(not_modified)
			header_len = http_status_304(header, header_len);
		else
		{
			// fork (W8): allocate before sending the headers, so a failed allocation gets a clean 503
			u8 npages = c_len ? (u8)MIN(MAX_PAGES, 1 + ((c_len - 1) / _64KB_)) : 1;

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

		header_len += sprintf(header + header_len, "Content-Length: %llu\r\n\r\n", (unsigned long long)c_len);
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
			u64 read_e = 0, pos;

			#ifdef USE_NTFS
			if(is_ntfs) ps3ntfs_seek64(fd, 0, SEEK_SET);
			else
			#endif
			cellFsLseek(fd, 0, CELL_FS_SEEK_SET, &pos);

			while(working)
			{
				#ifdef USE_NTFS
				if(is_ntfs) read_e = ps3ntfs_read(fd, (void *)buffer, buffer_size);
				#endif
				if(is_ntfs || cellFsRead(fd, (void *)buffer, buffer_size, &read_e) == CELL_FS_SUCCEEDED)
				{
					if(read_e > 0)
					{
						#ifdef UNLOCK_SAVEDATA
						if(webman_config->unlock_savedata) unlock_param_sfo(param, (unsigned char*)buffer, (u16)read_e);
						#endif
						if(send(conn_s, buffer, (size_t)read_e, 0) < 0) break;
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
