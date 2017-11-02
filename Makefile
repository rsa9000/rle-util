TARGET=rle
CC?=gcc
CFLAGS+=-Wall
PREFIX?=/usr/local
INSTALL?=install

.PHONY: all install clean

all: $(TARGET)

$(TARGET): rle.c
	$(CC) $(CFLAGS) $< -o $@

install:
	$(INSTALL) -d $(PREFIX)/bin
	$(INSTALL) -m 755 $(TARGET) $(PREFIX)/bin

clean:
	rm -rf $(TARGET)
