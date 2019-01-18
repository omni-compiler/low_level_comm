include ../make.mk

CFLAGS += -fopenmp

%.out: %.c ../include/pingpong_common.h
	$(CC) $(CFLAGS) -o $@ $< $(CLIBS)
