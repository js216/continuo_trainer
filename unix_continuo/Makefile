CFLAGS = -Wall -Wextra -std=c99 -fsanitize=address
LDLIBS = -lpthread -lutil -lrtmidi -lX11

PROGS = chunk group load midi stats l2ly g2ly entry run

all: $(addprefix bin/,$(PROGS))

bin/%: src/%.c | bin
	$(CC) $(CFLAGS) $(LDLIBS) $< -o $@

bin/%: src/%.rs | bin
	rustc $< -o $@

%.pdf: %.ly
	lilypond -o $(@:.pdf=) $^

%.ly: %.txt bin/l2ly
	bin/l2ly < $< > $@

bin:
	mkdir -p bin

.PHONY: format test clean pdf

pdf: $(patsubst %.txt,%.pdf,$(wildcard seq/*.txt))
	@mkdir -p tmp
	python3 src/run2dot.py < src/play.dot > tmp/play.dot
	dot -Tpdf tmp/play.dot -o tmp/play.pdf

format:
	clang-format -i src/*.c
	rustfmt src/*.rs
	stylua src/*.lua

test:
	lua src/tst.lua tst bin src

clean:
	rm -rf bin tmp seq/*.pdf
