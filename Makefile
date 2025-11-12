CC      = g++
CFLAGS  = -Iimgui -Isrc -Wall -Wextra -std=c++17 `pkg-config --cflags sdl2`
LDFLAGS = `pkg-config --libs sdl2` -lGL

TARGET  = bin/app
IMGUI_OBJS = \
	imgui/imgui.o \
	imgui/imgui_draw.o \
	imgui/imgui_tables.o \
	imgui/imgui_widgets.o \
	imgui/backends/imgui_impl_sdl2.o \
	imgui/backends/imgui_impl_opengl3.o

all: $(TARGET)

$(TARGET): $(IMGUI_OBJS) src/main.o | bin
	$(CC) -o $@ $^ $(LDFLAGS)

imgui/%.o: imgui/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

bin:
	mkdir -p bin

clean:
	rm -f $(IMGUI_OBJS) src/*.o $(TARGET)

.PHONY: all clean

