CC := gcc
CFLAGS := -Wall -O2

.PHONY: all install clean 

BINARIES := fdholderctl fdholderd

all: $(BINARIES)

clean:
	$(RM) $(BINARIES)

install: $(BINARIES)
	mkdir -p $(DESTDIR)/usr/bin $(DESTDIR)/usr/lib/fdholder
	install -t $(DESTDIR)/usr/bin fdholderctl
	install -t $(DESTDIR)/usr/lib/fdholder fdholderd
