# PC harness (gcc + ASan/UBSan) for the pure functions in webMAN.
#   make -C tests/host test
SRC := ../..
CFLAGS := -std=gnu99 -g -O1 -Wall -Wno-unused-function -fno-omit-frame-pointer \
	-fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=undefined
# alignment: memcpy64 copies through unaligned u64*, which the PPU allows

EXTRACT := \
	include/init/compare.h:IS,_IS,islike \
	include/init/string.h:strcopy,strncopy,memcpy64 \
	include/init/eval.h:get_value,get_param \
	include/init/html.h:htmlenc \
	include/file/file.h:get_ext,normalized_len,normalize_path \
	include/ftp.h:parsePath,absPath,ssplit \
	include/mount/cue_file.h:get_line \
	include/fork/http_cache.h:line_value,http_etag,http_status,parse_u64,http_range,http_cache_control

.PHONY: test clean
test: t
	./run.sh ./t known-bugs.txt

t: tests.c shim.h test.h gen/webman.inc gen/consts.h
	$(CC) $(CFLAGS) -o $@ tests.c

gen/webman.inc: extract.sh $(foreach e,$(EXTRACT),$(SRC)/$(firstword $(subst :, ,$(e))))
	@mkdir -p gen
	@: > $@.tmp
	@for e in $(EXTRACT); do ./extract.sh $(SRC)/$${e%%:*} $$(echo $${e#*:} | tr , ' ') >> $@.tmp || exit 1; done
	@mv $@.tmp $@

gen/consts.h: $(SRC)/include/init/paths.h
	@mkdir -p gen
	LC_ALL=C grep -E '^#define[[:space:]]+(MAX_LINE_LEN|STD_PATH_LEN|MAX_PATH_LEN)[[:space:]]' $< > $@

clean:
	rm -rf t gen
