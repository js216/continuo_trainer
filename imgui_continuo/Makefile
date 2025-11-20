CC      = g++
CFLAGS  = -Iimgui -Irtmidi -Isrc -Wall -Wextra -std=c++20 `pkg-config --cflags sdl2`
LDFLAGS = `pkg-config --libs sdl2` -lGL -lasound -lpthread

CFLAGS  += -g -fsanitize=address -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=address

TARGET  = continuo_trainer
SRC = $(wildcard src/*.cpp)
HDRS = $(wildcard src/*.h src/*.hpp)
OBJS = $(SRC:.cpp=.o)

GRAPH_FILE = inclusions.dot
PDF_FILE = inclusions.pdf

LIB_OBJS = \
	rtmidi/RtMidi.o \
	imgui/imgui.o \
	imgui/imgui_draw.o \
	imgui/imgui_tables.o \
	imgui/imgui_widgets.o \
	imgui/backends/imgui_impl_sdl2.o \
	imgui/backends/imgui_impl_opengl3.o

all: $(TARGET)

$(TARGET): $(LIB_OBJS) $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

rtmidi/RtMidi.o: rtmidi/RtMidi.cpp
	$(CC) -Wall -D__LINUX_ALSA__ -c $< -o $@

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(LIB_OBJS) $(OBJS) $(TARGET) compile_commands.json
	rm -f $(GRAPH_FILE) $(PDF_FILE)

# Testing and linting

check: format cppcheck tidy inclusions

format:
	clang-format --dry-run -Werror $(SRC)

tidy:
	make clean
	bear -- make -j8 $(TARGET)
	clang-tidy -p . $(SRC)

cppcheck:
	cppcheck --enable=all --inconclusive --std=c++17 --force --quiet \
		--error-exitcode=1 --suppress=missingInclude src

inclusions: $(INC_SRCS) scripts/inclusions.py
	python3 scripts/inclusions.py $(SRC) $(HDRS) > $(GRAPH_FILE) ; \
	PYTHON_EXIT=$$? ; \
	dot -Tpdf $(GRAPH_FILE) -o $(PDF_FILE) ; \
	exit $$PYTHON_EXIT


.PHONY: all clean check format cppcheck tidy inclusions
