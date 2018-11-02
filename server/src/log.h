//
//  log.h
//  FTP_server
//
//  Created by Poo Lei on 2018/10/26.
//  Copyright © 2018 Poo Lei. All rights reserved.
//

#ifndef log_h
#define log_h

#include <stdio.h>

#define LOG(msg) write_log(msg)

extern FILE* LOGFILE;
void open_log_file(void);
void close_log_file(void);
int write_log(char* msg);

#endif /* log_h */
