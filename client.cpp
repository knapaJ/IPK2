//
// Created by student on 20.4.22.
//

#include <iostream>
#include <netinet/in.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <getopt.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

#include "debug-headers.h"

void print_help(){
    std::cout <<
        "-h <host>, --host <host>                  Specify the server host address\n"
        "-p <port>, --port <port>                  Specify server port (default 115)\n"
        "-f <path>, --folder <path>                Path to the download and upload folder"
        << std::endl;
    exit(EXIT_SUCCESS);
}

int main(int argc, char ** argv){
    //Getopt struct is ready
    struct option long_opts[] = {
            {"host", required_argument, nullptr, 'h'},
            {"port", required_argument, nullptr, 'p'},
            {"folder", required_argument, nullptr, 'f'},
            {nullptr,0,nullptr,0}
    };
    std::string short_opts = "h:p:f:";

    //Now prepare where to save the options
    char * host_arg = nullptr;
    char * port_arg = "115";
    char * folder_arg = nullptr;


    //start getting opts
    int arg_cur = '\0';
    char *endptr = nullptr; // for the switch to be happy
    while ((arg_cur = getopt_long(argc, argv, short_opts.c_str(), long_opts, nullptr)) != -1){
        switch (arg_cur) {
            case 'h':
                std::cerr << "Parsed argument 'host' as '" << optarg << "'" <<std::endl;
                host_arg = optarg;
                break;

            case 'p':
                std::cerr << "Parser argument 'port' as '" << optarg << "'" <<std::endl;
                port_arg = optarg;
                break;

            case 'f':
                folder_arg = optarg;
                break;
            case '?':
                print_help();
                break;
            default:
                std::cerr << "Unknown error parsing arguments" << std::endl;
                exit(EXIT_FAILURE);
        }
    }

    if(!(host_arg && folder_arg)){
        std::cout << "Bad parameters" <<std::endl;
        print_help();
    }

    std::cerr << "Opening directory '" <<folder_arg<<"'"<<std::endl;
    if(chdir(folder_arg) == -1){
        perror("Error opening specified directory");
        exit(EXIT_FAILURE);
    }

    // Following code was inspired by: https://www.ibm.com/docs/en/i/7.2?topic=clients-example-ipv4-ipv6-client

    //Lets get cranking. First let us prepare some place to store hints we get about the address.
    struct addrinfo address_resolution_hints, *result_address = nullptr;
    memset(&address_resolution_hints, 0, sizeof(address_resolution_hints));
    address_resolution_hints.ai_flags = AI_NUMERICSERV;
    address_resolution_hints.ai_family = AF_UNSPEC;
    address_resolution_hints.ai_socktype = SOCK_STREAM;

    // Now, lets check if we got a valid IP address
    struct in6_addr server_address; //somewhere to store it
    if(inet_pton(AF_INET, host_arg, &server_address) == 1){
        //we got valid IPv4 address
        address_resolution_hints.ai_family = AF_INET;
        address_resolution_hints.ai_flags |= AI_NUMERICHOST;
    }
    else if(inet_pton(AF_INET6, host_arg, &server_address) == 1){
        //we got valid IPv6 address
        address_resolution_hints.ai_family = AF_INET6;
        address_resolution_hints.ai_flags |= AI_NUMERICHOST;
    }
    // Now we get the address in a sensible format
    int rc;
    if ((rc = getaddrinfo(host_arg, port_arg, &address_resolution_hints, &result_address)) != 0){
        std::cout << "Host not found: " << gai_strerror(rc);
        if (rc == EAI_SYSTEM){
            perror("Can not get address info");
        }
        exit(EXIT_FAILURE);
    }

    // inform the programmer :)
    char address_str[INET6_ADDRSTRLEN];
    if ( result_address->ai_family == AF_INET ){
        inet_ntop(AF_INET, &((struct sockaddr_in *)result_address->ai_addr)->sin_addr, address_str, INET6_ADDRSTRLEN);
    }
    else{
        inet_ntop(AF_INET6, &((struct sockaddr_in6 *)result_address->ai_addr)->sin6_addr, address_str, INET6_ADDRSTRLEN);
    }

    std::cerr << "Connecting to " << address_str << std::endl;

    // Ok now we can open our socket
    int connection_socket = -1;
    if((connection_socket = socket(result_address->ai_family, result_address->ai_socktype, result_address->ai_protocol)) < 0){
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }
    // ...and connect it to the server
    if(connect(connection_socket, result_address->ai_addr, result_address->ai_addrlen)){
        perror("Cannot connect to server");
        close(connection_socket);
        exit(EXIT_FAILURE);
    }


    /**************************************************/
    /* Now we have successfully opened our socket and */
    /* connected to the server, we can start to       */
    /* communicate.                                   */
    /**************************************************/
    char recv_buffer[BUFFER_LEN];
    char send_buffer[BUFFER_LEN];
    memset(recv_buffer, 0, BUFFER_LEN);
    memset(send_buffer, 0, BUFFER_LEN);

    std::string last_response{};
    while(read(connection_socket, recv_buffer, BUFFER_LEN-1) > 0){
        std::cout << recv_buffer << std::endl; // print prompt from server
        last_response = recv_buffer;

        //get user input
    get_input:
        std::cout << ">?" << std::flush;
        std::string user_command{};
        std::getline(std::cin, user_command);

        //when retrieving file, ask for local name and then save to file
        if (user_command.substr(0,4) == "SEND" &&!(last_response[0] == '+' || last_response[0]=='-'|| last_response[0] == '!'|| last_response.empty())){

            std::string local_name{};
            std::cout << "Enter local name for retrieved file: " << std::flush;
            std::getline(std::cin, local_name);

            send(connection_socket, user_command.c_str(), strlen(user_command.c_str())+1, 0);

            long size = strtol(last_response.c_str(), nullptr, 0);
            long org_size = size;
            FILE *local_file = fopen(local_name.c_str(), "w");
            while(size>0){

                std::cout<< "DOWNLOADING: Done " << (int)((((double)org_size-(double)size)/(double)org_size)*100)<<"%   \r" << std::flush;
                size_t actually_allocd = MIN(1000000,size);
                char * file_buffer = (char *) calloc(actually_allocd, sizeof(char ));
                if(!file_buffer){
                    perror("Failed to allocate buffer");
                    exit(EXIT_FAILURE);
                }
                memset(file_buffer,0,actually_allocd);
                long bytes_read = read(connection_socket,file_buffer,actually_allocd);
                fwrite(file_buffer, sizeof(char), bytes_read, local_file);
                size -= bytes_read;
                free(file_buffer);
            }
            fclose(local_file);
            std::cout<< "DOWNLOADING: Done " << 100 <<"%   \r" << std::flush;
            std::cout << "\nFile saved" <<std::endl;
            goto get_input;
        } else if (user_command.substr(0,4) == "SEND"){
            std::cout << "Client: Warning, I dont think the server is expecting a SEND command!"<< std::endl;
        }

        if (user_command.substr(0,4) == "STOR"){
            std::string local_name{};
            size_t local_size = 0;
            std::cout << "Enter name of file to upload: "<<std::flush;
            std::getline(std::cin,local_name);
            if(access(local_name.c_str(), R_OK) != 0){
                std::cout<< "CLIENT: Error opening file: " << strerror(errno) <<std::endl;
                goto get_input;
            }
            struct stat file_stats{};
            if(stat(local_name.c_str(), &file_stats) == 0){
                local_size = file_stats.st_size;
                if((file_stats.st_mode & S_IFMT) != S_IFREG){
                    std::cout << "Error, not a file"<<std::endl;
                    goto get_input;
                }
            } else{
                std::cout << "Error getting size of the file: " << strerror(errno) <<std::endl;
                goto get_input;
            }

            // Send the users command
            send(connection_socket, user_command.c_str(), strlen(user_command.c_str())+1, 0);
            memset(recv_buffer, 0, BUFFER_LEN);
            // Read response
            if(read(connection_socket, recv_buffer, BUFFER_LEN - 1) <= 0){
                perror("Socket error");
                exit(EXIT_FAILURE);
            }
            last_response = recv_buffer;
            std::cout << recv_buffer << std::endl;
            if(last_response[0]=='+'){
                // Response was OK, sending size
                std::cout<<"Will upload " << local_size << " bytes to server"<<std::endl;
                send(connection_socket, std::to_string(local_size).c_str(), strlen(std::to_string(local_size).c_str())+1,0);
                // Wait for size confirmation..
                if(read(connection_socket, recv_buffer, BUFFER_LEN - 1) <= 0){
                    perror("Socket error");
                    exit(EXIT_FAILURE);
                }
                last_response = recv_buffer;
                std::cout << recv_buffer << std::endl;
                if(last_response[0] == '+'){
                    // Size ok, upload.
                    size_t remaining = local_size;
                    FILE *up_file = fopen(local_name.c_str(), "r");
                    while (remaining > 0){
                        // Inform the user ofc
                        std::cout << "UPLOADING: Done: "<<(int)((((float )(local_size-remaining))/((float)local_size))*100)<<"%   \r"<<std::flush;

                        size_t actually_allocated = MIN(1000000,remaining);
                        char* buffer = (char*) calloc(actually_allocated, sizeof(char));
                        if(!buffer){
                            perror("Allocation error");
                            exit(EXIT_FAILURE);
                        }
                        size_t read = fread(buffer, sizeof(char), actually_allocated, up_file);
                        remaining -= read;
                        size_t tb_sent = read;
                        while(tb_sent > 0){
                            size_t sent = send(connection_socket, buffer, read, 0);
                            tb_sent-=sent;
                        }
                        free(buffer);
                    }
                    std::cout<<"UPLOADING: Done: 100%     "<<std::endl;
                    fclose(up_file);
                    goto recv_response;
                }else{
                    // Size not ok, back to user
                    goto get_input;
                }

            }else{
                //Response not OK, back to user
                goto get_input;
            }
        }

        send(connection_socket, user_command.c_str(), strlen(user_command.c_str())+1, 0);
        if (user_command.substr(0,4) == "DONE"){
            break;
        }
        recv_response:
        memset(recv_buffer,0,BUFFER_LEN);
    }

    std::cerr << "Server closed connection" << std::endl;

    freeaddrinfo(result_address);
    close(connection_socket);
    return EXIT_SUCCESS;
}