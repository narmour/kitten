# Default to building in the current directory, unless
# overridden by the calling Makefile or on the command line.
O=$(shell pwd)

BASE=..
PREFIX=$(O)/$(BASE)/install
LIBDIRS += -L$(PREFIX)/lib
PROGRAMS=test_prog
CFLAGS= -static $(LIBDIRS)
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
	gcc  $(CFLAGS) -o $(PROGRAMS) $(PROGRAMS).o ../liblwk/liblwk.a
	install -m 0755 $(PROGRAMS) $(PREFIX)/bin

$(PROGRAMS).o: test_prog.c
	gcc $(INCDIRS) -c  ../liblwk/liblwk.a test_prog.c 	


clean:
	rm $(PROGRAMS)
	rm *.o
