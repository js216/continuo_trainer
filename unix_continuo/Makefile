CFLAGS = -Wall -Wextra -std=c98 -fsanitize=address -lrtmidi -lX11

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

.PHONY: format test clean

format:
	clang-format -i src/*.c
	rustfmt src/*.rs

test:
	cd tst && sh run_tests.sh

clean:
	rm -rf bin seq/*.pdf

# R&D only

play1:
	rm -f tmp/pipe.log
	echo LOAD_LESSON 2 | bin/loader > tmp/pipe.log
	tail -f tmp/pipe.log | bin/grouper | bin/rules

play2:
	bin/midi >> tmp/pipe.log
