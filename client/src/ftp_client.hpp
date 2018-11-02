//
//  ftp_server.hpp
//  ftp_client
//
//  Created by Poo Lei on 2018/10/29.
//  Copyright © 2018 Poo Lei. All rights reserved.
//

#ifndef ftp_client_hpp
#define ftp_client_hpp

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>

#define DEFAULT_IP 0x7f000001
#define DEFAULT_PORT 21
#define INPUT_BUFFER 128
#define OUTPUT_BUFFER 512
#define MAX_FILE_NAME 64
#define FILE_BUFFER 1024*1024



struct Request{
    char verb[INPUT_BUFFER];
    char param[INPUT_BUFFER];
};

struct pasv_address{
    int ip;
    int port;
};

class ftp_client{
    struct Request current_request;
    int ip_address;
    int control_fd;
    int data_fd;
    int data_port_fd;
    int control_port;
    struct timeval timeout;
    fd_set set;
    char input_buffer[INPUT_BUFFER];
    char output_buffer[OUTPUT_BUFFER];
    char file_buffer[FILE_BUFFER];
    int mode;
    int max_fd;
    struct pasv_address pasv_addr;
public:
    char sys_path[MAX_FILE_NAME];
    ftp_client(int ip = DEFAULT_IP,int prt = DEFAULT_PORT);
    int process();
    int connect_to_server();
    void update_select();
    void handler_send_message();
    void handler_receive_message();
    void handler_PASV();
    void handler_PORT();
    void handler_RETR();
    void handler_LIST();
    void handler_STOR();
    void handler_receive_data();
    void handler_port_data();
    void handler_set_ip();
    
    void send_file(FILE *fp,int fd);
    void receive_file(FILE *fp,int fd);
    void pasv_connect();
};



#endif /* ftp_server_hpp */
