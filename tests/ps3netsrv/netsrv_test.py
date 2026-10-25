#!/usr/bin/env python3
"""Loopback tests for ps3netsrv (PC), through the netiso protocol, like a webMAN client.

  netsrv_test.py <ps3netsrv> <data-dir> md5      MD5 of an ISO and of the virtual ISO of a folder
  netsrv_test.py <ps3netsrv> <data-dir> fd0      without stdin: closing a virtual ISO must not break other sockets
  netsrv_test.py <ps3netsrv> <data-dir> bench <big-file>
                                                  MB/s and server CPU, on <big-file> and on the virtual ISOs
  netsrv_test.py <ps3netsrv> <data-dir> small     latency of 500 small READ_FILE requests (like a game list scan)

<data-dir> is created on the first run (PS3ISO/t.iso, viso/ and viso-small/ with 4,000 files, random data)
and reused afterwards, so the MD5s are comparable between builds. For bench, <big-file> is hard-linked
into <data-dir>, so it must be on the same filesystem.
"""
import hashlib
import os
import socket
import struct
import subprocess
import sys
import time

PORT = 38019
CHUNK = 65536  # what webMAN asks for in one read (netclient.h)
OPEN_FILE, READ_FILE = 0x1224, 0x1227


class Client:
    # ps3netsrv keeps one connection per IP (main.cpp:2216), so different clients use different loopback addresses
    def __init__(self, ip="127.0.0.1"):
        self.s = socket.create_connection(("127.0.0.1", PORT), timeout=30, source_address=(ip, 0))

    def recv(self, n):
        buf = bytearray()
        while len(buf) < n:
            b = self.s.recv(n - len(buf))
            if not b:
                raise ConnectionError("connection closed by the server")
            buf += b
        return bytes(buf)

    def open(self, path):
        p = path.encode()
        self.s.sendall(struct.pack(">HH12x", OPEN_FILE, len(p)) + p)
        size, _mtime = struct.unpack(">qQ", self.recv(16))
        return size

    def read(self, offset, n):
        self.s.sendall(struct.pack(">HxxIQ", READ_FILE, n, offset))
        (got,) = struct.unpack(">i", self.recv(4))
        if got < 0:
            raise IOError(f"READ_FILE returned {got} at offset {offset}")
        return self.recv(got)

    def md5(self, path):
        size, h, off = self.open(path), hashlib.md5(), 0
        while off < size:
            d = self.read(off, min(CHUNK, size - off))
            if not d:
                raise IOError(f"empty read at {off} of {size}")
            if off < 0x10000 and path.startswith("/***"):
                d = without_volume_dates(off, d)
            h.update(d); off += len(d)
        return h.hexdigest(), size

    def close(self):
        self.s.close()


def without_volume_dates(off, d):
    """Zeroes the creation/modification dates in the ISO 9660 volume descriptors (sectors 16+): the virtual ISO
    takes them from the server clock, so otherwise the MD5 would change from one minute to the next."""
    d = bytearray(d)
    for sec in range(0x8000, 0x10000, 0x800):
        a, b = max(sec + 813, off), min(sec + 847, off + len(d))
        if a < b:
            d[a - off:b - off] = bytes(b - a)
    return bytes(d)


def make_data(d):
    """Test data: a 48 MB ISO, a folder with files of different sizes, and a folder with many small files."""
    iso, viso, small = os.path.join(d, "PS3ISO", "t.iso"), os.path.join(d, "viso"), os.path.join(d, "viso-small")
    if not os.path.exists(iso):
        os.makedirs(os.path.dirname(iso)); os.makedirs(os.path.join(viso, "SUB"))
        with open(iso, "wb") as f:
            f.write(os.urandom(48 << 20))
        for name, n in [("A.BIN", 3 << 20), ("B.BIN", 1), ("C.BIN", 2048), ("SUB/D.BIN", (17 << 20) + 5)]:
            with open(os.path.join(viso, name), "wb") as f:
                f.write(os.urandom(n))
    if not os.path.exists(small):
        # like a JB game: thousands of small files, where VIsoFile reopens the file on every read
        for i in range(4000):
            sub = os.path.join(small, f"D{i // 500:02d}")
            os.makedirs(sub, exist_ok=True)
            with open(os.path.join(sub, f"F{i:05d}.BIN"), "wb") as f:
                f.write(os.urandom(16384))


def start(binary, root, no_stdin=False):
    p = subprocess.Popen([binary, root, str(PORT)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                         stdin=subprocess.DEVNULL, close_fds=True,
                         preexec_fn=(lambda: os.close(0)) if no_stdin else None)
    for _ in range(50):
        try:
            # from a different address than the clients: see #13 (race on reconnect from the same IP)
            socket.create_connection(("127.0.0.1", PORT), timeout=1, source_address=("127.0.0.3", 0)).close()
            return p
        except OSError:
            time.sleep(0.1)
    p.kill()
    sys.exit("the server did not start")


def cpu(pid):
    with open(f"/proc/{pid}/stat") as f:
        c = f.read().rsplit(")", 1)[1].split()
    return (int(c[11]) + int(c[12])) / os.sysconf("SC_CLK_TCK")


def main():
    binary, d, test = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]), sys.argv[3]
    make_data(d)
    p = start(binary, d, no_stdin=(test == "fd0"))
    try:
        if test == "md5":
            c = Client()
            print("iso ", *c.md5("/PS3ISO/t.iso"))
            print("viso", *c.md5("/***DVD***/viso"))
            c.close()
        elif test == "fd0":
            # A: virtual ISO, read until it enters a file (VIsoFile2 calls close_file(fd) before open),
            # then /CLOSEFILE (the destructor calls close_file(fd) once more).
            a = Client()
            size = a.open("/***DVD***/viso")
            off = 0
            while off < size:
                off += len(a.read(off, CHUNK))
            a.open("/CLOSEFILE")
            # B: a new connection must still work, and so must A.
            try:
                b = Client("127.0.0.2")
                ok_b = b.open("/PS3ISO/t.iso") == 48 << 20
                b.close()
            except OSError as e:
                ok_b = False; print("B:", e)
            try:
                ok_a = a.open("/PS3ISO/t.iso") == 48 << 20
            except OSError as e:
                ok_a = False; print("A:", e)
            a.close()
            print("fd0:", "OK" if ok_a and ok_b else "FAILED", f"(A {'ok' if ok_a else 'broken'}, B {'ok' if ok_b else 'refused'})")
            sys.exit(0 if ok_a and ok_b else 1)
        elif test == "small":
            import random
            random.seed(1)
            c = Client()
            size = c.open("/PS3ISO/t.iso")
            t0 = time.monotonic()
            for _ in range(500):
                off = random.randrange(0, size - 1092)
                assert len(c.read(off, 1092)) == 1092
            dt = time.monotonic() - t0
            print(f"small reads: 500 x 1092 bytes in {dt:.3f} s ({dt / 500 * 1000:.3f} ms each)")
            c.close()
        elif test == "bench":
            big = os.path.abspath(sys.argv[4])
            link = os.path.join(d, "big.bin")
            if not os.path.exists(link):
                os.link(big, link)
            for i, (name, path) in enumerate([("file", "/big.bin"), ("viso", "/***DVD***/viso"), ("viso-small", "/***DVD***/viso-small")]):
                c = Client(f"127.0.0.{10 + i}")
                t0, c0 = time.monotonic(), cpu(p.pid)
                h, size = c.md5(path)
                dt, dc = time.monotonic() - t0, cpu(p.pid) - c0
                print(f"{name:10s} {size / dt / 1e6:8.1f} MB/s  server CPU {dc:6.2f} s  ({size} bytes, md5 {h})")
                c.close()
    finally:
        p.kill(); p.wait()


if __name__ == "__main__":
    main()
