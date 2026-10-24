#!/usr/bin/env python3
"""Teste pe loopback pentru ps3netsrv (PC), prin protocolul netiso, ca un client webMAN.

  netsrv_test.py <ps3netsrv> <dir-date> md5      MD5 pentru un ISO și pentru ISO-ul virtual al unui folder
  netsrv_test.py <ps3netsrv> <dir-date> fd0      fără stdin: închiderea unui ISO virtual nu strică alte socket-uri
  netsrv_test.py <ps3netsrv> <dir-date> bench <fișier-mare>
                                                  MB/s și CPU-ul serverului, pe <fișier-mare> și pe ISO-ul virtual

<dir-date> e creat la prima rulare (PS3ISO/t.iso, viso/ și viso-mic/ cu 4.000 de fișiere, date aleatoare)
și refolosit apoi, ca MD5-urile să fie comparabile între build-uri. La bench, <fișier-mare> e legat (hardlink)
în <dir-date>, deci trebuie să fie pe același sistem de fișiere.
"""
import hashlib
import os
import socket
import struct
import subprocess
import sys
import time

PORT = 38019
CHUNK = 65536  # cât cere webMAN la o citire (netclient.h)
OPEN_FILE, READ_FILE = 0x1224, 0x1227


class Client:
    # ps3netsrv ține o singură conexiune per IP (main.cpp:2216), deci clienți diferiți vin de la adrese loopback diferite
    def __init__(self, ip="127.0.0.1"):
        self.s = socket.create_connection(("127.0.0.1", PORT), timeout=30, source_address=(ip, 0))

    def recv(self, n):
        buf = bytearray()
        while len(buf) < n:
            b = self.s.recv(n - len(buf))
            if not b:
                raise ConnectionError("conexiune închisă de server")
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
            raise IOError(f"READ_FILE a întors {got} la offset {offset}")
        return self.recv(got)

    def md5(self, path):
        size, h, off = self.open(path), hashlib.md5(), 0
        while off < size:
            d = self.read(off, min(CHUNK, size - off))
            if not d:
                raise IOError(f"citire goală la {off} din {size}")
            if off < 0x10000 and path.startswith("/***"):
                d = fara_date_volum(off, d)
            h.update(d); off += len(d)
        return h.hexdigest(), size

    def close(self):
        self.s.close()


def fara_date_volum(off, d):
    """Golește datele de creare/modificare din descriptorii de volum ISO 9660 (sectoarele 16+): ISO-ul virtual
    le ia din ceasul serverului, deci altfel MD5-ul s-ar schimba de la un minut la altul."""
    d = bytearray(d)
    for sec in range(0x8000, 0x10000, 0x800):
        a, b = max(sec + 813, off), min(sec + 847, off + len(d))
        if a < b:
            d[a - off:b - off] = bytes(b - a)
    return bytes(d)


def date(d):
    """Datele de test: un ISO de 48 MB și un folder cu fișiere de dimensiuni diferite pentru ISO-ul virtual."""
    iso, viso, mic = os.path.join(d, "PS3ISO", "t.iso"), os.path.join(d, "viso"), os.path.join(d, "viso-mic")
    if not os.path.exists(iso):
        os.makedirs(os.path.dirname(iso)); os.makedirs(os.path.join(viso, "SUB"))
        with open(iso, "wb") as f:
            f.write(os.urandom(48 << 20))
        for nume, n in [("A.BIN", 3 << 20), ("B.BIN", 1), ("C.BIN", 2048), ("SUB/D.BIN", (17 << 20) + 5)]:
            with open(os.path.join(viso, nume), "wb") as f:
                f.write(os.urandom(n))
    if not os.path.exists(mic):
        # ca un joc JB: mii de fișiere mici, unde VIsoFile redeschide fișierul la fiecare citire
        for i in range(4000):
            sub = os.path.join(mic, f"D{i // 500:02d}")
            os.makedirs(sub, exist_ok=True)
            with open(os.path.join(sub, f"F{i:05d}.BIN"), "wb") as f:
                f.write(os.urandom(16384))


def porneste(binar, root, fara_stdin=False):
    p = subprocess.Popen([binar, root, str(PORT)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                         stdin=subprocess.DEVNULL, close_fds=True,
                         preexec_fn=(lambda: os.close(0)) if fara_stdin else None)
    for _ in range(50):
        try:
            # de la altă adresă decât clienții: vezi #13 (cursa la reconectarea de la același IP)
            socket.create_connection(("127.0.0.1", PORT), timeout=1, source_address=("127.0.0.3", 0)).close()
            return p
        except OSError:
            time.sleep(0.1)
    p.kill()
    sys.exit("serverul nu a pornit")


def cpu(pid):
    with open(f"/proc/{pid}/stat") as f:
        c = f.read().rsplit(")", 1)[1].split()
    return (int(c[11]) + int(c[12])) / os.sysconf("SC_CLK_TCK")


def main():
    binar, d, test = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]), sys.argv[3]
    date(d)
    p = porneste(binar, d, fara_stdin=(test == "fd0"))
    try:
        if test == "md5":
            c = Client()
            print("iso ", *c.md5("/PS3ISO/t.iso"))
            print("viso", *c.md5("/***DVD***/viso"))
            c.close()
        elif test == "fd0":
            # A: ISO virtual, citit până intră într-un fișier (VIsoFile2 face close_file(fd) înainte de open),
            # apoi /CLOSEFILE (destructorul face încă un close_file(fd)).
            a = Client()
            size = a.open("/***DVD***/viso")
            off = 0
            while off < size:
                off += len(a.read(off, CHUNK))
            a.open("/CLOSEFILE")
            # B: o conexiune nouă trebuie să funcționeze în continuare, la fel și A.
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
            print("fd0:", "OK" if ok_a and ok_b else "EȘEC", f"(A {'ok' if ok_a else 'stricat'}, B {'ok' if ok_b else 'refuzat'})")
            sys.exit(0 if ok_a and ok_b else 1)
        elif test == "bench":
            mare = os.path.abspath(sys.argv[4])
            legatura = os.path.join(d, "mare.bin")
            if not os.path.exists(legatura):
                os.link(mare, legatura)
            for i, (nume, cale) in enumerate([("fișier", "/mare.bin"), ("viso", "/***DVD***/viso"), ("viso-mic", "/***DVD***/viso-mic")]):
                c = Client(f"127.0.0.{10 + i}")
                t0, c0 = time.monotonic(), cpu(p.pid)
                h, size = c.md5(cale)
                dt, dc = time.monotonic() - t0, cpu(p.pid) - c0
                print(f"{nume:7s} {size / dt / 1e6:8.1f} MB/s  CPU server {dc:6.2f} s  ({size} octeți, md5 {h})")
                c.close()
    finally:
        p.kill(); p.wait()


if __name__ == "__main__":
    main()
