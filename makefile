FILES = src/main.c src/parse.c src/mach.c src/hashtable.c src/events.c src/windows.c src/border.c src/animation.c 
LIBS = -framework AppKit -framework CoreVideo -F/System/Library/PrivateFrameworks/ -framework SkyLight

all: | bin
	clang -std=c99 -O3 -g $(FILES) -o bin/borders $(LIBS)

debug: | bin
	clang -std=c99 -O0 -g -DDEBUG $(FILES) -o bin/debug $(LIBS)

asan: | bin
	clang -std=c99 -Wall -g -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g $(FILES) -o bin/debug $(LIBS)
	./bin/debug

test:
	clang -std=c99 -O0 -g -ffunction-sections tests/color_style_test.c src/hashtable.c -Wl,-dead_strip $(LIBS) -o /tmp/jankyborders-color-style-test
	/tmp/jankyborders-color-style-test

bin:
	mkdir bin

clean:
	rm -rf bin
