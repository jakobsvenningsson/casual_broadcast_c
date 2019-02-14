
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "array.h"


struct node_state {
    int node_id;
    int socket;
    int nr_of_broadcasts;
    int * delivered_clock;
    int * node_id_to_port_map;
    int nr_of_nodes;
    Array * pending;
    Array * delivered;
} node_state;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Helper methods

// Returns 1 if every single entry in local_clock array is greater than its corresponding entry in the msg_clock array, otherwise 0. 
// This means that we have delivered every message that causally preceded the message.
int can_deliver(int local_clock[], int msg_clock[], size_t size) {
    for (int n = 0; n < size; ++n) {
        if(msg_clock[n] > local_clock[n]) {
            return 0;
        }
    }
    return 1;
}

struct sockaddr_in get_addr(int port, char addr_str[]) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(addr_str);
    addr.sin_port = htons(port);
    return addr;
}


int init_socket(int src_port) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if(s < 0) {
        printf("Failed to initiate socket.\n");
        exit(1);
    }
    struct sockaddr_in src_addr = get_addr(src_port, "127.0.0.1");
    if (bind(s, (struct sockaddr *) &src_addr, sizeof(src_addr)) < 0) {
        printf("Failed to bind socket to port.\n");
        exit(1);
    }
    return s;
}

int is_delivered(Array delivered, struct pending_message msg) {
    for(int i = 0; i < delivered.size; ++i) {
        struct pending_message d = delivered.array[i];
        if(d.seq == msg.seq && d.src == msg.src){
            return 1;
        }
    }
    return 0;
}

void deliver_messages_if_possible(Array * pending, Array * delivered, int * delivered_clock, int nr_of_nodes) {
    while(1) {
        for(int i = 0; i < pending->size; ++i) {
            struct pending_message msg = pending->array[i];
            if(can_deliver(delivered_clock, msg.clock, nr_of_nodes) && is_delivered(*delivered, msg) == 0) {
                printf("Deliver msg: %d from %d\n", msg.seq, msg.src);
                delivered_clock[msg.src] += 1;
                insertArray(delivered, msg);
                // Delete message pending and free allocated memory
                deleteArray(pending, msg);
                free(msg.clock);
                // Check if we can deliver any other messages
                deliver_messages_if_possible(pending, delivered, delivered_clock, nr_of_nodes);
                return;
            }
        }
        // We have looped through every single message without being able to deliver -> exit.
        break;
    }
}

int * init_delivered_clock(int n) {
    int *delivered_clock = malloc(sizeof(int) * n);
    for(int i = 0; i < n; ++i) {
        delivered_clock[i] = 0;
    }
    return delivered_clock;
}

struct node_state * init_node_state(int socket, Array * pending, Array * delivered, int * node_id_to_port_map, int nr_of_nodes, int node_id, int nr_of_broadcasts) {
    int * delivered_clock = init_delivered_clock(nr_of_nodes);
    struct node_state *state = malloc(sizeof(struct node_state));
    state->socket = socket;
    state->delivered_clock = delivered_clock;
    state->pending = pending;
    state->delivered = delivered;
    state->nr_of_nodes = nr_of_nodes;
    state->node_id_to_port_map = node_id_to_port_map;
    state->node_id = node_id;
    state->nr_of_broadcasts = nr_of_broadcasts;
    return state;
}

void cleanup(struct node_state * state) {
   freeArray(state->delivered);
   freeArray(state->pending);
   free(state->delivered_clock);
   free(state->node_id_to_port_map);
   free(state);
}

// main methods

void * udp_listen_routine(void * arg) {
    printf("Listening thread started.\n");

    struct node_state * state = ((struct node_state *) arg);
    Array * pending = state->pending;
    Array * delivered = state->delivered;
    int * delivered_clock = state->delivered_clock;

    int nr_delivered = 0;
    while(nr_delivered < (state->nr_of_broadcasts * state->nr_of_nodes)) {
        struct sockaddr_in client;
        int len = sizeof(client);

        int msg_size = (2 + state->nr_of_nodes) * sizeof(int);
        int msg[msg_size];

        recvfrom(state->socket, msg, msg_size, 0, (struct sockaddr *)&client, (socklen_t *)&len);
        
        pthread_mutex_lock(&mutex);

        struct pending_message pending_msg; 
        pending_msg.seq = msg[1];
        pending_msg.src = msg[0];
        pending_msg.clock = malloc(state->nr_of_nodes * sizeof(int));
        memcpy(pending_msg.clock, msg + 2, state->nr_of_nodes * sizeof(int));

        insertArray(pending, pending_msg);
        deliver_messages_if_possible(pending, delivered, delivered_clock, state->nr_of_nodes); 
        pthread_mutex_unlock(&mutex);

        nr_delivered++;
    }
    return NULL;
}

void * broadcast(void * arg) {
    // wait for all processes to initialize
    sleep(1);
    struct node_state * state = ((struct node_state *) arg);
    int seq = 0; 
    int n = state->nr_of_broadcasts;
    int broadcast_msg_size = 2 + state->nr_of_nodes * sizeof(int);

    for (int i = 0; i < n; ++i) {
        pthread_mutex_lock(&mutex);
        int * tmp_delivered_clock = malloc(state->nr_of_nodes * sizeof(int));
        memcpy(tmp_delivered_clock, state->delivered_clock, state->nr_of_nodes * sizeof(int));
        tmp_delivered_clock[state->node_id] = seq;
        for(int d = 0; d < state->nr_of_nodes; ++d) {
            struct sockaddr_in dst_addr = get_addr(state->node_id_to_port_map[d], "127.0.0.1");
            // Construct message
            int broadcast_msg[broadcast_msg_size];
            broadcast_msg[1] = seq;
            broadcast_msg[0] = state->node_id;
            memcpy(broadcast_msg + 2, tmp_delivered_clock, sizeof(int) * state->nr_of_nodes);
            if (sendto(state->socket, broadcast_msg, sizeof(broadcast_msg), 0, (struct sockaddr *) &dst_addr, sizeof(dst_addr)) < 0) {
                    printf("Failed to send socket msg.\n");
                    exit(1);
            }
        }
        free(tmp_delivered_clock);
        seq += 1;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int * read_process_file(char * file_name) {
    //Create file path
    char str[strlen(file_name) + 2];
    sprintf(str, "./%s", file_name);

    FILE * fp;
    fp =fopen(str, "r");

    char * line = NULL;
    size_t len = 0;
    getline(&line, &len, fp);
    
    int nr_of_nodes = atoi(line);
    int * node_id_to_port_map = malloc(sizeof(int) * nr_of_nodes);

    for(int i = 0; i < nr_of_nodes; ++i) {
        getline(&line, &len, fp);
        char * id = strtok (line," ");
        char * port = strtok (NULL," ");
        port[strlen(port)-1] = '\0';
        node_id_to_port_map[atoi(id)] = atoi(port);
    }
    return node_id_to_port_map;
}

int main( int argc, char *argv[] )  {
    if(argc < 3) {
        // Wrong number of arguments
        printf("Please supply a process file and a node id.\n");
        exit(1);
    }
    // Pending will hold all messages which are waiting to be delivered because of out of order delivery.
    Array pending;
    initArray(&pending, 20);
    Array delivered;
    initArray(&delivered, 20);

    int node_id = atoi(argv[1]);
    int nr_of_broadcasts = atoi(argv[3]);
    int * node_id_to_port_map = read_process_file(argv[2]);
    size_t nr_of_nodes = sizeof(node_id_to_port_map)/sizeof(int);
    int socket = init_socket(node_id_to_port_map[node_id]);
    struct node_state * state = init_node_state(socket, &pending, &delivered, node_id_to_port_map, nr_of_nodes, node_id, nr_of_broadcasts);

    pthread_t broadcast_thread;
    pthread_create(&broadcast_thread, NULL, broadcast, (void *) state);

    pthread_t listen_thread;
    pthread_create(&listen_thread, NULL, udp_listen_routine, (void *) state);

    pthread_join(listen_thread, NULL);

    cleanup(state);
    
    return 0;
}
