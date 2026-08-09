CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic
LDLIBS := $(shell pkg-config --libs libusb-1.0)
CPPFLAGS := $(shell pkg-config --cflags libusb-1.0)

scarlett-front-panel: scarlett-front-panel.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDLIBS)

.PHONY: clean
clean:
	rm -f scarlett-front-panel
