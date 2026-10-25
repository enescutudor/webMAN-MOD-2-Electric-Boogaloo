// Fork (S16, G9, opt-in): skip the game scan at boot when nothing it reads has changed.
//
// With the line "bootscan=fast" in wmfork.cfg, every full scan first takes a signature of what decides the game
// list: the webMAN settings, the profile, and the entries (name, type, size, date) of the content folders (PS3ISO,
// GAMES, ...) on each enabled drive and ps3netsrv server. The signature is saved when the scan has rewritten
// mygames.xml. At boot, if mygames.xml exists and the signature is the same, the scan is skipped, like with the
// upstream option "scan on startup disabled". Without the line nothing changes.
//
// Not seen (by design): changes inside a game folder or inside subfolders of the local content folders (ROMS/<system>/
// is seen). With an NTFS drive attached, or a server that does not answer, the boot scan always runs. A game running
// from a server makes the signature unknown (no second connection to that server, see net_guard.h), so it is not
// saved. /refresh.ps3 rescans as always.

#ifndef WMFORK_CFG
#define WMFORK_CFG		"/dev_hdd0/tmp/wmfork.cfg"
#endif
#define BOOTSCAN_SIG	TMP_DIR "/wmfork-scan.sig"


static const char *bootscan_dirs[] = {"GAMES", "GAMEZ", "PS3ISO", "BDISO", "DVDISO", "PS2ISO", "PSXISO", "PSXGAMES", "PSPISO",
									  "ISO", "video", "GAMEI", "ROMS", CUSTOM_PATH1, CUSTOM_PATH2};

static bool bootscan_enabled(void)
{
	char cfg[512], value[8];
	size_t len = read_file(WMFORK_CFG, cfg, sizeof(cfg) - 1, 0); cfg[len] = '\0';
	return line_value(cfg, "bootscan", '=', value, sizeof(value)) && IS(value, "fast");
}

static u64 sig_add(u64 h, const void *data, u32 len)
{
	const u8 *b = (const u8 *)data;
	while(len--) {h ^= *b++; h *= 0x100000001B3ULL;} // FNV-1a
	return h;
}

// Entries of a local folder; with depth, also the entries of its subfolders.
static u64 sig_local_dir(u64 h, char *path, u8 depth)
{
	int fd;
	h = sig_add(h, path, strlen(path) + 1);
	if(cellFsOpendir(path, &fd) != CELL_FS_SUCCEEDED) return sig_add(h, "-", 1);

	u16 plen = strlen(path);
	CellFsDirectoryEntry entry; u32 read_e;
	while(working && !cellFsGetDirectoryEntries(fd, &entry, sizeof(entry), &read_e) && read_e)
	{
		if(entry.entry_name.d_name[0] == '.') continue;

		h = sig_add(h, entry.entry_name.d_name, entry.entry_name.d_namlen);
		h = sig_add(h, &entry.attribute.st_mode, sizeof(entry.attribute.st_mode));
		h = sig_add(h, &entry.attribute.st_size, sizeof(entry.attribute.st_size));
		h = sig_add(h, &entry.attribute.st_mtime, sizeof(entry.attribute.st_mtime));

		if(depth && ENTRY_IS_DIR(entry) && (plen + entry.entry_name.d_namlen + 2 < STD_PATH_LEN))
		{
			sprintf(path + plen, "/%s", entry.entry_name.d_name);
			h = sig_local_dir(h, path, depth - 1);
			path[plen] = '\0';
		}
	}
	cellFsClosedir(fd);
	return h;
}

#ifdef NET_SUPPORT
// Entries of the content folders of a ps3netsrv server, listed like the scan does (with subfolders for the ISO
// folders). Returns false if the server cannot be used now.
static bool sig_net_server(u64 *h, u8 server)
{
	if(is_game_running_from_server(server)) return false; // no 2nd connection to the server of the running game

	int ns = connect_to_server_ex(webman_config->neth[server], webman_config->netp[server], 5);
	if(ns < 0) return false;
	set_tcp_nodelay(ns);

	char path[64]; int abort_connection = 0;
	for(u8 d = 0; d < 13; d++)
	{
		if((d == 9) || (d == 10) || (d == 7)) continue; // ISO, video, PSXGAMES are not scanned on servers
		bool jb = (d < 2) || (d == 11);
		for(u8 p = 0; p < 5; p++)
		{
			sprintf(path, "/%s%s", bootscan_dirs[d], SUFIX(p));
			*h = sig_add(*h, path, strlen(path) + 1);
			if(open_remote_dir(ns, path, &abort_connection, !jb) < 0) {*h = sig_add(*h, "-", 1); continue;}

			sys_addr_t data2 = NULL;
			int count = read_remote_dir(ns, &data2, &abort_connection);
			if(abort_connection) {if(data2) sys_memory_free(data2); sclose(&ns); return false;}
			if(!data2) continue;

			netiso_read_dir_result_data *data = (netiso_read_dir_result_data *)data2;
			for(int i = 0; i < count; i++)
			{
				*h = sig_add(*h, data[i].name, strlen(data[i].name));
				*h = sig_add(*h, &data[i].file_size, sizeof(data[i].file_size));
				*h = sig_add(*h, &data[i].mtime, sizeof(data[i].mtime));
				*h = sig_add(*h, &data[i].is_directory, sizeof(data[i].is_directory));
			}
			sys_memory_free(data2);
		}
	}
	sclose(&ns);
	return true;
}
#endif

// 0 = no signature (option off, NTFS drive attached, server not usable).
static u64 bootscan_signature(void)
{
	if(!bootscan_enabled()) return 0;

	#ifdef USE_NTFS
	if(webman_config->ntfs)
	{
		if(mountCount <= NTFS_UNMOUNTED) mount_all_ntfs_volumes();
		if(mountCount > 0) return 0;
	}
	#endif

	u64 h = 0xCBF29CE484222325ULL;
	h = sig_add(h, webman_config, sizeof(WebmanCfg));
	h = sig_add(h, &profile, sizeof(profile));

	char path[STD_PATH_LEN];
	for(u8 f0 = 0; f0 < MAX_DRIVES; f0++)
	{
		if((f0 == NTFS) || check_drive(f0)) continue;

		#ifdef NET_SUPPORT
		if(IS_NET) {if(!sig_net_server(&h, f0 - NET)) return 0; continue;}
		#else
		if(IS_NET) continue;
		#endif

		if(!isDir(drives[f0])) {h = sig_add(h, drives[f0], strlen(drives[f0]) + 1); continue;}

		for(u8 d = 0; d < sizeof(bootscan_dirs) / sizeof(bootscan_dirs[0]); d++)
			for(u8 p = 0; p < 5; p++)
				for(u8 ps3 = 0; ps3 < (f0 ? 2 : 1); ps3++) // USB and cards: also /PS3/<folder>
				{
					sprintf(path, "%s%s/%s%s", drives[f0], ps3 ? "/PS3" : "", bootscan_dirs[d], SUFIX(p));
					h = sig_local_dir(h, path, (d == 12) ? 1 : 0); // ROMS/<system>/
				}

		if(IS_HDD0 && webman_config->npdrm) {strcpy(path, "/dev_hdd0/game"); h = sig_local_dir(h, path, 0);}
	}
	return h ? h : 1;
}

// At boot: true if the game list is still valid.
static bool bootscan_unchanged(void)
{
	if(not_exists(MY_GAMES_XML)) return false;

	char saved[24]; size_t len = read_file(BOOTSCAN_SIG, saved, sizeof(saved) - 1, 0); saved[len] = '\0';
	if(len != 16) return false;

	u64 sig = bootscan_signature(); if(!sig) return false;
	char now[24]; sprintf(now, "%016llx", sig);
	return IS(saved, now);
}

static u64 bootscan_mtime(void)
{
	struct CellFsStat s;
	return (cellFsStat(MY_GAMES_XML, &s) == CELL_FS_SUCCEEDED) ? s.st_mtime : 0;
}

// After a full scan: keep the signature taken before it, only if the scan rewrote mygames.xml.
static void bootscan_save(u64 sig, u64 mtime_before)
{
	cellFsUnlink(BOOTSCAN_SIG);
	if(!sig || !refreshing_xml) return;

	u64 mtime = bootscan_mtime();
	if(!mtime || (mtime == mtime_before)) return;

	char text[24]; sprintf(text, "%016llx", sig);
	save_file(BOOTSCAN_SIG, text, SAVE_ALL);
}
