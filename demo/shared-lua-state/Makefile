CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -g3

# -ldl 是 POSIX 下的动态加载库，MinGW 不需要
# MinGW 下 Lua 内部使用 Win32 LoadLibrary，不需要额外链接
ifeq ($(findstring mingw,$(CC)),mingw)
LDFLAGS  := -llua -lm
else
LDFLAGS  := -llua -ldl -lm
endif

# ============================================================
# Native build
#   $ make
#   $ ./demo
# ============================================================
all: demo

demo: main.c apps/clock.lua apps/counter.lua
	$(CC) $(CFLAGS) main.c -o demo $(LDFLAGS)

# ============================================================
# Cross-compile for ARMv7
#   $ make CROSS=1 ARM_LUA_DIR=/path/to/arm/lua
# ============================================================
CROSSCC    := arm-linux-gnueabihf-gcc
ARM_LUA_DIR ?= /path/to/arm/lua

ifeq ($(CROSS),1)
override CC := $(CROSSCC)
override CFLAGS += -I$(ARM_LUA_DIR)/include
override LDFLAGS := -L$(ARM_LUA_DIR)/lib -llua -ldl -lm
endif

.PHONY: clean
clean:
	rm -f demo demo.exe demo_arm
