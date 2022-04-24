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
#include <getopt.h>
#include <map>
#include <fstream>
#include <array>
#include <signal.h>

#include "debug-headers.h"
#include "serve_client.h"

#define CLIENT_QUEUE_LEN 3


void print_help(){
    std::cout << "Usage:\n"
                 "-i <interface>, --interface <interface>               Define interface to listen on (optional)\n"
                 "-p <number>, --port <number>                          Define port number to listen on (optional)\n"
                 "                 -Please note, choosing low port number (like the default 115) might\n"
                 "                  require superuser privileges.\n"
                 "-u <file_path>, --users <file_path>                   Define file with user database\n"
                 "-f <path>, --folder <path>                            Specify the working directory of the server"
                 << std::endl;
    exit(EXIT_SUCCESS);
}

std::map<std::string, std::string> parse_user_db(char *user_db_file_path){
    std::map<std::string, std::string> ret{};

    //first open file
    std::ifstream user_db_file(user_db_file_path);
    if(!user_db_file.is_open()){
        std::cerr << "Cannot open user_db_file file" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string line;
    while(std::getline(user_db_file, line)){
        //We get a line, we have to split it by ':' to get the format <username>:<password>
        unsigned long int pos = 0;
        std::string username{};
        std::string password{};
        for (int i = 0;(pos = line.find(':')) != std::string::npos; i++){
            std::string token = line.substr(0,pos);
            line.erase(0, pos + 1);
            if(i==0){
                username = token;
                continue;
            }
            std::cerr << "Bad user database format!" << std::endl;
            exit(EXIT_FAILURE);
        }
        password = line; // the rest on the line

        //Now we have user and his password, let's pair them together and store them in the map.
        ret.insert(std::pair<std::string, std::string>(username, password));
    }

    user_db_file.close();

    return ret;
}

void handleSigInt(int dum){
    std::cerr << "Caught SIGINT - exiting" <<std::endl;
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    int opt_run_once = 0;
    char * interface_name = nullptr;
    char * port_str = nullptr;
    char * users_db_path = nullptr;
    char * work_dir = nullptr;

    //First define args
    struct option long_opts[] = {
            {"debug", no_argument, &opt_run_once, 1},
            {"interface", required_argument, nullptr, 'i'},
            {"port", required_argument, nullptr, 'p'},
            {"users", required_argument, nullptr, 'u'},
            {"folder", required_argument, nullptr, 'f'},
            {nullptr, 0, nullptr, 0}
    };

    //now we can parse them
    int arg_cur = '\0';
    while((arg_cur = getopt_long(argc,argv,"i:p:u:f:", long_opts, nullptr)) != -1){
        switch(arg_cur){
            case 0:
                break;
            case 'i':
                std::cerr << "Setting interface to " << optarg << std::endl;
                interface_name = optarg;
                break;
            case 'p':
                std::cerr << "Setting port to " << optarg << std::endl;
                port_str = optarg;
                break;
            case 'u':
                std::cerr << "Setting user_db file to " << optarg << std::endl;
                users_db_path = optarg;
                break;
            case 'f':
                std::cerr << "Setting folder to " << optarg << std::endl;
                work_dir = optarg;
                break;
            case '?':
                print_help();
                break;
            default:
                std::cerr << "Unknown error parsing arguments" << std::endl;
                exit(EXIT_FAILURE);
        }
    }

    if(!(work_dir && users_db_path)){
        print_help();
    }
    // Ok so we got all the arguments we need, so let's verify them
    // First let's see if we got an interface, else bind to all
    unsigned int interface_index = 0; // bind to all
    if (interface_name){
        if ((interface_index = if_nametoindex(interface_name)) == 0){
            perror("Error binding to interface");

            std::cout << "List of all available interfaces:" << std::endl;

            struct if_nameindex *if_list;
            if ( (if_list = if_nameindex()) != nullptr){
                for (struct if_nameindex *if_desc = if_list; if_desc->if_index != 0 || if_desc->if_name != nullptr; if_desc++){
                    std::cout << if_desc->if_name <<std::endl;
                }
                if_freenameindex(if_list);
            }else{
                std::cerr << "Error printing all interfaces" <<std::endl;
            }

            exit(EXIT_FAILURE);
        }
    }

    // Lets check port number
    unsigned short port_number = 115; // 115 by default
    if(port_str){
        uint port_no_temp = strtoul(port_str, nullptr, 0);
        if(port_no_temp > UINT16_MAX){
            std::cout << port_no_temp << " is not a valid port number. <0-" << UINT16_MAX << ">" <<std::endl;
            exit(EXIT_FAILURE);
        }
        port_number = (unsigned short ) port_no_temp;
    }

    //Parse user database
    std::map<std::string, std::string> user_db = parse_user_db(users_db_path);

    // Only thing remaining is to change our work directory
    if(chdir(work_dir) == -1){
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }




    // This site has been used as a reference: https://www.ibm.com/docs/en/i/7.2?topic=sscaaiic-example-accepting-connections-from-both-ipv6-ipv4-clients
    // But this code is in no shape or form a copy, more like a loose interpretation.

    std::cerr << "Hello, World!\nStarting server" << std::endl;

    struct sockaddr_in6 listening_address{};
    memset(&listening_address, 0, sizeof(listening_address));
    listening_address.sin6_addr = in6addr_any;
    listening_address.sin6_family = AF_INET6;
    listening_address.sin6_port = htons(port_number);
    listening_address.sin6_scope_id = interface_index;


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

    std::cerr << "Setting up SIGINT handler" <<std::endl;
    signal(SIGINT, handleSigInt);

    while(true){
        std::cerr << "Waiting for connection" << std::endl;
        // Reset workdir, if the previous user changed it
        if(chdir(work_dir) == -1){
            perror("Error opening directory");
            exit(EXIT_FAILURE);
        }

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

        serve_client(&clientInfo, user_db);


        close(client_socket);

        std::cerr << "Connection to " << client_ip_str << " closed" << std::endl;

        if(opt_run_once){
            std::cerr << "Debug flag set, closing after one connection."<<std::endl;
            break;
        }
    }


    close(server_socket);
    return 0;


}