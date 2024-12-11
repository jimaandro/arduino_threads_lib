CC = gcc
CFLAGS = -m32 -Wall -Wextra -pedantic -g -Iinclude -D_GNU_SOURCE -fno-toplevel-reorder
BUILD_PATH = ./build

# Put the path to the source file here and replace .c with .o
SRC_FILE = examples/spin3.o

all: build_path a.out

a.out: build/thread.o build/chan.o build/queue.o build/symtablehash.o build/threadsafe_libc.o build/swtch.o $(SRC_FILE)
	$(CC) $(CFLAGS) -o $(BUILD_PATH)/$@ $^

build/swtch.o: src/swtch.S
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

build_path: | $(BUILD_PATH)


$(BUILD_PATH):
	mkdir -p $@

clean:
	rm -r $(BUILD_PATH) $(SRC_FILE)