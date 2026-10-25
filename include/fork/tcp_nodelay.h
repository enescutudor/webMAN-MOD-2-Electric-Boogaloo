// Fork: no Nagle delay where two small writes follow each other without an answer in between. With Nagle on,
// the second write waits until the peer ACKs the first one, and the peer delays that ACK (~40 ms). The bytes on
// the wire are the same.
// - S14, N5: requests to ps3netsrv (command, then path): 42 ms -> ~1 ms per stat or open_dir in a game list scan.
// - S15: FTP control connection ("150", then "226" after the transfer): 53 ms -> ~11 ms per small file.

#include <netinet/tcp.h>

static void set_tcp_nodelay(int s)
{
	int one = 1;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
}
