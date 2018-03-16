//
//  main.c
//  ex_4
//
//  Created by Eliyah Weinberg on 7.3.2018.
//  Copyright © 2018 Eliyah Weinberg. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include "slist.h"

#define USAGE "Usage: server <port>\n"
#define NAME "guest"
#define MSG_MAX_SIZE 4096
#define SUCCESS 0
#define FAILURE -1
#define TRUE 1
#define FALSE 0


//#define P_DEBUG

typedef int bool_t;

typedef struct s_attributes {
    slist_t *write_msg;
    slist_t *clients;
    bool_t drop_clients;
    char* peek_buffer;
    int main_fd;
    int max_fd;
    fd_set *read_fds;
    fd_set *write_fds;
}server_attr;

typedef struct _client_data{
    int client_fd;
    char* id;
}client_data;

/*-------------------------GLOBAL VARIABLES-----------------------------------*/
server_attr *alloc_memory;


/*---------------------FUNCTION DECLARATION-----------------------------------*/
void sigint_handler(int signum);

void free_memory(server_attr *dealloc_m);

void db_print(char* msg);

int init_server(int port);

void chat_routine(server_attr *this);

int connect_client(server_attr *this, int new_fd);

server_attr* init_attribs(int sock_fd);

int init_sets(server_attr* this);

int read_from_clients(server_attr* this);

int write_to_clients(server_attr* this);

int receive_message(client_data* client, server_attr* server_at);

int send_message(char* message, int client_fd);

int delete_clients(server_attr* server_at);

/*------------------------------M A I N---------------------------------------*/
int main(int argc, const char * argv[]) {
    if (argc < 2){
            printf(USAGE);
            exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    int sock_fd;
    
    if (port < 0){
        printf(USAGE);
        exit(EXIT_FAILURE);
    }
    
    sock_fd = init_server(port);
    if (sock_fd == FAILURE)
        exit(EXIT_FAILURE);
    
    server_attr* attribs = init_attribs(sock_fd);
    if (!attribs)
        exit(EXIT_FAILURE);
    
    alloc_memory = attribs; //creating global pointer for sigint termination
    
    signal(SIGINT, sigint_handler);
    
    chat_routine(attribs);
    
    return EXIT_FAILURE;
}




/*----------------------FUNCTIONS IMPLEMENTATION------------------------------*/

int init_server(int port){
    int sock_fd;
    struct sockaddr_in serv_adr;
    
    sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0){
        perror("Error on socket openning");
        return FAILURE;
    }
    
    
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = INADDR_ANY;
    serv_adr.sin_port = htons(port);
    
    if ( bind(sock_fd, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) < 0){
        perror("Error on binding");
        return FAILURE;
    }
    
    if (listen(sock_fd, 5) == -1){
        perror("Error on listen");
        return FAILURE;
    }
    db_print("server initilized successfuly");
    return sock_fd;
}

//----------------------------------------------------------------------------//
void sigint_handler(int signum){
    printf("\nServer shutdown\n");
    free_memory(alloc_memory);
    exit(EXIT_SUCCESS);
}

//----------------------------------------------------------------------------//
void db_print(char* msg){
#ifdef P_DEBUG
    printf("%s\n",msg);
#endif
}

//----------------------------------------------------------------------------//
server_attr* init_attribs(int sock_fd){
    server_attr* new = (server_attr*)malloc(sizeof(server_attr));
    if (!new)
        return NULL;
    
    new->main_fd = sock_fd;
    new->write_fds = NULL;
    new->read_fds = NULL;
    new->max_fd = sock_fd;
    new->drop_clients = FALSE;
    new->clients = (slist_t*)malloc(sizeof(slist_t));
    if (!new->clients){
        free_memory(new);
        return NULL;
    }
    new->write_msg = (slist_t*)malloc(sizeof(slist_t));
    if (!new->write_msg) {
        free(new->clients);
        free(new);
        return NULL;
    }
    new->peek_buffer = (char*)malloc(sizeof(char)*MSG_MAX_SIZE);
    if (!new->peek_buffer){
        free(new->write_msg);
        free(new->clients);
        free(new);
    }
    
    slist_init(new->clients);
    slist_init(new->write_msg);
    return new;
}

//----------------------------------------------------------------------------//
void free_memory(server_attr *dealloc_m){
    client_data* next_client=(client_data*)slist_pop_first(dealloc_m->clients);
    while (next_client != NULL) {
        free(next_client->id);
        free(next_client);
        next_client = (client_data*)slist_pop_first(dealloc_m->clients);
    }
    slist_destroy(dealloc_m->clients, SLIST_LEAVE_DATA);
    slist_destroy(dealloc_m->write_msg, SLIST_FREE_DATA);
    free(dealloc_m->write_msg);
    free(dealloc_m->clients);
    free(dealloc_m->peek_buffer);
    free(dealloc_m);
    db_print("all memory deallocated successesfuly");
    
}

//----------------------------------------------------------------------------//
void chat_routine(server_attr *this){
    int new_fd;
    fd_set read_fds;
    fd_set write_fds;
    struct sockaddr cli_addr;
    socklen_t clilen = 0;
    this->read_fds = &read_fds;
    this->write_fds = &write_fds;
    while (1) {
        init_sets(this);
        if (select((this->max_fd)+1,
                   this->read_fds, 0, 0, 0)<0){
            if (errno != EINTR){
                perror("select error");
                free_memory(this);
                return;
            }
        }
        /*checking if there is new connection requests*/
        if(FD_ISSET(this->main_fd,this->read_fds)){
            new_fd = accept(this->main_fd,
                            (struct sockaddr*)&cli_addr, &clilen);
            if (new_fd == -1){
                perror("fail of accept");
            }
            else
                connect_client(this, new_fd);
        }
        
        
        
        read_from_clients(this);
        
        write_to_clients(this);
        
        if (this->drop_clients)
            delete_clients(this);
    }
}

//----------------------------------------------------------------------------//
int connect_client(server_attr *this, int new_fd){
    int base = 10;
    int i, temp = new_fd;
    client_data* new_client = (client_data*)malloc(sizeof(client_data));
    if (!new_client)
        return FAILURE;
    
    new_client->client_fd = new_fd;
    if (new_fd == 0)
        i = 1;
    else
        for (i=0; temp>0; i++)
            temp = temp/base;
    
    new_client->id = (char*)malloc(sizeof(char)*(i+strlen(NAME)+3));
    if (!new_client->id) {
        free(new_client);
        return FAILURE;
    }
    snprintf(new_client->id, (i+strlen(NAME)+3), "%s%d: ",NAME,new_fd);

    
    db_print("new client connected:");
    db_print(new_client->id);
    
    this->max_fd = this->max_fd > new_fd ? this->max_fd : new_fd;
    slist_append(this->clients, new_client);
    
    return SUCCESS;
}

//----------------------------------------------------------------------------//
int init_sets(server_attr* this){
    slist_node_t* node = slist_head(this->clients);
    client_data* client = NULL;
    db_print("at sets initialization");
    FD_ZERO(this->read_fds);
    FD_ZERO(this->write_fds);
    FD_SET(this->main_fd, this->read_fds);
    while (node) {
        client = (client_data*)slist_data(node);
        FD_SET(client->client_fd, this->read_fds);
        FD_SET(client->client_fd, this->write_fds);
        node = slist_next(node);
    }
    
    return SUCCESS;
}

//----------------------------------------------------------------------------//
int read_from_clients(server_attr* this){
    db_print("reading from clients");
    slist_node_t* node = slist_head(this->clients);
    client_data* client = NULL;
    int receive_error = FALSE;
    
    while (node) {
        client = (client_data*)slist_data(node);
        /*Cheking if there is messages*/
        if (client->client_fd != -1){
            if (FD_ISSET(client->client_fd, this->read_fds)){
                printf("server is ready to read ");
                printf("from socket %d\n", client->client_fd);
                receive_error = receive_message(client, this);
                if (receive_error == FAILURE){
                    perror("error on receive");
                    free_memory(this);
                    exit(EXIT_FAILURE);
                }
            }
        }
        node = slist_next(node);
    }
    
    return SUCCESS;
}

//----------------------------------------------------------------------------//
int write_to_clients(server_attr* this){
    slist_node_t* c_node;
    client_data* client;
    bool_t sent = FALSE;
    int flag;
    char* message = (char*)slist_pop_first(this->write_msg);
    while (message) {
        c_node = slist_head(this->clients);
        while (c_node) {
            client = (client_data*)slist_data(c_node);
            if (client->client_fd != -1){
                if (FD_ISSET(client->client_fd, this->write_fds)){
                    printf("Server is ready to write to socket %d\n"
                                                            ,client->client_fd);
                    flag = send_message(message, client->client_fd);
                    if (flag == SUCCESS)
                        sent = TRUE;
                }
            }
            c_node = slist_next(c_node);
        }
        if (!sent){//if wasn't able to write to any client
            slist_prepend(this->write_msg, message);
            return SUCCESS;
        }
        free(message);
        message = (char*)slist_pop_first(this->write_msg);
    }
    return SUCCESS;
}

//----------------------------------------------------------------------------//
int receive_message(client_data* client, server_attr* server_at){
    ssize_t to_read = 0;
    ssize_t peek_recvd, rc = 0;
    char* ptr, *message, *write_ptr;
    const char* EOL = "\r\n";
    memset(server_at->peek_buffer, '\0', MSG_MAX_SIZE);
    
    peek_recvd = recv(client->client_fd,
                      server_at->peek_buffer,
                      MSG_MAX_SIZE,
                      MSG_DONTWAIT | MSG_PEEK);
    if (peek_recvd < 1){
        if (errno == EAGAIN || errno == EWOULDBLOCK){
            return SUCCESS;
        }
        else if (errno == ECONNRESET || peek_recvd == 0){
            client->client_fd = -1;
            server_at->drop_clients = TRUE;
            db_print("connection forced to close");
            return SUCCESS;
            
        }
        fprintf(stderr, "receive from clint %d ",client->client_fd);
        perror("error");
        return FAILURE;
    }
    
    //checking if end line token received
    ptr = strstr(server_at->peek_buffer, EOL);
    if (!ptr){
        read(client->client_fd, server_at->peek_buffer, peek_recvd);
        return SUCCESS;
        
    }
   
    ptr += 2;
    to_read = ptr - server_at->peek_buffer; //number of bytes to read
    message = (char*)malloc(sizeof(char)*(to_read+strlen(client->id)+1));
    if (!message)
        return FAILURE;
    strcpy(message, client->id);
    write_ptr = message+strlen(client->id);
    while (to_read > 0) {
        rc = read(client->client_fd,
                  write_ptr,
                  to_read);
        if(rc<0){
            free(message);
            return SUCCESS;
        }
        
        write_ptr += rc;
        to_read -= rc;
    }
    *write_ptr = '\0';
    db_print("receiving message ended successfuly");
    db_print(message);
    
    slist_append(server_at->write_msg, message);
    return SUCCESS;
}

//----------------------------------------------------------------------------//
int send_message(char* message, int client_fd){
    ssize_t to_write = strlen(message);
    ssize_t wc;
    
    while (to_write) {
        wc = send(client_fd, message, to_write, MSG_DONTWAIT);
        if (wc < 0){
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return SUCCESS;
            perror("error while sending message");
            return FAILURE;
        }
        to_write -= wc;
        message += wc;
    }
    return SUCCESS;
}

//----------------------------------------------------------------------------//
int delete_clients( server_attr* server_at){
    server_at->max_fd = server_at->main_fd;
    client_data* current = (client_data*)slist_pop_first(server_at->clients);
    slist_t* new_list = (slist_t*)malloc(sizeof(slist_t));
    if (!new_list){
        free_memory(server_at);
        return FAILURE;
    }
    slist_init(new_list);
    while (current) {
        if (current->client_fd == -1){
            free(current->id);
            free(current);
        }
        else {
            server_at->max_fd = server_at->max_fd > current->client_fd ?
            server_at->max_fd : current->client_fd;
            slist_append(new_list, current);
        }
        current = (client_data*)slist_pop_first(server_at->clients);
    }
    slist_destroy(server_at->clients, SLIST_LEAVE_DATA);
    free(server_at->clients);
    server_at->clients = new_list;
    return SUCCESS;
}

