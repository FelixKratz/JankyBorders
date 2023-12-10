FILES = src/main.c src/parse.c src/mach.c src/hashtable.c src/events.c src/windows.c src/border.c 
LIBS = -framework AppKit -F/System/Library/PrivateFrameworks/ -framework SkyLight

all: | bin
	clang -std=c99 -O3 -g $(FILES) -o bin/borders $(LIBS)

debug: | bin
	clang -std=c99 -O0 -g -DDEBUG $(FILES) -o bin/debug $(LIBS)

bin:
	mkdir bin

clean:
	rm -rf bin
