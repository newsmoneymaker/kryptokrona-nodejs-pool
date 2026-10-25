# xkrhash: cn_turtle_lite_slow_hash_v2 share validation helper of the XKR pool. Sources in xkr-src/ are
# vendored verbatim from Kryptokrona's own node (see xkrhash.cpp's header comment and
# kryptokrona-project.md), symbols renamed xkr_ via xkr-src/xkr_rename.h to avoid colliding with any
# other vendored coin's crypto.
#   make            (STATIC=-static makes a binary that runs on any Linux)
CXX ?= g++
CC ?= gcc
STATIC ?=

XKR_DIR = xkr-src
ISA_FLAGS = -maes -msse2 -msse4.1 -mssse3

C_SOURCES = aesb.c blake256.c groestl.c hash.c hash-extra-blake.c hash-extra-groestl.c hash-extra-jh.c \
            hash-extra-skein.c jh.c keccak.c oaes_lib.c skein.c slow-hash.c tree-hash.c
C_OBJS = $(addprefix $(XKR_DIR)/,$(C_SOURCES:.c=.o))

CFLAGS = -O2 -w -std=gnu11 $(ISA_FLAGS) -include $(XKR_DIR)/xkr_rename.h -I$(XKR_DIR) -I$(XKR_DIR)/Common
CXXFLAGS = -O2 -std=c++11 -I$(XKR_DIR)

xkrhash: xkrhash.o $(C_OBJS)
	$(CXX) -O2 $(STATIC) -o $@ xkrhash.o $(C_OBJS) -lpthread

xkrhash.o: xkrhash.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(XKR_DIR)/%.o: $(XKR_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f xkrhash xkrhash.o $(C_OBJS)
