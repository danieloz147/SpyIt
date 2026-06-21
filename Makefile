ARCH ?= x64

.PHONY: all stream enum-screens bof clean deps

all: stream enum-screens bof

stream:
	$(MAKE) -C Stream/C ARCH=$(ARCH)

enum-screens:
	$(MAKE) -C Enum-Screens/C ARCH=$(ARCH)

bof:
	$(MAKE) -C Enum-Screens/BOF

clean:
	$(MAKE) -C Stream/C clean
	$(MAKE) -C Enum-Screens/C clean
	$(MAKE) -C Enum-Screens/BOF clean

deps:
	@if command -v apt-get >/dev/null 2>&1; then \
		sudo apt-get update && sudo apt-get install -y mingw-w64; \
	else \
		echo "apt-get not found. Install mingw-w64 using your distro package manager."; \
		exit 1; \
	fi
