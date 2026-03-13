# bench/Makefile — build WALI mmap benchmark
#
# Variables:
#   PREFIX   path to installed WALI musl sysroot  (default: /usr/local/musl)
#   CC       C compiler                            (default: clang)
#   RUNTIME  WALI runtime binary for 'make run'   (default: wali-rt)

PREFIX  ?= /usr/local/musl
CC      ?= clang
RUNTIME ?= wali-rt

TARGET  = --target=wasm32-linux-muslwali
CFLAGS  = $(TARGET) -mbulk-memory -matomics -O2 --sysroot=$(PREFIX)

OUT     = bench_mmap.wasm
SRC     = bench_mmap.c

.PHONY: all run clean

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

run: $(OUT)
	$(RUNTIME) $(OUT)

clean:
	rm -f $(OUT)