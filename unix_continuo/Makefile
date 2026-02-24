CFLAGS = -Wall -Wextra -std=c99 -fsanitize=address -lrtmidi

PROGS = analyzer chunker driver grouper gui hint loader midi \
        realizer recorder rules scorer stats l2ly g2ly render \
        draw

all: $(addprefix bin/,$(PROGS))

pdfs: $(patsubst %.txt,%.pdf,$(wildcard seq/*.txt))

bin/%: src/%.c | bin
	$(CC) $(CFLAGS) $< -o $@

bin/%: src/%.rs | bin
	rustc $< -o $@

%.pdf: %.ly
	lilypond -o $(@:.pdf=) $^

%.ly: %.txt bin/l2ly
	bin/l2ly < $< > $@

bin:
	mkdir -p bin

format:
	clang-format -i src/*.c
	rustfmt src/*.rs

clean:
	rm -rf bin seq/*.pdf
