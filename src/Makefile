TARGETS = safefs


CC ?= gcc

CFLAGS = -Wall -g
CFLAGS_FUSE = -D_FILE_OFFSET_BITS=64
LIBFUSE_INCLUDE_DIR = $(shell pkg-config --cflags fuse)
LIBFUSE_LIB = $(shell pkg-config --libs fuse)
CFLAGS_LIBFUSE = $(CFLAGS) $(LIBFUSE_INCLUDE_DIR) $(LIBFUSE_LIB)
CFLAGS_EXTRA = -Wall -g -L/usr/local/lib -lzlog -lpthread
LIBCRYPT_FLAGS = `libgcrypt-config --cflags`
GNULIB_FLAGS = `pkg-config --cflags --libs  glib-2.0`
OPENSSL_FLAGS = -lssl -lcrypto
LIBERASURECODE_FLAGS = -lerasurecode -ldl
COMP_FLAGS = -Wall -g

all: safefs

logdef.o: logdef.c
	$(CC) $< `pkg-config --cflags --libs  glib-2.0` $(CFLAGS_EXTRA) -c -o $@

utils.o: utils.c
	$(CC) $< $(CFLAGS_EXTRA) -c -o $@

nopcrypt.o: crypto/nopcrypt.c
	$(CC) $< $(CFLAGS_EXTRA) $(CFLAGS_LIBFUSE) -fpic -c -o $@

nopcrypt_padded.o:crypto/nopcrypt_padded.c
	$(CC) $< $(CFLAGS_EXTRA) $(CFLAGS_LIBFUSE) $(GNULIB_FLAGS) -fpic -c -o $@

xor.o: multi_loop_drivers/xor.c
	$(CC) $< $(CFLAGS_EXTRA) $(CFLAGS_LIBFUSE) $(GNULIB_FLAGS) -fpic -c -o $@

rep.o: multi_loop_drivers/rep.c
	$(CC) $< $(CFLAGS_EXTRA) $(CFLAGS_LIBFUSE) $(GNULIB_FLAGS) -fpic -c -o $@

erasure.o:multi_loop_drivers/erasure.c
	$(CC) $< $(CFLAGS_EXTRA) $(CFLAGS_LIBFUSE) $(GNULIB_FLAGS) -fpic -c -o $@

nopalign.o: align/nopalign.c
	$(CC) $< $(CFLAGS_EXTRA) $(CFLAGS_LIBFUSE) -fpic -c -o $@

nopfuse.o: nopfuse.c
	$(CC) $< $(CFLAGS_EXTRA) $(CFLAGS_LIBFUSE) $(GNULIB_FLAGS) -fpic -c -o $@

blockalign.o: align/blockalign.c
	$(CC) $< $(CFLAGS_EXTRA) $(GNULIB_FLAGS) $(CFLAGS_LIBFUSE) -fpic -c -o $@

timestamps.o: timestamps/timestamps.c
	$(CC) $< $(CFLAGS_EXTRA) $(GNULIB_FLAGS) $(CFLAGS_LIBFUSE) -fpic -c -o $@

sfuse.o: sfuse.c
	$(CC) $< -fpic $(CFLAGS_LIBFUSE)  $(GNULIB_FLAGS) $(CFLAGS_EXTRA) -c -o $@ `libgcrypt-config --cflags --libs`

ivtable.o: ivtable/ivtable.c
	$(CC) $< $(CFLAGS) $(GNULIB_FLAGS) $(CFLAGS_EXTRA) -fpic -c -o $@

alignfuse.o: alignfuse.c
	$(CC) $< -fpic $(CFLAGS_LIBFUSE) $(GNULIB_FLAGS)  $(CFLAGS_EXTRA)  -c -o $@ `libgcrypt-config --cflags --libs`

symmetric.o: crypto/openssl/symmetric.c
	$(CC) $< $(CFLAGS) $(OPENSSL_FLAGS)  -fpic -c -o $@

rand_symetric.o: crypto/rand_symmetric.c
	$(CC) $<  $(CFLAGS) $(OPENSSL_FLAGS) $(GNULIB_FLAGS) $(CFLAGS_LIBFUSE) -fpic -c -o $@

det_symmetric.o: crypto/det_symmetric.c
	$(CC) $< $(CFLAGS) $(OPENSSL_FLAGS) $(GNULIB_FLAGS) $(CFLAGS_LIBFUSE)  -fpic -c -o $@

encode.o: sfuse.o openssl_det_symmetric.o openssl_rand_symetric.o nopcrypt.o
	$(CC) sfuse.o openssl_det_symmetric.o openssl_rand_symetric.o nopcrypt.o -c -o $@

sds_config.o: SDSConfig.c
	$(CC) $<  `pkg-config --cflags --libs  glib-2.0` -c -o $@

multi_loopback.o: multi_loopback.c utils.o
	$(CC) $< $(CFLAGS_LIBFUSE) $(CFLAGS_EXTRA) $(GNULIB_FLAGS) -c -o $@

inih.o: inih/ini.c
	$(CC) $< -c -o $@

safefs: alignfuse.o nopalign.o blockalign.o  sds_config.o logdef.o inih.o timestamps.o sfuse.o symmetric.o det_symmetric.o rand_symetric.o nopcrypt.o nopcrypt_padded.o utils.o ivtable.o erasure.o rep.o xor.o multi_loopback.o nopfuse.o
	$(CC) SDSFuse.c alignfuse.o  nopalign.o blockalign.o timestamps.o sfuse.o symmetric.o det_symmetric.o rand_symetric.o rep.o xor.o erasure.o nopcrypt.o sds_config.o logdef.o inih.o nopcrypt_padded.o utils.o multi_loopback.o ivtable.o nopfuse.o  $(LIBCRYPT_FLAGS) $(CFLAGS_FUSE) $(CFLAGS_LIBFUSE)  $(CFLAGS_EXTRA)  $(OPENSSL_FLAGS) $(LIBERASURECODE_FLAGS) `pkg-config --cflags --libs  glib-2.0` -o $@


info: $(TARGETS)
	@echo
	@echo Compiled. The following is a typical way to run the loopback file system. In
	@echo this example, /tmp/dir is an existing directory whose contents will become
	@echo available in the existing mount point /Volumes/loop:
	@echo
	@echo "sudo ./sloopback /Users/jtpaulo/Desktop/sfusetest/mount -omodules=threadid:subdir,subdir=/Users/jtpaulo/Desktop/sfusetest/data -oallow_other,native_xattr,volname=LoopbackFS"
	@echo

clean:
	rm -f $(TARGETS) *.o
