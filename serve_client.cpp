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
#include <sys/types.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <iostream>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <dirent.h>

#include "serve_client.h"


#define BUFFER_LEN 1024
#define HNAME_BUFF_LEN 101

typedef enum {
    LOGIN,
    READY,
    DONE,
    NAME,
    RETR
}server_state_t;


bool verify_login(const std::string& username, const std::string& password, std::map<std::string, std::string> user_db){
    for (auto itr = user_db.find(username); itr != user_db.end(); itr++) {
        if (itr->second == password){
            return true;
        }
    }
    return false;
}



int serve_client(const client_info_t *clientInfo, const std::map<std::string, std::string>& user_db){

    //Some info about current session
    server_state_t state = LOGIN;
    std::string username{};
    std::string password{};
    std::string old_name{};
    std::string retr_name{};

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

        // So we have reading the socket handled!
        std::string client_command = recv_buff;
        std::string message{}; // message to be sent to the client

        switch (state) {
            case LOGIN:
                if (client_command.substr(0, 4) == "USER" || client_command.substr(0, 4) == "ACCT") {
                    client_command.erase(0, 5);
                    username = client_command;
                    message = "+Username received";
                }
                else if (client_command.substr(0, 4) == "PASS") {
                    client_command.erase(0, 5);
                    password = client_command;
                    message = "+Password received";
                }
                else if (client_command.substr(0, 4) == "DONE") {
                    message = "+Bye";
                    state = DONE;
                } else {
                    message = "-Command not recognized";
                }
                if (!password.empty() && !username.empty()) {
                    if (verify_login(username, password, user_db)) {
                        message = "!Logged in as ";
                        message += username;
                        state = READY;
                    } else {
                        message = "-Incorrect username or password";
                    }
                }
                send(clientInfo->socket_fd, message.c_str(), strlen(message.c_str()) + 1, 0);
                break;

            case READY:
                if (client_command.substr(0, 4) == "DONE") {
                    message = "+Bye";
                    state = DONE;
                }
                else if(client_command.substr(0, 4) == "LIST"){
                    client_command.erase(0,5);
                    bool verbose = false;
                    if(!client_command.empty())
                        verbose = client_command.substr(0,1) == "V";
                    client_command.erase(0,2);
                    std::string directory;
                    if(!client_command.empty())
                        directory = client_command;
                    else directory = "./";

                    struct dirent **dir_list;
                    int n;
                    if((n = scandir( directory.c_str(), &dir_list, NULL, alphasort)) == -1){
                        message = "-Error listing directory ";
                        message+= directory;
                        message+= ": ";
                        message+= strerror(errno);
                    }else{
                        if(verbose) {
                            message = "type| Name\n";
                            message+= "----+--------------------------\n";
                            for(int i = 0; i<n; i++){
                                if(dir_list[i]->d_type != DT_DIR) continue;
                                    switch (dir_list[i]->d_type) {
                                        case DT_REG:
                                            message += "file|";
                                            break;
                                        case DT_DIR:
                                            message += "dir |";
                                            break;
                                        case DT_LNK:
                                            message += "sym |";
                                            break;
                                        default:
                                            message += "    |";
                                    }
                                message+= dir_list[i]->d_name;
                                message+= "\n";
                            }
                            for(int i = 0; i<n; i++){
                                if(dir_list[i]->d_type == DT_DIR) continue;
                                switch (dir_list[i]->d_type) {
                                    case DT_REG:
                                        message += "file|";
                                        break;
                                    case DT_DIR:
                                        message += "dir |";
                                        break;
                                    case DT_LNK:
                                        message += "sym |";
                                        break;
                                    default:
                                        message += "    |";
                                }
                                message+= dir_list[i]->d_name;
                                message+= "\n";
                            }

                        }else{
                            for(int i = 0; i<n; i++){
                                if(dir_list[i]->d_type != DT_REG) continue;
                                message+= dir_list[i]->d_name;
                                message+= "\n";
                            }
                        }
                        while (n--){
                            free(dir_list[n]);
                        }
                        free(dir_list);
                    }
                }
                else if(client_command.substr(0, 4) == "CDIR"){
                    client_command.erase(0,5);
                    std::string directory;
                    if (!client_command.empty())
                        directory = client_command;
                    else
                        directory = "./";
                    if(chdir(directory.c_str()) ==0 ){
                        message = "+Changed working dir to ";
                        message += directory;
                    } else {
                        message = "-Can't connect to directory: ";
                        message += strerror(errno);
                    }
                }
                else if(client_command.substr(0, 4) == "KILL"){
                    client_command.erase(0,5);
                    if(client_command.empty()){
                        message = "-Incomplete command";
                    }else {
                        std::string file = client_command;
                        if (remove(file.c_str()) == 0) {
                            message = "+";
                            message += file;
                            message += " deleted";
                        }else{
                            message = "-Not deleted: ";
                            message += strerror(errno);
                        }
                    }
                }
                else if(client_command.substr(0, 4) == "NAME"){
                    client_command.erase(0,5);
                    if(client_command.empty()){
                        message = "-Incomplete command";
                    }else{
                        old_name = client_command;
                        if(access(old_name.c_str(), F_OK) == 0){
                            message="+File exists, send new name";
                            state = NAME;
                        }else{
                            message="-File not found: ";
                            message+= strerror(errno);
                        }
                    }
                }
                else if(client_command.substr(0, 4) == "RETR"){
                    client_command.erase(0,5);
                    if(client_command.empty()){
                        message = "-Incomplete command";
                    }else{
                        retr_name = client_command;
                        if(access(retr_name.c_str(), R_OK) == 0){
                            struct stat file_stats{};
                            if(stat(retr_name.c_str(),&file_stats) == 0){
                                if((file_stats.st_mode & S_IFMT) == S_IFREG) {
                                    message = std::to_string(file_stats.st_size);
                                    state = RETR;
                                }else{
                                    message = "-Not a file";
                                }
                            } else {
                                message = "-Cannot transfer file, error getting it's size: ";
                                message += strerror(errno);
                            }
                        } else{
                            message = "-Can't send file: ";
                            message+= strerror(errno);
                        }
                    }
                }
                else if(client_command.substr(0, 4) == "STOR"){
                    client_command.erase(0,5);
                    if(!client_command.empty()){
                        //Get write mode
                        std::string mode = client_command.substr(0,3);
                        client_command.erase(0,4);
                        if (!client_command.empty()){
                            std::string stor_name = client_command;
                            bool ok = false;
                            if(mode == "NEW"){
                                if(access(stor_name.c_str(), F_OK) == 0){
                                    message = "-File already exists";
                                }else{
                                    mode = "w";
                                    message = "+OK, will create new file";
                                    ok = true;
                                }
                            }else if(mode == "OLD"){
                                mode = "w";
                                message = "+Ok, will overwrite old file";
                                ok = true;
                            }else if(mode == "APP"){
                                mode = "a";
                                message = "+Ok, will append to old file";
                                ok = true;
                            }else{
                                message = "-Bad parameter";
                            }

                            if(ok){
                                //Sent message OK...
                                send(clientInfo->socket_fd, message.c_str(), strlen(message.c_str())+1, 0);
                                //And now wait for size...
                                memset(recv_buff, 0, BUFFER_LEN);
                                if(read(clientInfo->socket_fd, recv_buff, BUFFER_LEN-1) <=0 ){
                                    perror("Client closed connection unexpectedly");
                                    goto exit_serve_client;
                                }
                                size_t upload_size = strtoul(recv_buff, nullptr, 0);
                                // Let's check if we have enough free space...
                                struct statvfs fsstat{};
                                if(statvfs("./", &fsstat) == 0){
                                    if(fsstat.f_bsize*fsstat.f_bavail > upload_size){
                                        //We have enough space, HOORAY!
                                        send(clientInfo->socket_fd, "+OK, waiting for file", strlen("+OK, waiting for file")+1, 0);

                                        FILE *local_file = fopen(stor_name.c_str(), mode.c_str());
                                        size_t remaining = upload_size;
                                        while (remaining>0){
                                            size_t actually_allocated = MIN(1000000,remaining);
                                            char *buffer = (char *) calloc(actually_allocated, sizeof(char));
                                            if(!buffer){
                                                perror("Allocation error");
                                                exit(EXIT_FAILURE);
                                            }
                                            size_t recvd = read(clientInfo->socket_fd, buffer, actually_allocated);
                                            fwrite(buffer, sizeof(char), recvd, local_file);
                                            remaining-=recvd;
                                            free(buffer);
                                        }
                                        fclose(local_file);
                                        //ok, we got the file
                                        message = "+Ok, saved";
                                    }else{
                                        message = "-Not enough disk space";
                                    }
                                }else{
                                    message = "-Cannot verify disk space: ";
                                    message+= strerror(errno);
                                }

                            }

                        }else{
                            message = "-Incomplete command";
                        }
                    }else{
                        message = "-Incomplete command";
                    }
                }
                else if(client_command.substr(0, 4) == "MDIR"){
                    client_command.erase(0,5);
                    if(!client_command.empty()){
                        std::string dirname = client_command;
                        if(mkdir(dirname.c_str(), 0777) ==0){
                            message = "Created directory ";
                            message+= dirname;
                        }else{
                            message = "-Can not create directory ";
                            message+= dirname;
                            message+= ": ";
                            message+= strerror(errno);
                        }
                    }else{
                        message = "-Incomplete command";
                    }
                }
                else{
                    message = "-Command not recognised";
                }
                send(clientInfo->socket_fd, message.c_str(), strlen(message.c_str()) +1, 0);
                break;
            case NAME:
                if(client_command.substr(0, 4) == "TOBE"){
                    client_command.erase(0,5);
                    if(client_command.empty()){
                        message = "-Incomplete command, rename aborted";
                    }else{
                        std::string new_name = client_command;
                        if(rename(old_name.c_str(),new_name.c_str()) == 0){
                            message = "+Renamed ";
                            message+= old_name;
                            message+= " to ";
                            message+= new_name;
                        }else{
                            message = "-Could not rename file: ";
                            message+= strerror(errno);
                        }
                    }
                    state=READY;
                }else{
                    message="-Rename sequence aborted";
                    state = READY;
                }
                send(clientInfo->socket_fd, message.c_str(), strlen(message.c_str()) +1, 0);
                break;
            case RETR:
                if(client_command.substr(0, 4) == "SEND"){
                    struct stat file_stats{};
                    stat(retr_name.c_str(), &file_stats);

                    size_t org_size = file_stats.st_size;
                    size_t size_to_be_sent = org_size;
                    FILE *in_file = fopen(retr_name.c_str(), "r");
                    while(size_to_be_sent>0){
                        size_t actually_allocd = MIN(1000000,size_to_be_sent);
                        char * send_buffer = (char *) calloc(actually_allocd,sizeof (char ));

                        fread(send_buffer,sizeof (char), actually_allocd, in_file);

                        size_t rest = actually_allocd;
                        char *send_pos = send_buffer;
                        while(rest>0){
                            size_t sent = send(clientInfo->socket_fd, send_buffer, rest, 0);
                            rest -= sent;
                            send_pos += sent;
                            size_to_be_sent-=sent;
                        }

                        free(send_buffer);
                    }
                    fclose(in_file);


                    state = READY;
                    break;
                }else{
                    message="-Retrieve sequence aborted";
                    state = READY;
                }
                send(clientInfo->socket_fd, message.c_str(), strlen(message.c_str()) +1, 0);
                break;
            default:
                message = "Server Exception";
                send(clientInfo->socket_fd, message.c_str(), strlen(message.c_str()) +1, 0);
                std::cerr << "Server exception" << std::endl;
                exit(EXIT_FAILURE);
        }
        if( state == DONE){
            exit_serve_client:
            break;
        }


    }

    return 0;
}