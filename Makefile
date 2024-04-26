CC=gcc
CFLAGS=-std=gnu99  -Wall -Wextra -Werror -pedantic
FILES=isa-ldapserver.c

.PHONY: clean ez run

all: isa-ldapserver

isa-ldapserver: $(FILES)
	$(CC) $(CFLAGS) -o $@ $(FILES)

clean:
	rm --force isa-ldapserver