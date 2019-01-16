TARGET = $(wildcard *.c)
EXES  = $(TARGET:.c=.out)

all default: $(EXES)

all:
	make -C mpi_p2p
	make -C mpi_onesided
