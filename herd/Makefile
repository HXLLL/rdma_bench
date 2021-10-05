CFLAGS  := -g3 -Wall -Wno-unused-result -I ../libhrd/ -I ../mica/
LD      := gcc -O3
# LD      := gcc
LDFLAGS := ${LDFLAGS} -libverbs -lrt -lpthread -lmemcached -lnuma -lprofiler

APPS    := main

all: ${APPS}

main: ../libhrd/hrd_conn.o ../libhrd/hrd_util.o ../mica/mica.o ../mica/city.o main.o client.o master.o worker.o main.h
	${LD} -o $@ $^ ${LDFLAGS}

PHONY: clean
clean:
	rm -f *.o ../libhrd/*.o ../mica/*.o ${APPS}
