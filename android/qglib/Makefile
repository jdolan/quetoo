# Standalone build + test for the qglib compatibility layer.
# Portable to any C11 toolchain (gcc/clang). On Windows use run_tests.bat (MSVC)
# for the core suite.
#
#   make test       build + run every container test against qglib
#   make test-glib  build + run the SAME tests against system glib-2.0
#                   (differential gate: each must match its `make test` result)
#   make clean      remove build artifacts
#
# Each container has test_<name>.c. The core suite (test_qglib.c) covers GArray
# and GList; the rest are one translation unit each.

CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2

GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0 2>/dev/null)
GLIB_LIBS   := $(shell pkg-config --libs glib-2.0 2>/dev/null)

.PHONY: test test-glib clean

test:
	@set -e; \
	echo "[qglib] core (array+list)"; $(CC) $(CFLAGS) -o qgtest      test_qglib.c    qglib_array.c qglib_list.c; ./qgtest; \
	echo "[qglib] string";            $(CC) $(CFLAGS) -o t_string    test_string.c   qglib_string.c            ; ./t_string; \
	echo "[qglib] slist";             $(CC) $(CFLAGS) -o t_slist     test_slist.c    qglib_slist.c             ; ./t_slist; \
	echo "[qglib] ptrarray";          $(CC) $(CFLAGS) -o t_ptrarray  test_ptrarray.c qglib_ptrarray.c          ; ./t_ptrarray; \
	echo "[qglib] queue";             $(CC) $(CFLAGS) -o t_queue     test_queue.c    qglib_queue.c             ; ./t_queue; \
	echo "[qglib] hash";              $(CC) $(CFLAGS) -o t_hash       test_hash.c     qglib_hash.c  qglib_list.c; ./t_hash; \
	echo "[qglib] util";              $(CC) $(CFLAGS) -o t_util       test_util.c     qglib_util.c              ; ./t_util; \
	echo "[qglib] checksum";          $(CC) $(CFLAGS) -o t_checksum   test_checksum.c qglib_checksum.c          ; ./t_checksum; \
	echo "[qglib] misc";              $(CC) $(CFLAGS) -o t_misc       test_misc.c     qglib_misc.c              ; ./t_misc; \
	echo "[qglib] rand";              $(CC) $(CFLAGS) -o t_rand       test_rand.c     qglib_rand.c              ; ./t_rand; \
	echo "[qglib] regex";             $(CC) $(CFLAGS) -o t_regex      test_regex.c    qglib_regex.c qglib_string.c qglib_util.c ; ./t_regex; \
	echo "[qglib] io";                $(CC) $(CFLAGS) -o t_io         test_io.c       qglib_io.c qglib_util.c ; ./t_io; \
	echo "ALL CONTAINERS PASS (qglib backend)"

test-glib:
	@set -e; \
	for t in qglib string slist ptrarray queue hash util checksum misc rand regex io; do \
	  echo "[real-glib] $$t"; \
	  $(CC) $(CFLAGS) -DQG_USE_REAL_GLIB $(GLIB_CFLAGS) -o tg_$$t test_$$t.c $(GLIB_LIBS); ./tg_$$t; \
	done; \
	echo "ALL CONTAINERS PASS (system glib backend)"

clean:
	rm -f qgtest qgtest-glib t_string t_slist t_ptrarray t_queue t_hash t_util t_checksum t_misc t_rand t_regex t_io tg_* *.o *.obj *.exe *.pdb *.ilk
