CC=clang

COMMON_LIBS= -lm -ldl -lpthread
COMMON_CFLAGS= -Wall -Wextra -ggdb -std=c99 -pedantic

COMMON_INCLUDES= -Iinclude/
COMMON_SOURCES= src/glad.c src/tinyfiledialogs.c

GLFW=$(shell pkg-config --cflags --libs glfw3)

.PHONY: all
all: main

main: build
	$(CC) $(COMMON_CFLAGS) -o build/main src/main.c $(COMMON_SOURCES) $(COMMON_INCLUDES) $(COMMON_LIBS) $(GLFW) -D_DEFAULT_SOURCE

gen: build
	$(CC) $(COMMON_CFLAGS) -o build/gen src/gen.c $(COMMON_INCLUDES) -D_DEFAULT_SOURCE

build:
	mkdir -pv build

.PHONY: clean
clean:
	rm -r build
