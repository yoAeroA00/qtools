CC       = gcc
LIBS     = -lreadline
CFLAGS   = -O2 -g -Wno-unused-result -Wunused

OBJS     = hdlc.o  qcio.o memio.o chipconfig.o
BINS     = qcommand qrmem qrflash qdload mibibsplit qwflash qwdirect qefs qnvram qblinfo qident qterminal qbadblock qflashparm

.PHONY: all clean

all:    $(BINS)

clean:
	rm -f *.o
	rm -f $(BINS)

#.c.o:
#	$(CC) -o $@ $(LIBS) $^ qcio.o

qcio.o: qcio.c
hdlc.o: hdlc.c
sahara.o: sahara.c
chipconfig.o: chipconfig.c
memio.o: memio.c
ptable.o: ptable.c
#	$(CC) -c qcio.c

qcommand: qcommand.o  $(OBJS)
	$(CC) $^ -o $@ $(LIBS)

qrmem: qrmem.o $(OBJS)
	$(CC) $^ -o $@ $(LIBS)

qrflash: qrflash.o $(OBJS) ptable.o
	$(CC) $^ -o $@ $(LIBS)

qwflash: qwflash.o $(OBJS)
	$(CC) $^ -o $@ $(LIBS)

#qwimage: qwimage.o $(OBJS)
#	$(CC) $^ -o $@ $(LIBS)

qdload: qdload.o sahara.o $(OBJS)  ptable.o
	$(CC) $^ -o $@ $(LIBS)

qwdirect: qwdirect.o $(OBJS)  ptable.o
	$(CC) $^ -o $@ $(LIBS)
	
qefs  : qefs.o efsio.o $(OBJS)
	$(CC) $^ -o $@ $(LIBS)

qnvram  : qnvram.o $(OBJS)
	$(CC) $^ -o $@ $(LIBS)
	
mibibsplit: mibibsplit.o $(OBJS)
	$(CC) $^ -o $@ $(LIBS)

qblinfo:    qblinfo.o $(OBJS)
	$(CC) $^ -o $@  $(LIBS)

qident:      qident.o $(OBJS)
	$(CC) $^ -o $@ $(LIBS)

qterminal:   qterminal.o $(OBJS)
	$(CC) $^ -o $@ $(LIBS)

qbadblock:   qbadblock.o $(OBJS)  ptable.o
	$(CC) $^ -o $@ $(LIBS)

qflashparm:  qflashparm.o $(OBJS)
	$(CC) $^ -o $@ $(LIBS)
	
