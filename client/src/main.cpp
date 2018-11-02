//
//  main.cpp
//  ftp_client
//
//  Created by Poo Lei on 2018/10/29.
//  Copyright © 2018 Poo Lei. All rights reserved.
//

#include <iostream>
#include "ftp_client.hpp"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, const char * argv[]) {
    int server_ip = DEFAULT_IP;
    int server_port = DEFAULT_PORT;
    if (argc != 1){
        for (int i = 1;i<argc;i++)
        {
            if (strcmp(argv[i],"-port") == 0){
                server_port = atoi(argv[i+1]);
            }
            else if (strcmp(argv[i], "-ip") == 0){
                server_ip = ntohl(inet_addr(argv[i+1]));
            }
        }
    }
    ftp_client client(server_ip,server_port);
    getcwd(client.sys_path, MAX_FILE_NAME);
    strcat(client.sys_path,"/");
    client.connect_to_server();
    client.process();
    return 0;
}
