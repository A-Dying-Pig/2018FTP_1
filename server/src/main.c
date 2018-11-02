//
//  main.c
//  FTP_server
//
//  Created by Poo Lei on 2018/10/25.
//  Copyright © 2018 Poo Lei. All rights reserved.
//

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include "log.h"
#include "FTP.h"
#include <stdlib.h>
#include <string.h>


int main(int argc, const char * argv[]) {
    ftp_control_port = 21;
    local_ip = DEFAULT_IP;
    rpath = NULL;
    if (argc != 1){
        for (int i = 1;i<argc;i++)
        {
            if (strcmp(argv[i],"-port") == 0){
                ftp_control_port = atoi(argv[i+1]);
            }
            else if (strcmp(argv[i], "-root") == 0){
                rpath = (char*)malloc(sizeof(char) * MAX_PATH_LENGTH);
                strcpy(rpath,argv[i+1]);
            }
            else if (strcmp(argv[i], "-ip") == 0){
                local_ip = ntohl(inet_addr(argv[i+1]));
            }
        }
    }
    printf("Server IP: %x\n",local_ip);
    open_log_file();
    server_start();
    free(rpath);
    ftp_server_process();
    close_log_file();
 
    
    return 0;
}
