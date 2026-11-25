CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude
LIBS = -pthread

MODULE_SRCS = src/modules/bakery.c src/modules/pending_tx.c src/modules/persistence.c \
              src/modules/network.c src/modules/state.c src/modules/peer.c \
              src/modules/packet_handler.c src/modules/threads.c src/modules/args.c
MODULE_OBJS = $(MODULE_SRCS:.c=.o)

MAIN_SRC = src/main.c
MAIN_OBJ = $(MAIN_SRC:.c=.o)

SIM_SRC = src/simulador.c
SIM_OBJ = $(SIM_SRC:.c=.o)

MON_SRC = src/monitor.c
MON_OBJ = $(MON_SRC:.c=.o)

TARGETS = tienda_node simulador monitor

all: $(TARGETS)

tienda_node: $(MAIN_OBJ) $(MODULE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

simulador: $(SIM_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

monitor: $(MON_OBJ) src/modules/network.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

src/modules/%.o: src/modules/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(MAIN_OBJ): include/store_p2p.h include/state.h include/persistence.h include/peer.h include/threads.h include/packet_handler.h include/args.h include/network.h include/bakery.h

src/modules/bakery.o: include/bakery.h include/store_p2p.h
src/modules/pending_tx.o: include/pending_tx.h include/store_p2p.h
src/modules/persistence.o: include/persistence.h include/store_p2p.h
src/modules/network.o: include/network.h include/store_p2p.h
src/modules/state.o: include/state.h include/store_p2p.h include/bakery.h include/pending_tx.h
src/modules/peer.o: include/peer.h include/store_p2p.h include/bakery.h include/network.h
src/modules/packet_handler.o: include/packet_handler.h include/store_p2p.h include/bakery.h include/pending_tx.h include/persistence.h include/peer.h include/network.h
src/modules/threads.o: include/threads.h include/store_p2p.h include/packet_handler.h include/bakery.h include/network.h include/peer.h
src/modules/args.o: include/args.h include/store_p2p.h

clean:
	rm -f $(TARGETS) $(MAIN_OBJ) $(MODULE_OBJS) $(SIM_OBJ) $(MON_OBJ) *.o *.log log_node*.txt

.PHONY: all clean