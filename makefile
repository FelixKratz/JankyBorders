all: | bin
	clang -std=c99 -O3 -g src/main.c src/hashtable.c src/events.c src/windows.c src/border.c -o bin/borders -framework AppKit -F/System/Library/PrivateFrameworks/ -framework SkyLight

debug:
	clang -std=c99 -O0 -g src/main.c src/hashtable.c src/events.c src/windows.c src/border.c -o bin/debug -framework AppKit -F/System/Library/PrivateFrameworks/ -framework SkyLight

bin:
	mkdir bin

clean:
	rm -rf bin
