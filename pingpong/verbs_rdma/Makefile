include ../make.mk

%.out: %.c ../include/pingpong_common.h ../include/pingpong_ibv_common.h
	$(CC) $(CFLAGS) -o $@ $< $(CLIBS)
