# Default to building in the current directory, unless
# overridden by the calling Makefile or on the command line.
O=$(shell pwd)

BASE=..
PREFIX=$(O)/$(BASE)/install
LIBDIRS += -L$(PREFIX)/lib
PROGRAMS=tests
CFLAGS= -static $(LIBDIRS) -std=c99 -lm
$(info CFLAGS is $(CFLAGS))
INCDIRS += \
	-I. \
	-I$(PREFIX)/include \
	-I$(BASE)/../include \
	-I$O/$(BASE)/../include \
	-I$O/$(BASE)/../include2 \
	-I/home/armour1/Desktop/kitten/include/lwk

all: $(PROGRAMS)

$(PROGRAMS): $(PROGRAMS).o
	gcc  $(CFLAGS) -o $(PROGRAMS) $(PROGRAMS).o ../liblwk/liblwk.a -lpthread -lm
	install -m 0755 $(PROGRAMS) $(PREFIX)/bin
LINUX.out: $(PROGRAMS).o
	gcc  $(CFLAGS) -o LINUX.out $(PROGRAMS).o ../liblwk/liblwk.a -lpthread -lm
	

$(PROGRAMS).o: tests.c
	gcc $(INCDIRS) -c  tests.c  ../liblwk/liblwk.a


clean:
	rm $(PROGRAMS)
	rm *.o
