// Fork (7c.3a): while a game runs from network server <id>, a scan of that server would open a second connection
// from the same IP, and ps3netsrv (C++) closes the older one, which is the game's connection (main.cpp:2216);
// netiso.sprx does not reconnect, so the game gets read errors. That server is skipped until the game exits.
// The other servers and the local drives are scanned normally.

#ifdef NET_SUPPORT
static bool is_game_running_from_server(u8 server_id)
{
	if(!IS_INGAME) return false;

	unsigned int real_disctype, effective_disctype, iso_disctype;
	cobra_get_disc_type(&real_disctype, &effective_disctype, &iso_disctype);
	if(iso_disctype == DISC_TYPE_NONE) return false;

	char last_game[16];
	if(read_file(LAST_GAME_TXT, last_game, sizeof(last_game) - 1, 0) < 6) return false;
	last_game[sizeof(last_game) - 1] = '\0';

	return islike(last_game, "/net") && ((u8)(last_game[4] - '0') == server_id) && (last_game[5] == '/');
}
#endif
