CC=arm-linux-gcc
CFLAGS=-Wall -Werror
LDFLAGS=-lm
PROGS=ffsmark

CONFS=config_fill.cfg config_nofill.cfg
SCRIPTS=Scripts/*

TARGET=root@193.52.16.240:~

ffsmark_SRC = postmark.c flashmon_ctrl.c syscaches.c \
	ffsmark_core.c
ffsmark_OBJS = $(ffsmark_SRC:.c=.o)

all: depends $(PROGS)

ffsmark: $(ffsmark_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	
%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $<

install: all
	scp -r $(PROGS) $(CONFS) $(SCRIPTS) $(TARGET)

clean:
	rm -rf *.o $(PROGS) .depends
	
depends: .depends
.depends:
	$(CC) -MM *.c *.h > .depends

-include .depends
