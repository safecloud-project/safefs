CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -g
CFLAGS += -lerasurecode -ldl
CFLAGS += `pkg-config --cflags --libs glib-2.0`
CFLAGS += `pkg-config --cflags --libs fuse`

SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
MAIN=main

$(MAIN): $(OBJECTS)
	$(CC) $(OBJECTS) $(CFLAGS) -o $(MAIN)

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm --force $(MAIN) $(OBJECTS)
