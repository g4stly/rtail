CC 	= gcc
CFLAGS 	= -Wall -c -g

LIBS 	= -lcurses
LIBDIR	= lib
INCLUDE	= include

TARGET 	= build/rtail
SOURCES = src/main.c src/util.c
OBJECTS	= $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p build
	$(CC) -o $@ $(OBJECTS) -L$(LIBDIR) $(LIBS)
%.o: %.c
	$(CC) $(CFLAGS) -I$(INCLUDE) -o $@ $<

.PHONY: clean
clean: 
	rm -rf $(OBJECTS) $(TARGET)
