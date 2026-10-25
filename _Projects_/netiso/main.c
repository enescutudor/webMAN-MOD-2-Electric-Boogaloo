#include <sdk_version.h>
#include <cellstatus.h>
#include <cell/cell_fs.h>

#include <sys/prx.h>
#include <sys/ppu_thread.h>
#include <sys/event.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/memory.h>
#include <sys/ss_get_open_psid.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netex/net.h>
#include <netex/errno.h>

#define SC_COBRA_SYSCALL8				(8)
#define SC_GET_PRX_MODULE_BY_ADDRESS	(461)
#define SC_STOP_PRX_MODULE				(482)
#define SC_PPU_THREAD_EXIT				(41)

#define NONE -1
#define SYS_PPU_THREAD_NONE        (sys_ppu_thread_t)NONE
#define SYS_EVENT_QUEUE_NONE       (sys_event_queue_t)NONE

#define FAILED -1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "types.h"
#include "common.h"
#include "netiso.h"
#include "syscall8.h"
#include "scsi.h"

SYS_MODULE_INFO(NETISO, 0, 1, 1);
SYS_MODULE_START(netiso_start);
SYS_MODULE_STOP(netiso_stop);

#define THREAD_NAME "sm_netm"
#define STOP_THREAD_NAME "sm_nets"

#define MIN(a, b)	((a) <= (b) ? (a) : (b))
#define ABS(a)		(((a) < 0) ? -(a) : (a))

#define CD_CACHE_SIZE			(48)

#define ATA_HDD			0x101000000000007ULL
#define BDVD_DRIVE		0x101000000000006ULL
#define PATA0_HDD_DRIVE		0x101000000000008ULL
#define PATA0_BDVD_DRIVE	BDVD_DRIVE
#define PATA1_HDD_DRIVE		ATA_HDD
#define PATA1_BDVD_DRIVE	0x101000000000009ULL
#define BUILTIN_FLASH		0x100000000000001ULL
#define MEMORY_STICK		0x103000000000010ULL
#define SD_CARD			0x103000100000010ULL
#define COMPACT_FLASH		0x103000200000010ULL

#define USB_MASS_STORAGE_1(n)	(0x10300000000000AULL+n) /* For 0-5 */
#define USB_MASS_STORAGE_2(n)	(0x10300000000001FULL+(n-6)) /* For 6-127 */

#define	HDD_PARTITION(n)	(ATA_HDD | ((u64)n<<32))
#define FLASH_PARTITION(n)	(BUILTIN_FLASH | ((u64)n<<32))

#define DISC_TYPE_NONE		0

#define MAX_TRACKS	80

#define DEVICE_TYPE_PS3_DVD	0xFF70
#define DEVICE_TYPE_PS3_BD	0xFF71
#define DEVICE_TYPE_PS2_CD	0xFF60
#define DEVICE_TYPE_PS2_DVD	0xFF61
#define DEVICE_TYPE_PSX_CD	0xFF50
#define DEVICE_TYPE_BDROM	0x40
#define DEVICE_TYPE_BDMR_SR	0x41 /* Sequential record */
#define DEVICE_TYPE_BDMR_RR 	0x42 /* Random record */
#define DEVICE_TYPE_BDMRE	0x43
#define DEVICE_TYPE_DVD		0x10 /* DVD-ROM, DVD+-R, DVD+-RW etc, they are differenced by booktype field in some scsi command */
#define DEVICE_TYPE_CD		0x08 /* CD-ROM, CD-DA, CD-R, CD-RW, etc, they are differenced somehow with scsi commands */

#define PLAYSTATION      "PLAYSTATION "

enum EMU_TYPE
{
	EMU_OFF = 0,
	EMU_PS3,
	EMU_PS2_DVD,
	EMU_PS2_CD,
	EMU_PSX,
	EMU_BD,
	EMU_DVD,
	EMU_MAX,
};

enum STORAGE_COMMAND
{
	CMD_READ_ISO,
	CMD_READ_DISC,
	CMD_READ_CD_ISO_2352,
	CMD_FAKE_STORAGE_EVENT,
	CMD_GET_PSX_VIDEO_MODE
};

typedef struct
{
	char server[0x40];
	char path[0x420];
	u32 emu_mode;
	u32 numtracks;
	u16 port;
	u8 pad[6];
	ScsiTrackDescriptor tracks[MAX_TRACKS];
} __attribute__((packed)) netiso_args;

static sys_ppu_thread_t thread_id = 1;

static int g_socket = NONE;
static sys_event_queue_t command_queue = SYS_EVENT_QUEUE_NONE;

static u8 working = 1;
static u64 discsize;
static u64 cd_sec_size =  2352;
static int is_cd2352;
static u8 *cd_cache;
static u32 cached_cd_sector=0x80000000;

int netiso_start(u64 arg);
int netiso_stop(void);

static int sys_storage_ext_get_disc_type(unsigned int *real_disctype, unsigned int *effective_disctype, unsigned int *fake_disctype)
{
	system_call_4(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_GET_DISC_TYPE, (u64)(u32)real_disctype, (u64)(u32)effective_disctype, (u64)(u32)fake_disctype);
	return (int)p1;
}

static int sys_storage_ext_fake_storage_event(u64 event, u64 param, u64 device)
{
	system_call_4(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_FAKE_STORAGE_EVENT, event, param, device);
	return (int)p1;
}

static int sys_storage_ext_mount_discfile_proxy(sys_event_port_t result_port, sys_event_queue_t command_queue, int emu_type, u64 disc_size_bytes, u32 read_size, unsigned int trackscount, ScsiTrackDescriptor *tracks)
{
	system_call_8(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_MOUNT_DISCFILE_PROXY, result_port, command_queue, emu_type, disc_size_bytes, read_size, trackscount, (u64)(u32)tracks);
	return (int)p1;
}

static inline int sys_drm_get_data(void *data, u64 param)
{
	system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_DRM_GET_DATA, (u64)(u32)data, param);
	return (int)p1;
}

static inline int sys_storage_ext_umount_discfile(void)
{
	system_call_1(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_UMOUNT_DISCFILE);
	return (int)p1;
}

static inline void _sys_ppu_thread_exit(u64 val)
{
	system_call_1(SC_PPU_THREAD_EXIT, val);
}

static inline sys_prx_id_t prx_get_module_id_by_address(void *addr)
{
	system_call_1(SC_GET_PRX_MODULE_BY_ADDRESS, (u64)(u32)addr);
	return (int)p1;
}

static int fake_eject_event(void)
{
	sys_storage_ext_fake_storage_event(4, 0, BDVD_DRIVE);
	return sys_storage_ext_fake_storage_event(SC_COBRA_SYSCALL8, 0, BDVD_DRIVE);
}

static int fake_insert_event(u64 disctype)
{
	u64 param = (u64)(disctype) << 32ULL;
	sys_storage_ext_fake_storage_event(7, 0, BDVD_DRIVE);
	return sys_storage_ext_fake_storage_event(3, param, BDVD_DRIVE);
}

static void memcpy64(void *dst, void *src, int n)
{
	uint8_t p = n & 7;

	n >>= 3;
	uint64_t *d = (uint64_t *) dst;
	uint64_t *s = (uint64_t *) src;
	while (n--) *d++ = *s++;

	if(p)
	{
		char *m = (char *) d;
		char *c = (char *) s;
		while (p--) *m++ = *c++;
	}
}

static void set_socket_timeouts(int s); // fork (S11, 1b.2): reconnect.h

static int connect_to_server(char *server, u16 port)
{
	struct sockaddr_in sin;
	unsigned int temp;
	int s;

	if((temp = inet_addr(server)) != (unsigned int)-1)
	{
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = temp;
	}
	else
	{
		struct hostent *hp;

		if((hp = gethostbyname(server)) == NULL)
		{
			//DPRINTF("unknown host %s, %d\n", server, sys_net_h_errno);
			return NONE;
		}

		sin.sin_family = hp->h_addrtype;
		memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
	}

	sin.sin_port = htons(port);
	s = socket(AF_INET, SOCK_STREAM, 0);
	if(s < 0)
	{
		//DPRINTF("socket() failed (errno=%d)\n", sys_net_errno);
		return FAILED;
	}

	set_socket_timeouts(s); // fork (S11, 1b.2)

	if(connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		//DPRINTF("connect() failed (errno=%d)\n", sys_net_errno);
		socketclose(s);
		return FAILED;
	}

	//DPRINTF("connect ok\n");
	return s;
}

static int64_t open_remote_file(char *path)
{
	netiso_open_cmd cmd;
	netiso_open_result res;
	int len;

	len = strlen(path);

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = NETISO_CMD_OPEN_FILE;
	cmd.fp_len = len;

	if(send(g_socket, &cmd, sizeof(cmd), 0) != sizeof(cmd))
	{
		//DPRINTF("send failed (open_file) (errno=%d)!\n", sys_net_errno);
		return FAILED;
	}

	if(send(g_socket, path, len, 0) != len)
	{
		//DPRINTF("send failed (open_file) (errno=%d)!\n", sys_net_errno);
		return FAILED;
	}

	if(recv(g_socket, &res, sizeof(res), MSG_WAITALL) != sizeof(res))
	{
		//DPRINTF("recv failed (open_file) (errno=%d)!\n", sys_net_errno);
		return FAILED;
	}

	if(res.file_size == NONE)
	{
		//DPRINTF("Remote file %s doesn't exist!\n", path);
		return FAILED;
	}

	//DPRINTF("Remote file %s opened. Size = %x%08lx bytes\n", path, (res.file_size>>32), res.file_size&0xFFFFFFFF);
	return res.file_size;
}

static int read_remote_file_critical(u64 offset, void *buf, u32 size)
{
	netiso_read_file_critical_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = NETISO_CMD_READ_FILE_CRITICAL;
	cmd.num_bytes = size;
	cmd.offset = offset;

	if(send(g_socket, &cmd, sizeof(cmd), 0) != sizeof(cmd))
	{
		//DPRINTF("send failed (read file) (errno=%d)!\n", sys_net_errno);
		return FAILED;
	}

	if(recv(g_socket, buf, size, MSG_WAITALL) != (int)size)
	{
		//DPRINTF("recv failed (recv file)  (errno=%d)!\n", sys_net_errno);
		return FAILED;
	}

	return 0;
}

#include "prefetch.h"
#include "reconnect.h"

static int process_read_iso_cmd(u8 *buf, u64 offset, u32 size)
{
	u64 read_end;

	////DPRINTF("read iso: %p %lx %x\n", buf, offset, size);
	read_end = offset + size;

	if(read_end >= discsize)
	{
		//DPRINTF("Read beyond limits: %llx %x (discsize=%llx)!\n", offset, size, discsize);

		if(offset >= discsize)
		{
			memset(buf, 0, size);
			return 0;
		}

		memset(buf + (discsize - offset), 0, read_end - discsize);
		size = discsize - offset;
	}

	return is_cd2352 ? read_remote_file_critical(offset, buf, size) : read_iso_prefetch(offset, buf, size); // fork (S11, N1)
}

static int process_read_cd_2048_cmd(u8 *buf, u32 start_sector, u32 sector_count)
{
	netiso_read_cd_2048_critical_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = NETISO_CMD_READ_CD_2048_CRITICAL;
	cmd.start_sector = start_sector;
	cmd.sector_count = sector_count;

	if(send(g_socket, &cmd, sizeof(cmd), 0) != sizeof(cmd))
	{
		//DPRINTF("send failed (read 2048) (errno=%d)!\n", sys_net_errno);
		return FAILED;
	}

	if(recv(g_socket, buf, sector_count * 2048, MSG_WAITALL) != (int)(sector_count*2048))
	{
		//DPRINTF("recv failed (read 2048)  (errno=%d)!\n", sys_net_errno);
		return FAILED;
	}

	return 0;
}

static int process_read_cd_2352_cmd(u8 *buf, u32 sector, u32 remaining)
{
	int cache = 0;

	if(remaining <= CD_CACHE_SIZE)
	{
		int dif = (int)cached_cd_sector-sector;

		if(ABS(dif) < CD_CACHE_SIZE)
		{
			u8 *copy_ptr = NULL;
			u32 copy_offset = 0;
			u32 copy_size = 0;

			if(dif > 0)
			{
				if(dif < (int)remaining)
				{
					copy_ptr = cd_cache;
					copy_offset = dif;
					copy_size = remaining-dif;
				}
			}
			else
			{
				copy_ptr = cd_cache + ((-dif) * cd_sec_size);
				copy_size = MIN((int)remaining, CD_CACHE_SIZE + dif);
			}

			if(copy_ptr)
			{
				memcpy64(buf + (copy_offset * cd_sec_size), copy_ptr, copy_size * cd_sec_size);

				if(remaining == copy_size)
				{
					return 0;
				}

				remaining -= copy_size;

				if(dif <= 0)
				{
					u32 newsector = cached_cd_sector + CD_CACHE_SIZE;
					buf += ((newsector-sector) * cd_sec_size);
					sector = newsector;
				}
			}
		}

		cache = 1;
	}

	if(!cache)
	{
		return process_read_iso_cmd(buf, sector * cd_sec_size, remaining * cd_sec_size);
	}

	if(!cd_cache)
	{
		sys_addr_t addr;

		int ret = sys_memory_allocate(128 * 1024, SYS_MEMORY_PAGE_SIZE_64K, &addr);
		if(ret != CELL_OK)
		{
			//DPRINTF("sys_memory_allocate failed: %x\n", ret);
			return ret;
		}

		cd_cache = (u8 *)addr;
	}

	if(process_read_iso_cmd(cd_cache, sector * cd_sec_size, CD_CACHE_SIZE * cd_sec_size) != CELL_OK)
		return FAILED;

	memcpy64(buf, cd_cache, remaining * cd_sec_size);
	cached_cd_sector = sector;
	return 0;
}

static int detect_cd_sector_size(char *buffer)
{
	u16 sec_size[7] = {2352, 2048, 2336, 2448, 2328, 2340, 2368};
	for(u8 n = 0; n < 7; n++)
	{
		if( (!strncmp(buffer + ((sec_size[n]<<4) + 0x20), PLAYSTATION, 0xC)) ||
			(!strncmp(buffer + ((sec_size[n]<<4) + 0x19), "CD001", 5) && buffer[(sec_size[n]<<4) + 0x18] == 0x01) ) return sec_size[n];
	}

	return 2352;
}

static void netiso_thread(u64 arg)
{
	u8 buff[sizeof(netiso_args)]; // 1392 bytes
	u64 *argp = (u64*)(u32)arg;
	u64 *addr = (u64*)buff;

	for(u16 i = 0; i < sizeof(netiso_args)/8; i++) addr[i] = argp[i]; // copy arguments 64KB

	sys_event_port_t result_port;
	sys_event_queue_attribute_t queue_attr;
	unsigned int real_disctype;
	int ret;
	ScsiTrackDescriptor *tracks;
	int emu_mode, numtracks, cd_sec_size_param = 0;

	netiso_args *args = (netiso_args *)buff;

	g_socket = connect_to_server(args->server, args->port);
	if(g_socket < 0)
	{
		//sys_memory_free((sys_addr_t)args);
		sys_ppu_thread_exit(0);
	}

	int64_t size = open_remote_file(args->path);
	if(size < 0)
	{
		//sys_memory_free((sys_addr_t)args);
		sys_ppu_thread_exit(0);
	}

	emu_mode = args->emu_mode;

	discsize = (u64)size;

	if(!(discsize%2352)) cd_sec_size = 2352; else
	if(!(discsize%2336)) cd_sec_size = 2336; else
	if(!(discsize%2448)) cd_sec_size = 2448; else
	if(!(discsize%2048)) cd_sec_size = 2048; else cd_sec_size = 2352;

	// detect CD sector size
	if((emu_mode == EMU_PSX) && (discsize >= 65536) && (discsize <= 0x35000000UL))
	{
		sys_addr_t sysmem = NULL; u32 size = 65536;
		if(sys_memory_allocate(size, SYS_MEMORY_PAGE_SIZE_64K, &sysmem) == CELL_OK)
		{
			char *buffer = (char*)sysmem;

			int bytes_read;

			bytes_read = read_remote_file_critical(0, buffer, size);
			if(bytes_read)
			{
				cd_sec_size = detect_cd_sector_size(buffer);
			}

			sys_memory_free((sys_addr_t)sysmem);
		}
	}

	ret = sys_event_port_create(&result_port, 1, SYS_EVENT_PORT_NO_NAME);
	if(ret != CELL_OK)
	{
		//DPRINTF("sys_event_port_create failed: %x\n", ret);
		//sys_memory_free((sys_addr_t)args);
		sys_ppu_thread_exit(ret);
	}

	sys_event_queue_attribute_initialize(queue_attr);
	ret = sys_event_queue_create(&command_queue, &queue_attr, 0, 1);
	if(ret != CELL_OK)
	{
		//DPRINTF("sys_event_queue_create failed: %x\n", ret);
		//sys_memory_free((sys_addr_t)args);
		sys_ppu_thread_exit(ret);
	}

	sys_storage_ext_get_disc_type(&real_disctype, NULL, NULL);

	if(real_disctype != DISC_TYPE_NONE)
	{
		fake_eject_event();
	}

	if(emu_mode == EMU_PSX)
	{
		tracks = &args->tracks[0];
		numtracks = MIN(args->numtracks, MAX_TRACKS);

		is_cd2352 = 1;
		if(cd_sec_size & 0xf) cd_sec_size_param = cd_sec_size<<8;
		else cd_sec_size_param = (cd_sec_size<<4);
	}
	else
	{
		numtracks = 0;
		tracks = NULL;
		is_cd2352 = 0;
	}

	ret = sys_storage_ext_mount_discfile_proxy(result_port, command_queue, emu_mode, discsize, 256*1024, numtracks|cd_sec_size_param, tracks);
	//DPRINTF("mount = %x\n", ret);

	//sys_memory_free((sys_addr_t)args);
	fake_insert_event(real_disctype);

	if(ret != CELL_OK)
	{
		sys_event_port_destroy(result_port);
		sys_ppu_thread_exit(0);
	}

	while(working)
	{
		sys_event_t event;

		ret = sys_event_queue_receive(command_queue, &event, 0);
		if(ret != CELL_OK)
		{
			////DPRINTF("sys_event_queue_receive failed: %x\n", ret);
			break;
		}

		void *buf = (void *)(u32)(event.data3>>32ULL);
		u64 offset = event.data2;
		u32 size = event.data3&0xFFFFFFFF;

		for(int attempt = 0;; attempt++) // fork (S11, 1b.2): one retry on a new connection
		{
		switch(event.data1)
		{
			case CMD_READ_ISO:
			{
				if(is_cd2352)
				{
					ret = process_read_cd_2048_cmd(buf, offset/2048, size/2048);
				}
				else
				{
					ret = process_read_iso_cmd(buf, offset, size);
				}
			}
			break;

			case CMD_READ_CD_ISO_2352:
			{
				ret = process_read_cd_2352_cmd(buf, offset/cd_sec_size, size/cd_sec_size);
			}
			break;
		}
		if((ret == 0) || attempt || !working || !netiso_reconnect(args)) break;
		}

		ret = sys_event_port_send(result_port, ret, 0, 0);
		if(ret != CELL_OK)
		{
			//DPRINTF("sys_event_port_send failed: %x\n", ret);
			break;
		}
	}

	sys_storage_ext_get_disc_type(&real_disctype, NULL, NULL);
	fake_eject_event();
	sys_storage_ext_umount_discfile();

	if(real_disctype != DISC_TYPE_NONE)
	{
		fake_insert_event(real_disctype);
	}

	if(cd_cache)
	{
		sys_memory_free((sys_addr_t)cd_cache);
	}

	if(g_socket >= 0)
	{
		shutdown(g_socket, SHUT_RDWR);
		socketclose(g_socket);
		g_socket = NONE;
	}

	sys_event_port_disconnect(result_port);
	if(sys_event_port_destroy(result_port) != CELL_OK)
	{
		//DPRINTF("Error destroyng result_port\n");
	}

	//DPRINTF("Exiting main thread!\n");
	sys_ppu_thread_exit(0);
}

int netiso_start(u64 arg)
{
	if(arg)
	{
		sys_ppu_thread_create(&thread_id, netiso_thread, arg, -0x1d8, 0x2000, SYS_PPU_THREAD_CREATE_JOINABLE, THREAD_NAME);
	}

	// Exit thread using directly the syscall and not the user mode library or we will crash
	_sys_ppu_thread_exit(0);
	return SYS_PRX_RESIDENT;
}


static void netiso_stop_thread(u64 arg)
{
	//DPRINTF("netiso_stop_thread\n");
	working = 0;

	if(g_socket >= 0)
	{
		shutdown(g_socket, SHUT_RDWR);
		socketclose(g_socket);
		g_socket = NONE;
	}

	if(command_queue != SYS_EVENT_QUEUE_NONE)
	{
		if(sys_event_queue_destroy(command_queue, SYS_EVENT_QUEUE_DESTROY_FORCE) != CELL_OK)
		{
			//DPRINTF("Failed in destroying command_queue\n");
		}
	}

	if(thread_id != SYS_PPU_THREAD_NONE)
	{
		u64 exit_code;
		sys_ppu_thread_join(thread_id, &exit_code);
	}

	//DPRINTF("Exiting stop thread!\n");
	sys_ppu_thread_exit(0);
}

static void finalize_module(void)
{
	u64 meminfo[5];

	sys_prx_id_t prx = prx_get_module_id_by_address(finalize_module);

	meminfo[0] = 0x28;
	meminfo[1] = 2;
	meminfo[3] = 0;

	system_call_3(SC_STOP_PRX_MODULE, prx, 0, (u64)(u32)meminfo);
}

int netiso_stop(void)
{
	sys_ppu_thread_t t;
	u64 exit_code;

	sys_ppu_thread_create(&t, netiso_stop_thread, 0, 0, 0x2000, SYS_PPU_THREAD_CREATE_JOINABLE, STOP_THREAD_NAME);
	sys_ppu_thread_join(t, &exit_code);

	finalize_module();
	_sys_ppu_thread_exit(0);
	return SYS_PRX_STOP_OK;
}
