CFLAGS = -Wall -Wextra -std=c99 -fsanitize=address

PROG = analyzer chunker driver grouper gui hint loader midi \
       realizer recorder rules scorer stats

all: $(PROG)

%: %.c
	$(CC) $(CFLAGS) $^ -o $@

%: %.rs
	rustc $^ -o $@

clean:
	rm $(PROG)

test: $(PROG)
	echo "LESSON_FILE ../imgui_continuo/lessons/2.txt" | ./loader
