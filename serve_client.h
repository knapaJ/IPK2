//
// Created by student on 20.4.22.
//

#ifndef IPK2_SERVE_CLIENT_H
#define IPK2_SERVE_CLIENT_H

#include <string>
#include <map>

//Structure with all the client info needed by the serving function
typedef struct{
    char * ip_str;
    int port;
    int socket_fd;
} client_info_t;

//Client serving function
int serve_client(const client_info_t *clientInfo, const std::map<std::string, std::string>& user_db);


#endif //IPK2_SERVE_CLIENT_H
