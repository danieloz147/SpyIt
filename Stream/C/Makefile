ARCH ?= x64

CC_x64 := x86_64-w64-mingw32-gcc
CC_x86 := i686-w64-mingw32-gcc

ifeq ($(ARCH),x86)
CC := $(CC_x86)
else
CC := $(CC_x64)
endif

CFLAGS := -O2 -Wall -Wextra -DUNICODE -D_UNICODE
LIBS := -luser32 -lgdi32 -ldxgi -ld3d11 -lwindowscodecs -lws2_32 -lole32 -loleaut32 -lpropsys

TARGET := Stream.exe
SRC := Stream.c

.PHONY: all clean check-toolchain

all: check-toolchain $(TARGET)

check-toolchain:
	@command -v $(CC) >/dev/null 2>&1 || { \
		echo "Missing $(CC). Run 'make deps' at repo root."; \
		exit 1; \
	}

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f $(TARGET)
