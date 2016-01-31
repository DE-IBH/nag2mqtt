CC = gcc
CFLAGS = -fPIC -shared -Wall -g -lrt
RM = rm -f

CFLAGS += $(shell pkg-config --cflags json-c)
LDFLAGS += $(shell pkg-config --libs json-c)

all: neb2mqtt.so

neb2mqtt.so: neb2mqtt.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

install: neb2mqtt.so
	mkdir -p "$(DESTDIR)/usr/lib/nag2mqtt"
	cp neb2mqtt.so "$(DESTDIR)/usr/lib/nag2mqtt/"

clean:
	$(RM) *.so *.o *~
