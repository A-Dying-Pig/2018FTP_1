//
//  log.c
//  FTP_server
//
//  Created by Poo Lei on 2018/10/26.
//  Copyright © 2018 Poo Lei. All rights reserved.
//

#include "log.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
FILE *LOGFILE;

void open_log_file(){
    char filename[128];
    strcpy(filename,"/tmp/log.txt");
    FILE * fp = fopen(filename,"a+");
    if (fp == NULL){
        printf("Open log file failed!\n");
        exit(1);
    }
    LOGFILE = fp;
}

void close_log_file(){
    if (fclose(LOGFILE) == EOF){
        printf("Close log file failed!\n");
        exit(1);
    }
}

int write_log(char *msg){
    if(fputs(msg,LOGFILE) == EOF){
        printf("Write log message failed!\n");
        return 1;
    }
    return 0;
}
