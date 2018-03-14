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
#define SUCCESS 0
#define FAILURE -1
#define TRUE 1
#define FALSE 0

#define P_DEBUG

typedef int bool_t;

typedef struct s_attributes {
    slist_t *write_msg;
    slist_t *clients;
    int sock_fd;
    int max_fd;
    fd_set *read_fds;
    fd_set *write_fds;
}server_attr;

typedef struct _client_data{
    int client_fd;
    char* id;
    slist_t *msg;
}client_data;

typedef struct _s_message{
    char* str_message;
    int lenght;
}chat_message;
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

bool_t read_from_clients(server_attr* this);

int write_to_clients(server_attr* this);

int push_mesasge(client_data* client, server_attr* this, chat_message* message);

void destroy_cmessage_list(slist_t* list);

chat_message* receive_message(client_data* client, server_attr* this);

int send_message(chat_message* message, int client_fd);


/*------------------------------M A I N---------------------------------------*/
int main(int argc, const char * argv[]) {
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
    printf("Server shutdown\n");
    free_memory(alloc_memory);
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
    
    new->sock_fd = sock_fd;
    new->write_fds = NULL;
    new->read_fds = NULL;
    new->max_fd = sock_fd;
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
    
    slist_init(new->clients);
    slist_init(new->write_msg);
    return new;
}

//----------------------------------------------------------------------------//
void free_memory(server_attr *dealloc_m){
   
    destroy_cmessage_list(dealloc_m->write_msg);
    
    client_data* next_client=(client_data*)slist_pop_first(dealloc_m->clients);
    while (next_client != NULL) {
        destroy_cmessage_list(next_client->msg);
        free(next_client->id);
        free(next_client);
        next_client = (client_data*)slist_pop_first(dealloc_m->clients);
    }
    slist_destroy(dealloc_m->clients, SLIST_LEAVE_DATA);
    free(dealloc_m->clients);
    free(dealloc_m);
    db_print("all memory deallocated successesfuly");
    
}

//----------------------------------------------------------------------------//
void chat_routine(server_attr *this){
    bool_t data_to_write = FALSE;
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
                   this->read_fds,
                   this->write_fds, 0, 0)<0){
            perror("select error");
            free_memory(this);
            return;
        }
        /*checking if there is new connection requests*/
        if(FD_ISSET(this->sock_fd,this->read_fds)){
            new_fd = accept/*4*/(this->sock_fd,
                            (struct sockaddr*)&cli_addr, &clilen/*, O_NONBLOCK*/);
            connect_client(this, new_fd);
        }
        data_to_write = read_from_clients(this);
        
        if (data_to_write)
            write_to_clients(this);
        
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
    
    new_client->id = (char*)malloc(sizeof(char)*(i+strlen(NAME)+1));
    if (!new_client->id) {
        free(new_client);
        return FAILURE;
    }
    snprintf(new_client->id, (i+strlen(NAME)+1), "%s%d",NAME,new_fd);
    
    new_client->msg = (slist_t*)malloc(sizeof(slist_t));
    if (!new_client->msg){
        free(new_client->id);
        free(new_client);
        return FAILURE;
    }
    slist_init(new_client->msg);
    
    db_print("new client connected:");
    db_print(new_client->id);
    
    this->max_fd = this->max_fd > new_fd ? this->max_fd : new_fd;
    slist_append(this->clients, new_client);
    
    return SUCCESS;
}

int init_sets(server_attr* this){
    slist_node_t* node = slist_head(this->clients);
    client_data* client = NULL;
    db_print("at sets initialization");
    FD_ZERO(this->read_fds);
    FD_ZERO(this->write_fds);
    FD_SET(this->sock_fd, this->read_fds);
    
    while (node) {
        client = (client_data*)slist_data(node);
        FD_SET(client->client_fd, this->read_fds);
        
        node = slist_next(node);
    }
    
    return SUCCESS;
}

bool_t read_from_clients(server_attr* this){
    db_print("reading from clients");
    slist_node_t* node = slist_head(this->clients);
    client_data* client = NULL;
    bool_t exist_message = FALSE;;
    
    while (node) {
        client = (client_data*)slist_data(node);
        /*Cheking if there is messages in the client queue*/
        if (slist_head(client->msg)){
            exist_message = TRUE;
            push_mesasge(client, this, NULL);
        }
            
        if (FD_ISSET(client->client_fd, this->read_fds)){
            printf("server is ready to read");
            printf("from socket %d\n", client->client_fd);
            exist_message = TRUE;
            chat_message* message = receive_message(client, this);
            if (!message){
                perror("error on receive");
                free_memory(this);
                exit(EXIT_FAILURE);
            }
            push_mesasge(client, this, message);
        }
        
        node = slist_next(node);
    }
    
    return exist_message;
}

int write_to_clients(server_attr* this){
    slist_node_t* c_node;
    client_data* client;
    chat_message* message = (chat_message*)slist_pop_first(this->write_msg);
    while (message) {
        c_node = slist_head(this->clients);
        while (c_node) {
            client = (client_data*)slist_data(c_node);
            if (FD_ISSET(client->client_fd, this->write_fds)){
                printf("Server is ready to write to socket %d\n"
                                                        ,client->client_fd);
                send_message(message, client->client_fd);
            }
        }
        free(message->str_message);
        free(message);
        message = (chat_message*)slist_pop_first(this->write_msg);
    }
    return SUCCESS;
}

int push_mesasge(client_data* client, server_attr* this, chat_message* message){
    if (!message){
       
    }
}

void destroy_cmessage_list(slist_t* list){
    chat_message* message=(chat_message*)slist_pop_first(list);
    while (message) {
        free(message->str_message);
        free(message);
    }
    slist_destroy(list, SLIST_LEAVE_DATA);
    free(list);
}
