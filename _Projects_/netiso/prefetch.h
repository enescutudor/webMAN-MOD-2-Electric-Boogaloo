// Fork (S11, N1): speculative read-ahead for sequential ISO reads.
//
// Cobra sends one read at a time and waits for it, so every 256 KB block pays a full network turnaround.
// After two consecutive sequential reads, the request for the next block is sent as soon as the current one has
// arrived, so the server is already reading and sending it while Cobra and the game consume the current block.
// If Cobra then asks for something else, the pending answer is read and discarded first, and speculation stops
// until reads are sequential again. Cobra always receives exactly the bytes it asked for.
//
// The protocol is unchanged: ps3netsrv processes commands in order, so a command sent early just waits its turn.
// Only used for the plain ISO path (not for the PSX/CD paths, which issue other commands on the same socket).

static u64 prefetch_next;      // offset that would continue the last read
static u8 prefetch_seq;        // consecutive sequential reads seen (0..2)
static u8 prefetch_pending;    // a speculative request is in flight
static u64 prefetch_offset;    // ... for this offset
static u32 prefetch_size;      // ... and this size
static u8 prefetch_drain[16 * 1024];

static int send_read_critical(u64 offset, u32 size)
{
	netiso_read_file_critical_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = NETISO_CMD_READ_FILE_CRITICAL;
	cmd.num_bytes = size;
	cmd.offset = offset;

	return (send(g_socket, &cmd, sizeof(cmd), 0) == sizeof(cmd)) ? 0 : FAILED;
}

// Reads and discards the answer of the speculative request.
static int drain_prefetch(void)
{
	for(u32 left = prefetch_size; left;)
	{
		int n = (left < sizeof(prefetch_drain)) ? (int)left : (int)sizeof(prefetch_drain);
		if(recv(g_socket, prefetch_drain, n, MSG_WAITALL) != n) return FAILED;
		left -= n;
	}
	prefetch_pending = 0;
	return 0;
}

// Same contract as read_remote_file_critical().
static int read_iso_prefetch(u64 offset, void *buf, u32 size)
{
	if(prefetch_pending && ((prefetch_offset != offset) || (prefetch_size != size)))
	{
		prefetch_seq = 0;
		if(drain_prefetch() != 0) return FAILED;
	}

	if(!prefetch_pending && (send_read_critical(offset, size) != 0)) return FAILED;
	prefetch_pending = 0;

	if(recv(g_socket, buf, size, MSG_WAITALL) != (int)size) return FAILED;

	prefetch_seq = (offset == prefetch_next) ? ((prefetch_seq < 2) ? prefetch_seq + 1 : 2) : 0;
	prefetch_next = offset + size;

	// speculate only on whole blocks inside the disc: reads at the end are cut and zero-filled by the caller
	if((prefetch_seq >= 2) && (prefetch_next + size <= discsize) && (send_read_critical(prefetch_next, size) == 0))
	{
		prefetch_pending = 1;
		prefetch_offset = prefetch_next;
		prefetch_size = size;
	}

	return 0;
}
