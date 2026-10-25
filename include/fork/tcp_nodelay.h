// Fork (S14, N5): no Nagle delay on the requests to ps3netsrv.
//
// netclient sends a command and its path with two send() calls. With Nagle on, the path waits until the server
// ACKs the command, and the server delays that ACK (~40 ms) because it is still waiting for the path. Measured on
// a game list scan: 42 ms per stat or open_dir before, ~1 ms after. The bytes on the wire are the same.

#include <netinet/tcp.h>

static void set_tcp_nodelay(int s)
{
	int one = 1;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
}
