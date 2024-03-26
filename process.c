#include "p2p.h"

volatile sig_atomic_t anti_entropy = FALSE;
// cmd_arg_c is argument count in cmd
// cmd_arg_vector is arguments from cmd

// debug select

int main(int cmd_arg_c, char *cmd_arg_vector[])
{
    printf("Check: Starting the server...\n");

    srand(time(0));
    int curID = atoi(cmd_arg_vector[1]);
    int totalPeers = atoi(cmd_arg_vector[2]);
    int tcp_port = atoi(cmd_arg_vector[3]);
    int udp_port = ROOT + curID;
    int local_seq = 1;
    int cmd_type;
    int tcp_socket, udp_socket, new_tcp_socket;
    int i, j, neighbor_index, read_bytes, status;
    size_t num_msgs = 0;
    int next_msg[2];
    int active = TRUE;
    int gossip = TRUE;
    char read_buffer[1025], incoming_message[256];
    struct sockaddr_in client_addr, tcp_addr, client_address, udp_addr, neighbor_addr;
    fd_set read_fds;
    int is_potential[2], num_neighbors;
    uint16_t vector_clock[totalPeers];

    // Allocating memory for messages, IDs, and command buffer
    char **msg_log = (char **)malloc(MAX_MSGS * sizeof(char *));
    uint16_t **msg_ids = (uint16_t **)malloc(MAX_MSGS * sizeof(uint16_t *));
    char chat_log_out[(MAX_MSG_LEN + 1) * MAX_MSGS];
    struct message *neighbor_msg_buf = malloc(sizeof(struct message));
    struct message *udp_peer_msg_buf = malloc(sizeof(struct message));
    struct client_command *cmd_buf = malloc(sizeof(struct client_command));

    signal(SIGALRM, timeout_hdler);

    // Initialize vector clock
    for (i = 0; i < totalPeers; i++)
    {
        vector_clock[i] = (uint16_t)1;
    }

    // Setup TCP socket
    tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0)
    {
        perror("TCP socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("TCP socket created successfully\n");

    // Setup TCP address
    memset(&tcp_addr, 0, sizeof(struct sockaddr_in));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(tcp_port);

    int tcp_addr_len = sizeof(tcp_addr);

    if (bind(tcp_socket, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) < 0)
    {
        perror("TCP bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(tcp_socket, 3) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Accepting new connections
    new_tcp_socket = accept(tcp_socket, (struct sockaddr *)&client_address, (socklen_t *)&tcp_addr_len);
    if (new_tcp_socket < 0)
    {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    // Set TCP socket to non-blocking mode
    status = fcntl(new_tcp_socket, F_SETFL, fcntl(new_tcp_socket, F_GETFL, 0) | O_NONBLOCK);
    if (status == -1)
    {
        perror("Setting TCP socket to non-blocking failed");
        exit(EXIT_FAILURE);
    }

    // Setup UDP socket
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0)
    {
        perror("UDP socket creation failed");
        exit(EXIT_FAILURE);
    }
    memset(&udp_addr, 0, sizeof(struct sockaddr_in));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_addr.sin_port = htons(udp_port);

    if (bind(udp_socket, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) < 0)
    {
        perror("UDP bind failed");
        exit(EXIT_FAILURE);
    }
    printf("UDP socket created and bound successfully\n");

    num_neighbors = get_potenial_neighbors(curID, totalPeers, is_potential);
    int neighbor_ports[num_neighbors];
    get_neighbors_port(curID, totalPeers, neighbor_ports);

    printf("SERVER STARTED AND WAITING!\n");
    alarm(ANTI_INTERVAL);

    while (active == TRUE)
    {

        // decided by timeout_hdler
        if (anti_entropy == TRUE)
        {

            printf("Server %d performing anti-entropy\n", curID);

            // 1. get destination addr
            // pick a neighbor randomly
            neighbor_index = get_ran_neighbors(num_neighbors);
            neighbor_addr.sin_family = AF_INET;
            neighbor_addr.sin_addr.s_addr = INADDR_ANY;
            neighbor_addr.sin_port = htons(neighbor_ports[neighbor_index]);

            // 2. fill out a message struct which is null, len is 0
            prepareMessage(neighbor_msg_buf, STATUS, curID, curID, 0, vector_clock, msg_log[0], totalPeers);
            // send and error check
            // sending msg in anti_entropy is by udp (peer to peer)
            printf("********** (ANTI ENT) Sending ANTI ENT STATUS Message from UDP port: %d to port: %d\n ", udp_port, ntohs(neighbor_addr.sin_port));
            // 3. send msg
            // size_t msg_size = sizeof(received_message);
            // ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t)
            // neighbor_addr is the destination addr
            // sizeof(neighbor_addr) : how much  of the destination addr to consider
            ssize_t bytes_sent = sendto(udp_socket, (const char *)neighbor_msg_buf, (socklen_t)sizeof(received_message), 0, (struct sockaddr *)&neighbor_addr, sizeof(neighbor_addr));
            if (bytes_sent != sizeof(struct message))
            {
                perror("anti|sendto failed");
            }
            else
            {
                printf("anti|Sent %zd bytes to neighbor\n", bytes_sent);
            }

            // check2 sending msg:
            printf("Sending message: '%s' from Server %d to Server %d\n", cmd_buf->msg, curID, neighbor_index);

            // 4. update status and reset the alarm
            anti_entropy = FALSE;
            alarm(ANTI_INTERVAL);
        }

        if (gossip == TRUE)
        {

            // check4 gossip
            printf("Server %d performing gossip\n", curID);

            gossip = FALSE;
            for (j = 0; j < num_neighbors; j++)
            {
                printf("j: %d", j);
                neighbor_addr.sin_family = AF_INET;
                neighbor_addr.sin_addr.s_addr = INADDR_ANY;
                neighbor_addr.sin_port = htons(neighbor_ports[j]);

                // fill message, send and check error
                printf("prepare MSG...\n");
                prepareMessage(neighbor_msg_buf, STATUS, curID, curID, 0, vector_clock, msg_log[0], totalPeers);
                printf("********** (GOSSIP)Sending GOSSIP STATUS Message from UDP port: %d to port: %d\n ", udp_port, ntohs(neighbor_addr.sin_port));
                // size_t msg_size = sizeof(received_message);
                ssize_t bytes_sent = sendto(udp_socket, (const char *)neighbor_msg_buf, (socklen_t)sizeof(received_message), 0, (struct sockaddr *)&neighbor_addr, sizeof(neighbor_addr));
                if (bytes_sent != sizeof(struct message))
                {
                    perror("GOSSIP|sendto failed\n");
                }
                else
                {
                    printf("GOSSIP|Sent %zd bytes to neighbor\n", bytes_sent);
                }
                printf("end gossip loop...\n");
            }
            printf("out of gossip loop... \n");
        }

        if (vector_clock[curID] > local_seq)
        {
            // update local seq
            local_seq = vector_clock[curID];
            printf("update vector clock...\n");
        }
        else
        {
            printf("non-update vector clock\n");
        }

        printf("SERVER %d AWAITING INPUT\n\n", curID);

        if (num_msgs > MAX_MSGS)
        {
            perror("out of msg limit\n");
            break;
        }
        else
        {
            printf("in limit...\n");
        }

        // FD_ZERO(&read_fds);
        // // add socket into file set
        // FD_SET(new_tcp_socket, &read_fds);
        // FD_SET(udp_socket, &read_fds);

        // printf("Before Select...\n");
        // // check all socket
        // // FD_SETSIZE is not change, max tracking number
        // // return number of socket that can read and write
        // if (select(FD_SETSIZE, &read_fds, NULL, NULL, NULL) < 0)
        // {
        //     // network error
        //     if (errno == EINTR)
        //     {
        //         perror("select interupted");
        //     }
        //     else
        //     {
        //         perror("Other error....");
        //         exit(EXIT_FAILURE);
        //     }
        // }
        // printf("After Select...\n");
        // Iterate through file descriptors to find the ones that are ready

        // handle incoming TCP&UDP connections from client
        // loop all socket by iterate FD_SETSIZE
        // new_tcp_socket = accept(tcp_socket, (struct sockaddr *)&client_address, (socklen_t *)&tcp_addr_len);
        // if (new_tcp_socket < 0) {
        //     perror("Accept failed");
        //     exit(EXIT_FAILURE);
        // }

        FD_ZERO(&read_fds);
        FD_SET(new_tcp_socket, &read_fds);
        FD_SET(udp_socket, &read_fds);

        printf("Before Select...\n");
        // check all socket
        // FD_SETSIZE is not change, max tracking number
        // return number of socket that can read and write
        if (select(FD_SETSIZE, &read_fds, NULL, NULL, NULL) < 0)
        {
            // network error
            if (errno == EINTR)
            {
                perror("select interupted");
            }
            else
            {
                perror("Other error....");
                exit(EXIT_FAILURE);
            }
        }
        printf("After Select...\n");

        printf("Before TCP&UDP...\n");
        for (i = 0; i < FD_SETSIZE; ++i)
        {
            if (FD_ISSET(i, &read_fds))
            {
                // case 1 : TCP socket
                if (i == new_tcp_socket)
                {
                    printf("Server %d: Received a client message on TCP\n", curID);
                    // read data from a file(tcp socket) descriptor into a buffer
                    // return number of bytes that were read into the buffer
                    // read_bytes = read(new_tcp_socket, read_buffer, 1024);
                    // printf("read_type: %d\n", read_bytes);
                    if ((read_bytes = read(new_tcp_socket, read_buffer, 1024)) == 0)
                    {
                        // case 1.2 read return 0 -> connection close
                        // server ->get client's name
                        int client_addr_len = sizeof(client_addr);
                        // use getpeername() on a socket within a server application, the "peer" is  the entity on the other end of that socket's connection.
                        getpeername(new_tcp_socket, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
                        printf("Client disconnected - IP: %s, Port: %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                        close(new_tcp_socket);
                        // debug
                        // new_tcp_socket = -1;
                    }
                    else if (read_bytes == -1)
                    {
                        perror("TCP Read Errors");
                    }
                    else
                    {
                        // case 1.3 valid

                        // data is read from tcp to read_buffer
                        read_buffer[read_bytes] = '\0';
                        // copy into incoming message(command string)
                        strcpy(incoming_message, read_buffer);

                        // cmd_type stores return value and cmd_buf stores details
                        printf("Received command: %s\n", incoming_message);
                        // printf("Interpreted command type: %d\n", cmd_type);

                        if ((cmd_type = interpretCommand(incoming_message, cmd_buf)) == -1)
                        {
                            printf("TCP|cmd...error\n");
                        }

                        if (cmd_type == CRASH)
                        {
                            active = FALSE;
                            break;
                        }
                        // send back to client as a chatlog(tcp socket)
                        else if (cmd_type == GET)
                        {
                            printf("***************************************\nGET occur...\n");
                            buildChatLog(msg_log, num_msgs, chat_log_out);
                            printf("Chat log:\n%s\n", chat_log_out);
                            printf("Sending chat log, length: %lu\n", strlen(chat_log_out));

                            ssize_t send_res;
                            send_res = send(new_tcp_socket, chat_log_out, strlen(chat_log_out), 0);
                            // printf("sent data is  %zd bytes.\n", send_res);

                            if (send_res != strlen(chat_log_out))
                            {
                                printf("send error...\n");
                            }
                        }
                        // send to neighbor (random) as a rumor(udp socket)
                        else if (cmd_type == MSG)
                        {
                            printf("***************************************\nMSG occur...\n");

                            num_msgs += log_new_msg(cmd_buf->msg, curID, local_seq, msg_log, num_msgs, msg_ids, vector_clock, totalPeers);
                            printf("Logged new message: '%s'\n", cmd_buf->msg);
                            printf("Current number of messages: %zu\n", num_msgs);
                            // create new messages(RUMOR)
                            prepareMessage(neighbor_msg_buf, RUMOR, curID, curID, local_seq, vector_clock, cmd_buf->msg, totalPeers);

                            printf("Prepared RUMOR message for sending.\n");
                            local_seq++;

                            // send to neighbor peer
                            // step 1 get neighbor index
                            neighbor_index = get_ran_neighbors(num_neighbors);
                            printf("Selected neighbor index for sending: %d\n", neighbor_index);

                            neighbor_addr.sin_family = AF_INET;
                            neighbor_addr.sin_addr.s_addr = INADDR_ANY;
                            neighbor_addr.sin_port = htons(neighbor_ports[neighbor_index]);
                            printf("********** (cmd_type == MSG) Sending MSG: RUMOR Message from to port: %d\n ", ntohs(neighbor_addr.sin_port));

                            // step 2 send
                            sendto(udp_socket, (const char *)neighbor_msg_buf, sizeof(struct message), MSG_DONTWAIT,
                                   (const struct sockaddr *)&neighbor_addr, sizeof(neighbor_addr));
                        }
                    }
                }
                // case 2 : UDP socket
                else if (i == udp_socket)
                {
                    struct sockaddr_in neighbor_addr;
                    memset(&neighbor_addr, 0, sizeof(neighbor_addr));
                    int stat, msg_idx;
                    socklen_t len = sizeof(neighbor_addr);
                    // fail to receive udp socket
                    // store sending server in peer_addr, data in udp_peer_msg_buf
                    int receive_status = recvfrom(udp_socket, udp_peer_msg_buf, sizeof(struct message), MSG_DONTWAIT, (struct sockaddr *)&neighbor_addr, &len);
                    if (receive_status == -1)
                    {
                        perror("udp|error\n");
                        break;
                    }

                    // check3 receive msg
                    printf("********** --- ***** : UDP Server %d received message: '%s' from Server %d\n", curID, udp_peer_msg_buf->msg, udp_peer_msg_buf->server_id);

                    // check message type
                    enum message_type udp_msg_type = udp_peer_msg_buf->type;
                    // enum message_type udp_msg_type = udp_peer_msg_buf->type;

                    // if get rumor from a neighbor 0, then continue to send rumor to another neighbor(1)
                    if (udp_msg_type == RUMOR)
                    {
                        // update return 0 or 1 (new msg number)
                        j = update_log(udp_peer_msg_buf, msg_log, num_msgs, msg_ids, vector_clock, totalPeers);
                        num_msgs += j;

                        // when the rumor is new
                        if (j == 1)
                        {
                            // only when neighbor number >1 will node send rumor
                            // end until server is at the end
                            if (num_neighbors > 1)
                            {
                                // get_neighbors_index return the server sending message
                                // get the index of neighbor that don't send msg
                                j = get_neighbors_index(udp_peer_msg_buf->server_id, curID, num_neighbors);
                                j = 1 - j;

                                neighbor_addr.sin_family = AF_INET;
                                neighbor_addr.sin_addr.s_addr = INADDR_ANY;
                                neighbor_addr.sin_port = htons(neighbor_ports[j]);

                                prepareMessage(neighbor_msg_buf, STATUS, curID, curID, 0, vector_clock, msg_log[0], totalPeers);
                                printf("********** (j == 1 , num_neighbors > 1)Sending STATUS Message from UDP port: %d to port: %d\n ", udp_port, ntohs(neighbor_addr.sin_port));

                                if (sendto(udp_socket, (const char *)neighbor_msg_buf, sizeof(received_message), MSG_DONTWAIT, (const struct sockaddr *)&neighbor_addr, sizeof(neighbor_addr)) != sizeof(struct message))
                                {
                                    perror("UDP| send fail");
                                }
                            }
                        }

                        j = get_neighbors_index(udp_peer_msg_buf->server_id, curID, num_neighbors);

                        neighbor_addr.sin_family = AF_INET;
                        neighbor_addr.sin_addr.s_addr = INADDR_ANY;
                        neighbor_addr.sin_port = htons(neighbor_ports[j]);

                        prepareMessage(neighbor_msg_buf, STATUS, curID, curID, 0, vector_clock, msg_log[0], totalPeers);
                        printf("********** (RUMOR out loop) Sending STATUS Message from UDP port: %d to port: %d\n ", udp_port, ntohs(neighbor_addr.sin_port));
                        if (sendto(udp_socket, (const char *)neighbor_msg_buf, sizeof(received_message), MSG_DONTWAIT, (const struct sockaddr *)&neighbor_addr, sizeof(neighbor_addr)) != sizeof(struct message))
                        {
                            perror("UDP| send fail");
                        }
                    }
                    if (udp_msg_type == STATUS)
                    {
                        printf("Server %d received STATUS Message From server: %d\n", curID, udp_peer_msg_buf->server_id);
                        // stat = assessSync(next_msg, udp_peer_msg_buf, vector_clock, totalPeers);

                        if ((stat = assessSync(next_msg, udp_peer_msg_buf, vector_clock, totalPeers)) == 1)
                        {

                            j = get_neighbors_index(udp_peer_msg_buf->server_id, curID, num_neighbors);
                            neighbor_addr.sin_family = AF_INET;
                            neighbor_addr.sin_addr.s_addr = INADDR_ANY;
                            neighbor_addr.sin_port = htons(neighbor_ports[j]);

                            msg_idx = search_msg(msg_ids, num_msgs, next_msg[0], next_msg[1]);
                            if (msg_idx == -1)
                            {
                                printf("Failed Search...");
                                break;
                            }
                            prepareMessage(neighbor_msg_buf, RUMOR, curID, next_msg[0], next_msg[1], vector_clock, msg_log[msg_idx], totalPeers);
                            printf("********** (stat == 1) Server %d Sending RUMOR Message from UDP port: %d to port: %d\n ", curID, udp_port, ntohs(neighbor_addr.sin_port));
                            // print_message(neighbor_msg_buf, totalPeers);

                            // send
                            if (sendto(udp_socket, (const char *)neighbor_msg_buf, (socklen_t)sizeof(received_message), MSG_DONTWAIT,
                                       (const struct sockaddr *)&neighbor_addr, sizeof(neighbor_addr)) != sizeof(struct message))
                            {
                                perror("Send Failed.");
                            }
                        }
                        else if (stat == -1)
                        {
                            // need messages, send a status message
                            j = get_neighbors_index(udp_peer_msg_buf->server_id, curID, num_neighbors);
                            neighbor_addr.sin_family = AF_INET;
                            neighbor_addr.sin_addr.s_addr = INADDR_ANY;
                            neighbor_addr.sin_port = htons(neighbor_ports[j]);

                            prepareMessage(neighbor_msg_buf, STATUS, curID, curID, 0, vector_clock, msg_log[0], totalPeers);
                            printf("********** (stat == -1)Server %d Sending STATUS Message from UDP port: %d to port: %d\n ", curID, udp_port, ntohs(neighbor_addr.sin_port));

                            // send
                            if (sendto(udp_socket, (const char *)neighbor_msg_buf, (socklen_t)sizeof(received_message), MSG_DONTWAIT,
                                       (const struct sockaddr *)&neighbor_addr, sizeof(neighbor_addr)) != sizeof(struct message))
                            {
                                perror("Send Failed.");
                            }
                        }
                        else
                        {
                            if (num_neighbors == 1)
                            {
                                printf("Nothing to do, no more neighbors...\n");
                            }
                            else if (rand() % 2 == HEADS)
                            {
                                j = get_neighbors_index(udp_peer_msg_buf->server_id, curID, num_neighbors);
                                j = 1 - j;

                                neighbor_addr.sin_family = AF_INET;
                                neighbor_addr.sin_addr.s_addr = INADDR_ANY;
                                neighbor_addr.sin_port = htons(neighbor_ports[j]);

                                prepareMessage(neighbor_msg_buf, STATUS, curID, curID, 0, vector_clock, msg_log[0], totalPeers);
                                printf("*********** (rand()2 == HEADS) Sending STATUS Message from UDP port: %d to port: %d\n ", udp_port, ntohs(neighbor_addr.sin_port));

                                // send
                                if (sendto(udp_socket, (const char *)neighbor_msg_buf, (socklen_t)sizeof(received_message), MSG_DONTWAIT,
                                           (const struct sockaddr *)&neighbor_addr, sizeof(neighbor_addr)) != sizeof(struct message))
                                {
                                    perror("Send Failed.");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    free(cmd_buf);
    free(neighbor_msg_buf);
    free(udp_peer_msg_buf);
    for (i = 0; i < num_msgs; i++)
    {
        free(msg_log[i]);
        free(msg_ids[i]);
    }
    free(msg_log);
    free(msg_ids);
}

void timeout_hdler(int signum)
{
    anti_entropy = TRUE;
    signal(SIGALRM, timeout_hdler);
}