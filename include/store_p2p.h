#ifndef STORE_P2P_H
#define STORE_P2P_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <signal.h>

#define MAX_PEERS 32
#define MAX_THREADS 256
#define MAX_PENDING_TX 1024
#define MAX_WORKERS 128
#define COLA_ESCUCHA 16
#define TIMEOUT_POLL_MS 500
#define ESPERA_APAGON_MS 150

typedef enum {
    STORE_TYPE_INDEPENDENT = 0,
    STORE_TYPE_CHAIN = 1
} StoreType;

typedef enum {
    STORE_STATUS_CLOSED = 0,
    STORE_STATUS_OPEN = 1
} StoreStatus;

typedef enum {
    MSG_HANDSHAKE = 1,
    MSG_TRANSACTION,
    MSG_LIMIT_REACHED,
    MSG_GOV_QUERY,
    MSG_GOV_RESPONSE,
    MSG_PEER_LIST,
    MSG_CONSENSUS_SYNC
} MessageType;

typedef struct {
    uint32_t type;
    int sender_id;
    int sender_type;
    double amount;
    uint32_t ticket_number;     // turno Bakery local o version de consenso
    uint32_t connection_order;  // orden en que se establecio la conexion P2P
    char payload[256];
} MessagePacket;

typedef struct {
    _Atomic bool choosing[MAX_THREADS];
    _Atomic uint32_t number[MAX_THREADS];
    _Atomic int thread_count;
} BakeryLock;

typedef struct {
    MessagePacket packet;
    uint32_t connection_order;
    uint32_t ticket_number;
} PendingTransaction;

typedef struct {
    PendingTransaction queue[MAX_PENDING_TX];
    int head;
    int tail;
    int count;
} PendingTxQueue;

typedef struct {
    int store_id;
    StoreType type;
    int listen_port;
    double max_revenue_limit;
    char target_peer_ip[64];
    int target_peer_port;
    char log_filename[128];
} StoreConfig;

typedef struct {
    double own_revenue;
    double chain_revenue;
    double max_revenue_limit;
    StoreStatus status;
    uint32_t current_ticket;
    BakeryLock lock;
    PendingTxQueue pending_tx;
    bool should_exit;
} StoreState;

typedef struct {
    int socket_fd;
    struct sockaddr_in address;
    bool is_active;
    int peer_id;
    StoreType peer_type;
    uint32_t connection_order;
} PeerNode;

typedef struct {
    PeerNode peers[MAX_PEERS];
    int total_peers;
    uint32_t connection_counter;
    BakeryLock lock;
} PeerTable;

typedef struct {
    StoreConfig config;
    PeerTable peer_table;
    StoreState state;
    int log_fd;
    uint32_t consensus_version;
    double last_consensus_chain_revenue;
    pthread_t workers[MAX_WORKERS];
    _Atomic int worker_count;
    _Atomic bool cerrado;
} AppContext;

typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
    AppContext *app_ctx;
} WorkerContext;

typedef struct {
    AppContext *app_ctx;
    int listen_fd;
} ListenerContext;

static inline MessagePacket nuevo_paquete(uint32_t tipo, int id, int tipo_tienda) {
    MessagePacket p = {0};
    p.type = tipo;
    p.sender_id = id;
    p.sender_type = tipo_tienda;
    return p;
}

#endif