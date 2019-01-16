CC 	= mpicc
CFLAGS	= -Wall -I../include
CLIBS	= -libverbs

TARGET = $(wildcard *.c)
EXES  = $(TARGET:.c=.out)

all default: $(EXES)

.PHONY = clean

clean:
	rm -rf *.o *.out
