CFLAGS = -Wall -Wextra -std=c99 -fsanitize=address
LDLIBS = -lpthread -lutil -lrtmidi -lX11

PROGS = chunk group gui hint load midi realize record stats \
        l2ly g2ly render draw show entry run

all: $(addprefix bin/,$(PROGS))

pdfs: $(patsubst %.txt,%.pdf,$(wildcard seq/*.txt))

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

.PHONY: format test clean dot

dot:
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
	rm -rf bin seq/*.pdf
