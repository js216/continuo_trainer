CFLAGS = -Wall -Wextra -std=c99 -fsanitize=address -D_POSIX_C_SOURCE=200809L -Wno-unused-function
CPPFLAGS = -Ilib -Ilib/imgui -Ilib/imgui/backends
CXXFLAGS = -std=c++11 -g -Wall -Wformat `pkg-config --cflags glfw3`
LDLIBS = -lpthread -lutil -lrtmidi -lX11 -lm -lGL `pkg-config --static --libs glfw3`

PROGS = group midi l2ly g2ly entry run gui karaoke synth

IMGUI = imgui.o imgui_demo.o imgui_draw.o imgui_tables.o imgui_widgets.o \
        backends/imgui_impl_glfw.o backends/imgui_impl_opengl3.o

IMGUI_OBJ = $(patsubst %,lib/imgui/%,$(IMGUI))
PNG = $(patsubst %.txt,%.png,$(wildcard seq/*.txt)) $(patsubst %.txt,%.png,$(wildcard chn/*.txt))

all: $(addprefix bin/,$(PROGS))

bin/%: src/%.c | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LDLIBS)

bin/gui: src/gui.cpp $(IMGUI_OBJ) | $(PNG) bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

bin/%: src/%.rs | bin
	rustc $< -o $@

%.png: %.ly
	lilypond --png -dpreview -o $(@:.png=) $<
	convert $(@:.png=).preview.png -trim +repage -bordercolor white -border 10x20 $@
	rm $(@:.png=).preview.png

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
	clang-format -i src/*.c src/*.cpp
	rustfmt src/*.rs
	stylua src/*.lua

test:
	lua src/tst.lua tst bin src

clean:
	rm -rf bin tmp seq/*.pdf seq/*.png chn/*.ly chn/*.png $(IMGUI_OBJ)
