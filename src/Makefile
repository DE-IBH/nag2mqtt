CC = gcc
CFLAGS = -fPIC -shared -Wall -g -lrt
RM = rm -f

CFLAGS += $(shell pkg-config --cflags json-c)
LDFLAGS += $(shell pkg-config --libs json-c)

default: neb2mqtt.so

neb2mqtt.so: neb2mqtt.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	$(RM) neb2mqtt.so

