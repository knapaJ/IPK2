//
// Created by student on 20.4.22.
//

#include "debug-headers.h"
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <string>
#include <cstdio>
#include <netinet/in.h>
#include <iostream>
#include <cstdlib>
#include "serve_client.h"

#define BUFFER_LEN 1024
#define HNAME_BUFF_LEN 101




int serve_client(const client_info_t *clientInfo){

    // Ok so client is connected, we gotta greet him
    char hostname[HNAME_BUFF_LEN];
    memset(hostname, 0, HNAME_BUFF_LEN);
    gethostname(hostname, HNAME_BUFF_LEN - 1);
    std::string greeting = "+";
    greeting += hostname;
    greeting += " SimpleFTP server (RFC913) - Welcome! (author: xknapo05)";
    // Now send the greeting
    send(clientInfo->socket_fd, greeting.c_str(), strlen(greeting.c_str())+1, 0);

    while(true){
        //Basically just read forever the commands coming from the client socket
        char recv_buff[BUFFER_LEN];
        memset(recv_buff, 0, BUFFER_LEN);

        long int recv_code = -1;

        if((recv_code = read(clientInfo->socket_fd, recv_buff, BUFFER_LEN -1)) == 0){
            std::cerr << "Connection closed by client" << std::endl;
            break;
        }
        if(recv_code == -1){
            perror("Error reading socket");
            break;
        }



    }

    return 0;
}