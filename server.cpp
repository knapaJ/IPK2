#include <iostream>
#include <netinet/in.h>
#include <cstdio>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>

#include "debug-headers.h"
#include "serve_client.h"

#define CLIENT_QUEUE_LEN 3
#define STDIN_FD 0
#define MAX(a,b) a > b ? a : b


int main() {
    // This site has been used as a reference: https://www.ibm.com/docs/en/i/7.2?topic=sscaaiic-example-accepting-connections-from-both-ipv6-ipv4-clients
    // But this code is in no shape or form a copy, more like a loose interpretation.

    std::cerr << "Hello, World!\nStarting server" << std::endl;

    struct sockaddr_in6 listening_address{};
    memset(&listening_address, 0, sizeof(listening_address));
    listening_address.sin6_addr = in6addr_any;
    listening_address.sin6_family = AF_INET6;
    listening_address.sin6_port = htons(SERVER_PORT);
    listening_address.sin6_scope_id = 0;
    listening_address.sin6_scope_id = if_nametoindex("lo");

    char int_name_buffer[IF_NAMESIZE];
    if(!if_indextoname(listening_address.sin6_scope_id, int_name_buffer)){
        perror("Interface unknown");
        strcpy(int_name_buffer, "?");
    }
    std::cerr << "Setting socket to bind to interface " << int_name_buffer << std::endl;

    int server_socket = -1;

    if ((server_socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) < 0){
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }


    int opt_val = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT | SO_REUSEADDR, &opt_val, sizeof(opt_val)) < 0){
        perror("Error setting SO_REUSEADDR");
        exit(EXIT_FAILURE);
    }

    // Enable dual stack socket
    opt_val = 0;
    if (setsockopt(server_socket, IPPROTO_IPV6, IPV6_V6ONLY, &opt_val, sizeof(opt_val)) < 0){
        perror("Error creating dual stack socket");
        exit(EXIT_FAILURE);
    }


    if (bind(server_socket, (struct sockaddr*)&listening_address, sizeof (listening_address)) < 0){
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }


    if(listen(server_socket, CLIENT_QUEUE_LEN) < 0){
        perror("Error listening on socket");
        exit(EXIT_FAILURE);
    }


    while(true){
        std::cerr << "Waiting for connection" << std::endl;
        int client_socket = -1;
        struct sockaddr_in6 client_address{};
        int client_address_len = sizeof (client_address);
        //Accept connection
        if ((client_socket = accept(server_socket, (sockaddr *) &client_address, (socklen_t *)&client_address_len)) < 0){
            perror("Failed accepting connection");
            break;
        }
        //Now we have a socket connection

        char client_ip_str[INET6_ADDRSTRLEN];
        inet_ntop(client_address.sin6_family, &client_address.sin6_addr, client_ip_str, INET6_ADDRSTRLEN);

        std::cerr << "New connection from " << client_ip_str << std::endl;

        client_info_t clientInfo;
        clientInfo.socket_fd = client_socket;
        clientInfo.ip_str = client_ip_str;
        clientInfo.port = ntohs(client_address.sin6_port);

        serve_client(&clientInfo);


        close(client_socket);

        std::cerr << "Connection to " << client_ip_str << " closed" << std::endl;

    }


    close(server_socket);
    return 0;


}