CC=clang
COMMON_CFLAGS= -Wall -Wextra -ggdb -std=c99 -pedantic
COMMON_LIBS= -ldl -lpthread -lm 
COMMON_SOURCES= src/glad.c
COMMON_INCLUDES= -Iinclude/

GLFW=$(shell pkg-config --cflags --libs glfw3)
FREETYPE=$(shell pkg-config --cflags --libs freetype2) 

.PHONY: all
all: main

main: build
	$(CC) $(COMMON_CFLAGS) -o build/main src/main.c src/tinyfiledialogs.c $(COMMON_SOURCES) $(COMMON_INCLUDES) $(COMMON_LIBS) $(GLFW) $(FREETYPE)

gen: build
	$(CC) $(COMMON_CFLAGS) -o build/gen src/gen.c $(COMMON_INCLUDES) -D_DEFAULT_SOURCE

build:
	mkdir -pv build

.PHONY: clean
clean:
	rm -r build
