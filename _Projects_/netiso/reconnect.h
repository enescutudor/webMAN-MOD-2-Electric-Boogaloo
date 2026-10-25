// Fork (S11, 1b.2): timeouts and reconnection for the game's connection to ps3netsrv.
//
// Without a timeout, a server that stops answering leaves the game blocked in recv() forever; a dropped connection
// (server restart, network glitch, ps3netsrv closing an older connection from the same IP) makes every following
// read fail, because netiso never reconnected. Now a failed read closes the socket (a late answer would desync the
// stream), reconnects, reopens the same file, checks its size and retries that read once.

#include <sys/timer.h>
#include <sys/time.h>

#define NETISO_TIMEOUT_SEC		30	// generous: a sleeping server disk can take ~10-15 s to spin up
#define NETISO_RECONNECT_TRIES	3

static void set_socket_timeouts(int s)
{
	struct timeval tv;
	tv.tv_sec = NETISO_TIMEOUT_SEC;
	tv.tv_usec = 0;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

static void close_game_socket(void)
{
	if(g_socket >= 0)
	{
		shutdown(g_socket, SHUT_RDWR);
		socketclose(g_socket);
		g_socket = NONE;
	}
	prefetch_pending = 0;
	prefetch_seq = 0;
}

// Returns 1 when the same file is open again on a new connection.
static int netiso_reconnect(netiso_args *args)
{
	close_game_socket();

	for(int i = 0; (i < NETISO_RECONNECT_TRIES) && working; i++)
	{
		sys_timer_sleep(1);

		g_socket = connect_to_server(args->server, args->port);
		if(g_socket < 0) continue;

		if(open_remote_file(args->path) == (int64_t)discsize) return 1;

		close_game_socket();
	}
	return 0;
}
