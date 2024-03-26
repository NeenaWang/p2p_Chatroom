#include "p2p.h"

int i;

// need to parse client commands and do actions
int interpretCommand(char *cmd_data, client_command *cmd){
    // command has four possible types

    // tokenize the string
    // divides string into tokens

    // crash for testing the resilience and fault tolerance
    // strcmp to check if strictly identical(==)
    if (strcmp(cmd_data, "crash\n")==0){
        // cmd is a ppointer to the command
        cmd->cmd_type = CRASH;
        return CRASH;
    }
    char* cmd_token=strtok(cmd_data, " ");

    if (strcmp(cmd_token, "msg")==0){
        // message id , message type and data itself based on cmd struct

        // set the type as MSG
        cmd -> cmd_type = MSG;
        // set the message id 
        cmd_token = strtok(NULL, " ");
        long m_id = strtol(cmd_token, NULL, 10);
        cmd->msg_id = (uint16_t) m_id; // transfer and store id 
        // set the message data 

        cmd_token = strtok(NULL, "\0");
        if (strlen(cmd_token)==0){
            return -1;
        }
        strcpy(cmd->msg, cmd_token); // copy to struct and set data 

        return MSG;
    }
    else if(strcmp(cmd_token, "get")==0){
        cmd->cmd_type = GET;
        return GET;
    }
    else {
        perror("Incorrect command type");
        return -1;
    }
}

/**
 * get neighbors.
 * get neighbor id.
 * compute the index.
*/

// get potantial neighbors and the number
    // current id for server is curID
    // thread number is totalPeers
    // wether potential: isPotential
int get_potenial_neighbors(int curID, int totalPeers, int *isPotential){
    // initialize (0 or 1)
    // 0 is left and 1 is right
    int num_neighbors = 0;
    if (curID > totalPeers){
        return 0;
    }
    if (curID > 0){
        isPotential[0]=TRUE;
        num_neighbors++;
    }
    if (curID < totalPeers - 1){
        isPotential[1] = TRUE;
        num_neighbors++;
    }
    return num_neighbors;
    // int i;
    // for (i = 0; i < 2; i++) {
    //     isPotential[i] = FALSE;
    // }

    // current node is not in valid range 
    // if (curID<0 || curID >=totalPeers){
    //     return 0;
    // }

    // // left is True 
    // if (curID >0){
    //     isPotential[0]=TRUE;
    //     num_neighbors++;
    // }

    // // right is True 
    // if (curID<totalPeers-1){
    //     isPotential[1]=TRUE;
    //     num_neighbors++;
    // }

    // return num_neighbors;
}

// select randomly a peer neighbor among potential neighbors
int get_ran_neighbors(int numNeighbors){
    // no valid left and right neighbors
    if (numNeighbors==1){
        return 0;
    }
    // select left or right
    return rand()%2;
}

// cur port->neighbor port
// set port for any neighbors
void get_neighbors_port(int curID, int totalPeers, int neighbor_ports[]){
    // cur id is the first one 
    // for (i = 0; i < 2; i++) {
    //     neighbor_ports[i] = -1; 
    // }

    if (curID == 0){
        // only right
        neighbor_ports[0] = 20000 + curID + 1;
        // left is incorrect
        return;
    }
    // is the last one 
    if (curID == totalPeers-1){
        // has left neighbor but not right
        neighbor_ports[0] = 20000 + curID - 1;
        return;
    }
    // in the middle
    else{
        neighbor_ports[0] = 20000 + curID - 1;
        neighbor_ports[1] = 20000 + curID + 1;
        return;
    }
}

// ports (cur and neighbor)->index
// set index for neighbors based on pid and neighbors's pid
int get_neighbors_index(uint16_t neighborID, uint16_t serverID, int numNeighbors){
    if (numNeighbors==1){
        return 0;
    }else{
        // compute the index
        return fmax(0, (int)neighborID - serverID);
    }
}

/**
 * send message
 * receive and process coming message.
*/

// after receiving new message, update vector clock
void update_vector_clock(uint16_t *vector_clock, uint16_t **msg_ids, size_t num_msg,
                         uint16_t new_msg_server, int totalPeers){
    
    // mark wether sequence number received 
    uint16_t if_received[MAX_MSGS+1];
    int count = 0;
    memset(if_received, 0, sizeof(if_received));

    int i;
    // iterate all messages and update ids
    // mark received msg from this server
    for (i = 0; i<num_msg; i++) {
        // check if is from new server
        if (msg_ids[i][0] == new_msg_server) {
            // mark as received
            if_received[msg_ids[i][1]-1] = TRUE;
            count++;
        }
    }

    // find the  longest sequence received from the server
    for (i = 0; i<count+1; i++){
        if (if_received[i]==FALSE){
            break;
        }
    }

    // update vector clock for this new server
    vector_clock[new_msg_server] = i+1;

    return;
}

// store all msg 
// format it clean and with a preflix "chatLog"
void buildChatLog(char **msg_log, size_t num_msg, char *chat_log){
    // initialize chatlog
    memset(chat_log, 0, sizeof(*chat_log));
    // add prefix
    strcat(chat_log, "chatLog ");
    // iterate all messages
    int cur_len, i;
    for (i=0; i<num_msg; i++){
        // current message and length
        char *cur = msg_log[i];
        cur_len = strlen(cur);
        
        // make it clean
        if (cur[cur_len-1] == '\n') {
            cur[strlen(cur)-1] = 0;
        }
        if (i>0){
            // add ',' to divide
            strcat(chat_log, ",");
        }
        // store cur in chat log 
        strcat(chat_log, cur);
    }
    // add ending \n
    strcat(chat_log, "\n");
}

// return index of msg in message log
// target server and sequence -> index in num_msg
int search_msg (uint16_t **msg_ids, size_t num_msg, uint16_t target_server, uint16_t target_seqnum)
{
    int i;
    for (i=0; i<num_msg; i++){
        if (msg_ids[i][0] == target_server && msg_ids[i][1] ==target_seqnum){
            return i;
        }
    }
    // not found
    return -1;
}

size_t log_new_msg(char *new_msg, uint16_t serverID, uint16_t seq, char **msg_log,  size_t num_msg, uint16_t **msg_ids,  uint16_t *vector_clock, int totalPeers){
    
    // Check if message already exists
    // if (search_msg(msg_ids, num_msg, serverID, seq) != -1) {
    //     // exists
    //     return 0;
    // }

    // store data and id
    char *msg_text = malloc(MAX_MSG_LEN* sizeof(char));
    uint16_t *msg_id = malloc(2*sizeof(uint16_t));

    // copy data 
    strncpy(msg_text, new_msg, strlen(new_msg));
    // update msg_id
    msg_id[0] = serverID;
    msg_id[1] = seq;
    // single msg_text and msg_id -> update logs and ids 
    msg_log[num_msg] = msg_text;
    msg_ids[num_msg] = msg_id;

    // update vector clock
    update_vector_clock(vector_clock, msg_ids, num_msg + 1, msg_id[0],totalPeers);

    //return count of msg 
    return 1;
}

// when receive new message 
// -> decide wether to update log and ids
size_t update_log (received_message *msg, char **msg_log, size_t num_msg, uint16_t **msg_ids,  uint16_t *vector_clock,  int totalPeers){
    
    int expected_seq = vector_clock[msg->original_sender_id];

    // if  received message is not expected 
    if (msg->seqnum!=expected_seq){
        // left behind
        if(msg->seqnum<expected_seq){
            // msg has been received
            return 0;
        }
        else {
            // do we have the msg? 
            // if search in msg nums -> have -> do nothing
            int index_msg = search_msg (msg_ids, num_msg, msg->original_sender_id, msg->seqnum);
            if (index_msg>=0){
                // have this message
                return 0;
            }
        }
    }
    // do not have this message->store 
    char *msg_data = malloc(MAX_MSG_LEN* sizeof(char));
    uint16_t *msg_id = malloc(2*sizeof(uint16_t));

    // copy received message into "msg_data"
    strncpy(msg_data,msg->msg, msg->message_len);
    // update id(server and sequence number)
    msg_id[0] = msg->original_sender_id;
    msg_id[1] = msg->seqnum;
    // update log 
    msg_log[num_msg]=msg_data;
    // update msg_ids ([num][0&1])
    msg_ids[num_msg] = msg_id;

    // update vector clock
    update_vector_clock(vector_clock, msg_ids, num_msg + 1, msg_id[0], totalPeers);
    // if it is new, return 1
    return 1;
}

void prepareMessage(struct message *msgBuffer, enum message_type type, uint16_t fromServerID, uint16_t originID, uint16_t sequenceNum, uint16_t *currentVectorClock, const char *messageContent, size_t totalServers) 
{
    // Initialize the message buffer to 0
    msgBuffer->type = type;
    msgBuffer->server_id = fromServerID; 
    msgBuffer->original_sender_id = originID;
    msgBuffer->seqnum = sequenceNum;

    //Copy vector clock into the message
    memcpy(msgBuffer->vector_clock, currentVectorClock, sizeof(currentVectorClock[0])*MAX_MSG_LEN);

    if (type == STATUS){
        msgBuffer->message_len = 0;
        memset(msgBuffer->msg, '\0', MAX_MSG_LEN);
    }
    else{
        msgBuffer->message_len = strlen(messageContent);
        strcpy(msgBuffer->msg, messageContent);
    }
    return;
}


// receiving status message
// returns an integer indicating the synchronization state
// case 1 sender is left behind by the local nodes 
// case 2 local nodes is left behind
int assessSync(int *unsync_msg, received_message *received_msg, uint16_t *local_vector_clock, int totalPeers){
    // initialize ststus as False
    int if_need_msg = FALSE;
    // received vector clock with messages
    // get by received message(struct has vector clock)
    // every peer maintain a vector_clock, so length is totalPeers
    uint16_t rcvd_status[totalPeers];
    memcpy(rcvd_status, received_msg->vector_clock, sizeof(local_vector_clock[0]*totalPeers));

    // update every peers' vector clock
    int i;
    for (i=0; i<totalPeers; i++){
        // case 1  update sender
        if (local_vector_clock[i] > rcvd_status[i]){
            // for local node, store unsync node info
            //  index and vector clock from received msg
            unsync_msg[0] = i;
            unsync_msg[1] = rcvd_status[i];
            return 1;
        }
        // case 2 receive
        else if (local_vector_clock[i] < rcvd_status[i]){
            if_need_msg=TRUE;
        }
    }
    // if unsync occurs
    if (if_need_msg==TRUE){
        return  -1;
    }
    else{
        return 0;
    }
}

void print_vector_clock(uint16_t *vector_clock, int num_procs){
    int i;
    for (i = 0; i<num_procs; i++){
        printf("Server: %d. Next Seqnum: %d\n",i,vector_clock[i]);
    }
    printf("\n");
}

void print_message(received_message *msg, int num_procs){
    printf("Message Type: %d\n"
           "Origin Server: %d\n"
           "From Server: %d\n"
           "sequence Number:%d\n"
           "message_t contents: %s\n", msg->type, msg->original_sender_id, msg->server_id,msg->seqnum, msg->msg);
    printf(" Status vector from message_t:\n");
    print_vector_clock(msg->vector_clock, num_procs);
    return;
}

