# webMAN MOD 2: Electric Boogaloo, developer notes

Fork of [aldostools/webMAN-MOD](https://github.com/aldostools/webMAN-MOD), based on upstream `70ad8ef` and merged
forward from upstream. The fork's work lives on the `fork/main` branch; `master` follows upstream.

## Ground rule: the low-level zone is never touched

Files that deal with flash, EPROM, LV1/LV2, Cobra syscalls, the system registry or the fan **are not modified**
in the fork. They only come in unchanged from upstream.

- `PROTECTED_PATHS.txt`: red zone. A commit that touches it is refused.
- `tools/amber-paths.txt`: amber zone (code one step away from system writes or syscalls). A commit that
  touches it needs an `Impact low-level` section in its message, saying what changes and why it is safe.
- The `PS3MAPI_REENABLE_SYSCALL8` line in `include/www/www_page.h` must stay exactly as upstream.

```sh
git config core.hooksPath tools/hooks   # once after cloning: enables the pre-commit and commit-msg hooks
tools/check-protected.sh                 # the whole fork against upstream/master (exit 1 if the red zone is touched)
```

Merges from upstream pass: they bring upstream changes unmodified.

## Where the fork's code is

New code goes in `include/fork/`, hooked into upstream files with one-line changes:

| File | What |
|---|---|
| `http_cache.h` | ETag/304 and Cache-Control for files served from disk, CORS allow-list |
| `ftp_list.h` | buffered FTP LIST/MLSD/NLST |
| `net_guard.h` | no scan of the network server of the running game |

The mount plugin netiso.sprx (`_Projects_/netiso`, amber zone) has its own fork files: `prefetch.h` (read-ahead for
sequential ISO reads) and `reconnect.h` (timeouts, reconnection and one retry when the connection to ps3netsrv is lost).

The fork's own options live in `/dev_hdd0/tmp/wmfork.cfg` (one `key=value` per line, never in `webman_config`):
`cors=<origin>` allows that origin to call webMAN from a browser.

## Build

With the Cell SDK (`CELL_SDK` set, `ppu-lv2-*` in `PATH`) and `scetool` in `PATH`:

```sh
./_Make.sh                      # the default edition (Full); stops if make fails
tools/build-matrix.sh build/m   # Full, Lite, non-Cobra and Rebug PS3MAPI, in temporary copies (flags.h untouched)
tools/build-jqui-sortable.sh    # rebuilds the sortable-only jquery-ui.min.js (needs esbuild)
tools/build-plugins.sh          # netiso.sprx and raw_iso.sprx (loaded by webMAN from /dev_hdd0/tmp/wm_res)
```

ps3netsrv (PC): `make -f Makefile.linux` in `_Projects_/ps3netsrv` (now `-O2`, `VIsoFile2`).

## Tests

```sh
make -C tests/host test                                    # PC harness: gcc + ASan/UBSan (+ pthreads)
python3 tests/ps3netsrv/netsrv_test.py <ps3netsrv> <dir> md5|fd0|small|traversal|bench <big-file>
```

- `tests/host` extracts the pure functions from the real sources on every run, so it always tests the current
  code (38 checks). It also covers the fork's own modules: HTTP cache/range helpers, FTP list buffer, the
  system-path and CSRF-origin helpers, and the HTML-escape and buffer-bound fixes.
- Tests listed in `tests/host/known-bugs.txt` reproduce known upstream bugs that are not fixed yet and are
  expected to fail (XFAIL); when a fix makes one pass, the suite asks for it to be removed from the list.
- `tests/web/sortable.html` checks the game list drag & drop in a headless browser.

On the console, test builds are hot-loaded over the official plugin with `/reloadprx.ps3` and checked with a
smoke test (web, FTP, file manager, XMB game list, network mount) before and after every change; a reboot
always returns to the official build.

## Measurements (real console, XMB, internal HDD, 4 GB file, MD5 verified)

| | upstream (B0) | fork |
|---|---|---|
| Game list, first visit | 2.11 s, 6.76 MB | 1.68 s |
| Game list, second visit | 2.10 s, 6.76 MB | ~1.0 s, ~35 KB |
| `/refresh.ps3` game list scan (82 net games) | 3.9 s | 2.2 s |
| `/stat.ps3` of 20,000 files | 14.1-14.6 s | 6.9-7.5 s |
| Wildcard listing of 20,000 files | 57-62 s | ~7.0 s |
| FTP LIST of 5,000 files | 4.2-4.5 s | 3.4-3.65 s |
| FTP 200 small-file transfers (FileZilla-style) | 10.8 s | 2.3 s |
| HTTP GET | 32.1 MB/s | unchanged |
| In-game game list, first visit | 2.23 s | 2.30 s (within noise; the 5 ms pause per file is kept in-game) |
| Network game read (1.1 GB file, netiso) | 19.4 MB/s | 20.6 MB/s |
| Game reads after the server restarts mid-game | 0 MB (data source lost) | continues (~100 MB in 40 s) |

Game list scans and file listings/copies produce output identical to upstream (verified with `diff` on the
XMB XML and the HTML pages, and MD5 on copies); only the timing changes.

## Security

The web, FTP, netiso and PS3MAPI services are reachable from the local network, so the fork hardens them while
keeping existing clients (the file manager, FileZilla, an SFTP relay, PS3 Manager API) working:

- **ps3netsrv (PC):** rejects every `..` path component (path traversal), warns at startup when no IP allow-list
  is set, and has a read-only build target (`make -f Makefile.linux ro`). A loopback regression test
  (`tests/ps3netsrv/netsrv_test.py traversal`) proves the escape on upstream and its absence on the fork.
- **Console:** bounds/terminators on the paths that parse request lines, file names from a network server,
  `.cue` files, PARAM.SFO titles and popup URLs; HTML pages escape special characters in titles.
- **CSRF:** state-changing web commands are refused for cross-site browser requests (by `Origin`/`Referer`);
  requests with neither header, and localhost, are never restricted.
- **FTP:** the data connection is bound to the control client's IP, and failed `PASS` attempts are throttled.
- **PS3MAPI:** optional IP allow-list via `wmfork.cfg` (`ps3mapi_allow=`), off by default.

localhost (`127.*`) is exempt from every access restriction above. Optional switches live in
`/dev_hdd0/tmp/wmfork.cfg` (`cors=`, `bootscan=fast`, `ps3mapi_allow=`); nothing is enabled without it.
