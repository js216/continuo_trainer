CPPFLAGS = -Iimgui -Irtmidi -Isrc $(shell pkg-config --cflags x11)
CXXFLAGS = -Wall -Wextra -std=c++20 -g -fsanitize=address
LDFLAGS  = -lGL -lX11 -lXext -lpthread -lasound -fsanitize=address

TARGET  = continuo_trainer
SRC = $(wildcard src/*.cpp)
HDRS = $(wildcard src/*.h src/*.hpp)
OBJS = $(SRC:.cpp=.o)

LIB_OBJS = \
	rtmidi/RtMidi.o \
	imgui/imgui.o \
	imgui/imgui_draw.o \
	imgui/imgui_tables.o \
	imgui/imgui_widgets.o \
	imgui/misc/cpp/imgui_stdlib.o \
	imgui/backends/imgui_impl_opengl3.o

# Building the program

all: $(TARGET)

$(TARGET): $(LIB_OBJS) $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

rtmidi/RtMidi.o: rtmidi/RtMidi.cpp
	$(CXX) $(CXXFLAGS) -D__LINUX_ALSA__ -c $< -o $@

imgui/%.o: imgui/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF $(@:.o=.d) -c $< -o $@

%: %.o
	$(CXX) -o $@ $^ $(LDFLAGS)

# Testing and linting

check: format cppcheck tidy inclusions iwyu test

format:
	clang-format --dry-run -Werror $(SRC)

tidy:
	make clean
	bear -- make -j$(shell nproc) $(TARGET)
	run-clang-tidy -j$(shell nproc) -p . $(SRC)

cppcheck:
	cppcheck --enable=all --inconclusive --std=c++17 --force --quiet \
		--error-exitcode=1 --suppress=missingInclude src

inclusions: $(INC_SRCS) scripts/inclusions.py
	python3 scripts/inclusions.py $(SRC) $(HDRS) > incl.dot ; \
	PYTHON_EXIT=$$? ; \
	dot -Tpdf incl.dot -o incl.pdf ; \
	exit $$PYTHON_EXIT

iwyu:
	@set -e; \
	for f in $(SRC); do \
	include-what-you-use -Xiwyu --error=1 \
	-Xiwyu --no_comments \
	-Xiwyu --quoted_includes_first \
	$$f $(CPPFLAGS); \
	done

TEST_PROGS = test/test_theory.exe

test: $(TEST_PROGS)
	test/test_runner.sh $^

test/test_theory: test/test_theory.o src/theory.o src/util.o

# General

.PHONY: all clean check format cppcheck tidy inclusions iwyu test

clean:
	rm -f $(LIB_OBJS) $(OBJS) $(TARGET) compile_commands.json
	rm -f $(TEST_PROGS)
	rm -f incl.dot incl.pdf
	rm -f src/*.d

-include $(OBJS:.o=.d)
