CFLAGS = -Wall -Wextra -std=c99 -fsanitize=address -D_POSIX_C_SOURCE=200809L -Wno-unused-function
CPPFLAGS = -Ilib -Ilib/imgui -Ilib/imgui/backends
CXXFLAGS = -std=c++11 -g -Wall -Wformat `pkg-config --cflags glfw3`
LDLIBS = -lpthread -lutil -lrtmidi -lX11 -lm -lGL `pkg-config --static --libs glfw3`

PROGS = chunk group load midi l2ly g2ly entry run gui

IMGUI = imgui.o imgui_demo.o imgui_draw.o imgui_tables.o imgui_widgets.o \
        backends/imgui_impl_glfw.o backends/imgui_impl_opengl3.o

IMGUI_OBJ = $(patsubst %,lib/imgui/%,$(IMGUI))

all: $(addprefix bin/,$(PROGS))

bin/%: src/%.c | bin
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

bin/gui: src/gui.cpp $(IMGUI_OBJ) | $(patsubst %.txt,%.png,$(wildcard seq/*.txt)) bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

bin/%: src/%.rs | bin
	rustc $< -o $@

%.png: %.ly
	lilypond --png -dpreview -o $(@:.png=) $<
	mv $(@:.png=).preview.png $@

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
	rm -rf bin tmp seq/*.pdf seq/*.png $(IMGUI_OBJ)
