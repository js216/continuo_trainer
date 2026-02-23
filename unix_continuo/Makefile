CFLAGS = -Wall -Wextra -std=c99 -fsanitize=address

PROGS = analyzer chunker driver grouper gui hint loader midi \
        realizer recorder rules scorer stats

BUILD_PROGS = $(addprefix build/,$(PROGS))

.PHONY: all clean test

all: $(BUILD_PROGS)

build/%: %.c | build
	$(CC) $(CFLAGS) $< -o $@

build/%: %.rs | build
	rustc $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build

test: build/loader
	echo "LOAD_LESSON 1" | build/loader
