CFLAGS = -Wall -Wextra -std=c99 -fsanitize=address -lrtmidi -lX11

PROGS = analyzer chunker driver grouper gui hint loader midi \
        realizer recorder rules scorer stats l2ly g2ly render \
        draw show entry

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

.PHONY: format play test clean

format:
	clang-format -i src/*.c
	rustfmt src/*.rs

play:
	echo LOAD_LESSON 1 | bin/loader > test/pipe.log
	bin/midi >> test/pipe.log

test:
	cd test && sh run_tests.sh

clean:
	rm -rf bin seq/*.pdf
