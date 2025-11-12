CC      = g++
CFLAGS  = -Iimgui -Isrc -Wall -Wextra -std=c++17 `pkg-config --cflags sdl2`
LDFLAGS = `pkg-config --libs sdl2` -lGL

TARGET  = continuo_trainer
SRC = $(wildcard src/*.cpp)
OBJS = $(SRC:.cpp=.o)

IMGUI_OBJS = \
	imgui/imgui.o \
	imgui/imgui_draw.o \
	imgui/imgui_tables.o \
	imgui/imgui_widgets.o \
	imgui/backends/imgui_impl_sdl2.o \
	imgui/backends/imgui_impl_opengl3.o

all: $(TARGET)

$(TARGET): $(IMGUI_OBJS) $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

imgui/%.o: imgui/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(IMGUI_OBJS) $(OBJS) $(TARGET) compile_commands.json

# Testing and linting

check: format cppcheck tidy

format:
	clang-format --dry-run -Werror $(SRC)

tidy:
	bear -- make $(TARGET)
	clang-tidy $(SRC)

cppcheck:
	cppcheck --enable=all --inconclusive --std=c++17 --force --quiet \
		--error-exitcode=1 --suppress=missingInclude src

.PHONY: all clean check format cppcheck tidy
