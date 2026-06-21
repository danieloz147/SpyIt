CC_x64 := x86_64-w64-mingw32-gcc
CC_x86 := i686-w64-mingw32-gcc

CFLAGS := -O2 -Wall -Wextra -c -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-exceptions -fno-stack-protector -fno-ident
INCLUDES := -I.

OUT_X64 := enumerate_screens.x64.o
OUT_X86 := enumerate_screens.x86.o
SRC := enumerate_screens.c

.PHONY: all clean check-toolchain

all: check-toolchain $(OUT_X64) $(OUT_X86)

check-toolchain:
	@command -v $(CC_x64) >/dev/null 2>&1 || { \
		echo "Missing $(CC_x64). Run 'make deps' at repo root."; \
		exit 1; \
	}
	@command -v $(CC_x86) >/dev/null 2>&1 || { \
		echo "Missing $(CC_x86). Run 'make deps' at repo root."; \
		exit 1; \
	}

$(OUT_X64): $(SRC)
	$(CC_x64) $(CFLAGS) $(INCLUDES) $^ -o $@

$(OUT_X86): $(SRC)
	$(CC_x86) $(CFLAGS) $(INCLUDES) $^ -o $@

clean:
	rm -f $(OUT_X64) $(OUT_X86)
