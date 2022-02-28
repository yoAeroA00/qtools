CC       = gcc
LIBS     = -lreadline
CFLAGS   = -O2 -Wno-unused-result -Wunused

OBJS     = hdlc.o qcio.o memio.o chipconfig.o utils.o
BINS     = qcommand qrmem qrflash qdload mibibsplit qwflash qwdirect qefs qnvram qblinfo qident qterminal qbadblock qflashparm

.PHONY: all clean

all: $(BINS)

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(BINS): %: %.o $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f *.o
	rm -f $(BINS)

chipconfig : utils.o
qefs : efsio.o
qdload : sahara.o
qdload qrflash qwdirect qbadblock : ptable.o
$(addsuffix .o, $(BINS)) $(OBJS) : include.h
