ARCH ?= x64

CC_x64 := x86_64-w64-mingw32-gcc
CC_x86 := i686-w64-mingw32-gcc

ifeq ($(ARCH),x86)
CC := $(CC_x86)
DEFINES := -D_WIN32 -D_M_IX86
else
CC := $(CC_x64)
DEFINES := -D_WIN64 -D_WIN32 -D_M_AMD64 -D_AMD64_
endif

CFLAGS := -O2 -Wall -Wextra $(DEFINES)
LIBS := -luser32 -lgdi32

TARGET := enumerate_screens.exe
SRC := enumerate_screens.c

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
