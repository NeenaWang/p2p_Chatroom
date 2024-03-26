#ifndef P2P_H
#define P2P_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <math.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>




#define NUM_SERVERS 4
#define MAX_MSG_LEN 200
#define MAX_MSGS 1000
#define ROOT 20000
#define TRUE 	 1
#define FALSE 	 0
#define HEADS 1
#define TAILS 0
#define ANTI_INTERVAL 10



// define the type of messages
// rumor
// status
enum message_type {
    STATUS=0,
    RUMOR
} message_type;

// define the command type
enum command_type{
    MSG = 0,
    GET,
    CRASH,
    EXIT
} command_type;

// define the message struct using message_type
typedef struct message {
    enum message_type type;
    uint16_t message_len;
    // server id of the sending server 
    // eg a->b : a's id
    uint16_t server_id;
    // eg a->b->c : a's id a is the original server
    uint16_t original_sender_id; 
    // seq of message from the same origin
    // checking message loss, duplication...
    uint16_t seqnum;
    // every server is maintaining a vector clock
    uint16_t vector_clock[NUM_SERVERS];
    // message itself
    char msg[MAX_MSG_LEN]; 
} received_message;

// define the command struct using command_type
typedef struct client_command{
    enum command_type cmd_type;
    uint16_t msg_id;
    char msg[MAX_MSG_LEN];
} client_command;

// functions
void timeout_hdler(int signum);
int interpretCommand(char *cmd_data, client_command *cmd);
int get_potenial_neighbors(int curID, int totalPeers, int *isPotential);
int get_ran_neighbors(int numNeighbors);
void get_neighbors_port(int curID, int totalPeers, int neighbor_ports[]);
int get_neighbors_index(uint16_t neighborID, uint16_t serverID, int numNeighbors);
void updateVectorclock(uint16_t *vector_clock, uint16_t **msg_ids, size_t num_msg,
                         uint16_t new_msg_server, int totalPeers);
void buildChatLog(char **msg_log, size_t num_msg, char *chat_log);
int search_msg (uint16_t **msg_ids, size_t num_msg, uint16_t target_server, uint16_t target_seqnum);
size_t log_new_msg(char *new_msg, uint16_t serverID, uint16_t seq, char **msg_log,  size_t num_msg, uint16_t **msg_ids,  uint16_t *vector_clock, int totalPeers);
size_t update_log (received_message *msg, char **msg_log, size_t num_msg, uint16_t **msg_ids,  uint16_t *vector_clock,  int totalPeers);
void prepareMessage(struct message *msgBuffer, enum message_type type, uint16_t fromServerID, uint16_t originID, uint16_t sequenceNum, uint16_t *currentVectorClock, const char *messageContent, size_t totalServers);
int assessSync(int *unsync_msg, received_message *received_msg, uint16_t *local_vector_clock, int totalPeers);
void print_vector_clock(uint16_t *vector_clock, int num_procs);
void print_message(received_message *msg, int num_procs);
// int newClientPre(int listening_socket);
// debug add
// void handle_tcp_connection(int client_socket);
// void process_udp_datagram(int udp_socket);
#endif
