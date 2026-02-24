PROGS = analyzer chunker driver grouper gui hint loader midi \
        realizer recorder rules scorer stats l2ly g2ly

all: $(addprefix build/,$(PROGS))

pdfs: $(patsubst %.txt,%.pdf,$(wildcard lessons/*.txt))

build/%: %.c | build
	$(CC) $< -o $@ -Wall -Wextra -std=c99 -fsanitize=address -lrtmidi

build/%: %.rs | build
	rustc $< -o $@

%.pdf: %.ly
	lilypond -o $(@:.pdf=) $^

%.ly: %.txt build/l2ly
	build/l2ly < $< > $@

build:
	mkdir -p build

format:
	clang-format -i *.c
	rustfmt *.rs

clean:
	rm -rf build lessons/*.pdf
