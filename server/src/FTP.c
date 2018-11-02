//
//  FTP.c
//  FTP_server
//
//  Created by Poo Lei on 2018/10/26.
//  Copyright © 2018 Poo Lei. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include "FTP.h"
#include "log.h"


struct FTP_server ftp_server;
struct sockaddr_in control_addr;
struct sockaddr_in data_addr;
int local_ip;
int ftp_control_port;
char* rpath;

int server_start(){
    get_random_number();
    set_root_path(rpath);
    //server initialization
    control_addr.sin_family = AF_INET;
    control_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    control_addr.sin_port = htons(ftp_control_port);
    ftp_server.control_port_fd = create_TCP_socket(MAX_QUEUE,control_addr);
    
    for (int i = 0; i < MAX_CONCURRENT;i++){
        ftp_server.client_control_fds[i] = 0;
        ftp_server.client_data_fds[i] = 0;
        ftp_server.client_data_connection_fds[i] = 0;
        ftp_server.client_login[i] = -1;
        ftp_server.client_ID[i] = -1;
        ftp_server.mode[i].md = 0;
        ftp_server.upload_files[i] = 0;
        ftp_server.download_files[i] = 0;
        ftp_server.bytes[i] = 0;
        ftp_server.close_data_fds[i] = 0;
        ftp_server.path[i].current_path[0] = '/';
        ftp_server.rename_state[i] = 0;
    }
    return 0;
}

int ftp_server_process(){
    int max_fd = -1;
    while(1){
        update_select_settings(&max_fd);
        
        int ret = select(max_fd+1,&ftp_server.server_read_sets,&ftp_server.server_write_sets,NULL,&ftp_server.timeout);
        if(handlers(ret))
            break;
    }
    return 0;
}


int handlers(int select_ret){
    if (select_ret < 0){
        LOG("handlers:Problem select\n");
        return 0;
    }
    else if (select_ret == 0){
        //printf("Select overtime\n");
        return 0;
    }
    else{
        //handle new connection
        if (FD_ISSET(ftp_server.control_port_fd,&ftp_server.server_read_sets)){
            handler_new_connection();
        }
        //handle user request
        for (int i = 0; i < MAX_CONCURRENT;i++){
            if (FD_ISSET(ftp_server.client_data_connection_fds[i],&ftp_server.server_read_sets)){
                handler_pasv_data_connection(i);
            }
            if (FD_ISSET(ftp_server.client_control_fds[i],&ftp_server.server_read_sets)){
                handler_user(i);
            }
        }
    }
    return 0;
}


void handler_pasv_data_connection(int client_id){
    struct sockaddr_in client_addr;
    socklen_t length = sizeof(client_addr);
    int new_client_fd = accept(ftp_server.client_data_connection_fds[client_id],(struct sockaddr *) &client_addr,&length);
    
    if (new_client_fd < 0 ){
        LOG("handler_pasv_data_connection:Accept failed\n");
        return;
    }
    ftp_server.client_data_fds[client_id] = new_client_fd;
}


void handler_user(int client_id){
    //login
    if (ftp_server.client_login[client_id] == -1){
        handler_login(client_id);
    }
    else if (ftp_server.client_login[client_id] == 0){
        //Anonymous User
        handler_anonymous_user(client_id);
    }
    else{
        //TO DO - handle other user level
        printf("Other user level");
    }
}

void handler_login(int client_id){
    struct FTP_Request* req = read_request(client_id);
    if(req == NULL)
        return;
    //different situations
    if(strcmp(req->verb,"USER") == 0)
        handler_USER(client_id, req);
    else if(strcmp(req->verb,"PASS") == 0)
        handler_PASS(client_id, req);
    else if (strcmp(req->verb,"PASV") == 0)
        handler_PASV(client_id, req);
    else {
        struct FTP_Response msg1 = RES(530,"Not logged in");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
    free(req);
}


void handler_anonymous_user(int client_id){
    struct FTP_Request* req = read_request(client_id);
    if(req == NULL)
        return;
    if (ftp_server.rename_state[client_id] == 1){
        if(strcmp(req->verb, "RNTO") == 0)
            handler_RNTO(client_id, req);
        else{
            ftp_server.rename_state[client_id] = 0;
            struct FTP_Response msg1 = RES(553,"RNTO command expected");
            REPLY(ftp_server.client_control_fds[client_id],&msg1);
        }
        return;
    }
    if(strcmp(req->verb, "PORT") == 0)
        handler_PORT(client_id, req);
    else if (strcmp(req->verb,"PASV") == 0)
        handler_PASV(client_id, req);
    else if (strcmp(req->verb, "TYPE") == 0)
        handler_TYPE(client_id, req);
    else if (strcmp(req->verb, "SYST") == 0)
        handler_SYST(client_id, req);
    else if (strcmp(req->verb, "QUIT") == 0)
        handler_QUIT(client_id, req);
    else if (strcmp(req->verb, "ABOR") == 0)
        handler_ABOR(client_id, req);
    else if (strcmp(req->verb, "RETR") == 0)
        handler_RETR(client_id, req);
    else if ((strcmp(req->verb, "STOR") == 0))
        handler_STOR(client_id, req);
    else if ((strcmp(req->verb, "LIST") == 0))
        handler_LIST(client_id, req);
    else if (strcmp(req->verb,"PWD") == 0)
        handler_PWD(client_id, req);
    else if (strcmp(req->verb,"MKD") == 0)
        handler_MKD(client_id, req);
    else if (strcmp(req->verb,"RMD") == 0)
        handler_RMD(client_id, req);
    else if (strcmp(req->verb, "RNFR") == 0)
        handler_RNFR(client_id, req);
    else if(strcmp(req->verb, "CWD") == 0)
        handler_CWD(client_id,req);
    else{
        struct FTP_Response msg1 = RES(502,"Command not implemented");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
    free(req);
}

void client_disconnection(int client_id){
    if (ftp_server.client_data_connection_fds[client_id] != 0){
        if (close(ftp_server.client_data_connection_fds[client_id]) < 0){
            LOG("client_disconnection:Close client_data_connection_fds fail");
        }
        ftp_server.client_data_connection_fds[client_id] = 0;
    }
    if (ftp_server.client_data_fds[client_id] != 0){
        if (close(ftp_server.client_data_fds[client_id]) < 0){
            LOG("client_disconnection:Close client_data_fds fail");
        }
        ftp_server.client_data_fds[client_id] = 0;
    }
    FD_CLR(ftp_server.client_control_fds[client_id],&ftp_server.server_read_sets);
    FD_CLR(ftp_server.client_data_fds[client_id],&ftp_server.server_read_sets);
    FD_CLR(ftp_server.client_data_connection_fds[client_id],&ftp_server.server_read_sets);
    FD_CLR(ftp_server.client_data_connection_fds[client_id],&ftp_server.server_write_sets);
    ftp_server.client_control_fds[client_id] = 0;
    ftp_server.client_login[client_id] = -1;
    ftp_server.client_ID[client_id] = -1;
    ftp_server.mode[client_id].md = 0;
    ftp_server.upload_files[client_id] = 0;
    ftp_server.download_files[client_id] = 0;
    ftp_server.bytes[client_id] = 0;
    bzero(ftp_server.path[client_id].current_path,MAX_PATH_LENGTH);
    bzero(ftp_server.rename_dir[client_id].current_path,MAX_PATH_LENGTH);
    ftp_server.path[client_id].current_path[0] = '/';
    ftp_server.rename_state[client_id] = 0;
}


struct FTP_Request* read_request(int client_id){
    char buffer[COMMANDBUF]={0};
    
    long bytes = recv(ftp_server.client_control_fds[client_id],buffer,COMMANDBUF,0);
    if( bytes == -1){
        LOG("read_request:Reading failed\n");
        return NULL;
    }
    if (bytes == 0){
        printf("Client %d disconnect\n",client_id);
        client_disconnection(client_id);
        return NULL;
    }
    //parser
    struct FTP_Request* req = (struct FTP_Request*)malloc(sizeof(struct FTP_Request));
    for(int i = 0; i < MAX_REQUEST_WORDS;i++){
        req->param[i] = 0;
        req->verb[i] = 0;
    }
    int j = 0;
    bytes = strlen(buffer);
    //get rid of \r\n
    buffer[bytes - 1] = 0; // \n
    buffer[bytes - 2] = 0; // \r
    bytes -= 2;
    
    for (j = 0; j < bytes; j++){
        if(buffer[j] == ' '){
            //there is params
            buffer[j] = 0;
            strcpy(req->verb,buffer);
            strcpy(req->param,&buffer[j+1]);
            break;
        }
    }
    if (j == bytes){
        //no params
        strcpy(req->verb, buffer);
    }
    return req;
}

void handler_new_connection(){
    struct sockaddr_in client_addr;
    socklen_t length = sizeof(client_addr);
    int new_client_fd = accept(ftp_server.control_port_fd,(struct sockaddr *) &client_addr,&length);
    
    if (new_client_fd < 0 ){
        LOG("handler_new_connection:Accept failed\n");
        return;
    }
    
    int i = 0;
    for (i = 0; i < MAX_CONCURRENT;i++){
        if (ftp_server.client_control_fds[i] == 0){
            //Connection success
            ftp_server.client_control_fds[i] = new_client_fd;
            struct FTP_Response msg1 = RES(220,"Anonymous FTP server ready!");
            REPLY(new_client_fd, &msg1);
            printf("client %d connected\n",i);
            break;
        }
    }
    if (i == MAX_CONCURRENT){
        //reach MAX_CONCURRENT = Connection fail
        struct FTP_Response msg1 = RES(530,"Server busy!");
        REPLY(new_client_fd, &msg1);
    }
}


void update_select_settings(int* max){
    //timeout
    ftp_server.timeout.tv_sec = TIMEOUT_SEC;
    ftp_server.timeout.tv_usec = TIMEOUT_USEC;
    //fd set reset
    FD_ZERO(&ftp_server.server_read_sets);
    FD_ZERO(&ftp_server.server_write_sets);
    //set different fds
    FD_SET(ftp_server.control_port_fd,&ftp_server.server_read_sets);
    if (*max < ftp_server.control_port_fd){
        *max = ftp_server.control_port_fd;
    }
    //control port of clients
    for (int i = 0; i < MAX_CONCURRENT;i++){
        if (ftp_server.client_control_fds[i] != 0){
            FD_SET(ftp_server.client_control_fds[i],&ftp_server.server_read_sets);
            if (*max < ftp_server.client_control_fds[i])
                *max = ftp_server.client_control_fds[i];
            }
        if (ftp_server.close_data_fds[i] == 0){
            if (ftp_server.client_data_fds[i] != 0){
                FD_SET(ftp_server.client_data_fds[i],&ftp_server.server_read_sets);
                if (*max < ftp_server.client_data_fds[i]){
                    *max = ftp_server.client_data_fds[i];
                }
            }
            if (ftp_server.client_data_connection_fds[i] != 0){
                    FD_SET(ftp_server.client_data_connection_fds[i],&ftp_server.server_read_sets);
                if (*max < ftp_server.client_data_connection_fds[i]){
                    *max = ftp_server.client_data_connection_fds[i];
                }
            }
        }
        else{
            if(ftp_server.client_data_fds[i] > 0)
                close(ftp_server.client_data_fds[i]);
            if(ftp_server.client_data_connection_fds[i] > 0)
                close(ftp_server.client_data_connection_fds[i]);
            ftp_server.client_data_fds[i] = 0;
            ftp_server.client_data_connection_fds[i] = 0;
            ftp_server.mode[i].md = 0;
            ftp_server.close_data_fds[i] = 0;
        }
    }
}

int create_TCP_socket(int queue,struct sockaddr_in addr)
{
    int ld;
    socklen_t length;
    
    //create socket
    if ((ld = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG("create_TCP_socket:Problem creating socket\n");
        exit(1);
    }
    
    //bind socket
    if (bind(ld, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        LOG("create_TCP_socket:Problem binding\n");
        exit(1);
    }
    
    //listen socket
    if(listen(ld,queue) == -1)
    {
        LOG("create_TCP_socket:Problem listening\n");
        exit(1);
    }
    
    //test whether port is correct
    length = sizeof(addr);
    if (getsockname(ld, (struct sockaddr *) &addr, &length) < 0) {
        LOG("create_TCP_socket:Error getsockname\n");
        exit(1);
    }
    //print socket port
    printf("Control Port:%d\n",ntohs(addr.sin_port));
    return ld;
}

//Server Response
struct FTP_Response create_response(int cd,const char *message){
    struct FTP_Response res;
    for (int i = 0; i < MAX_WORDS_PER_LINE; i++){
        res.msg[i] = 0;
    }
    res.code = cd;
    strcpy(res.msg,message);
    res.next = NULL;
    return res;
}
void response_concat(struct FTP_Response* first,struct FTP_Response* second){
    first->next = second;
}

int server_reply(int fd,struct FTP_Response * res){
    struct FTP_Response *p = res;
    unsigned long count = 0;
    char buffer[MAX_REPLY_WORDS]={0};
    
    if (p->next == NULL){
        //one line reply
        sprintf(buffer, "%d %s\r\n",p->code,p->msg);
    }
    else{
        //multi-lines
        //first line
        sprintf(buffer, "%d-%s\r\n",p->code,p->msg);
        p = p->next;
        char temp[MAX_WORDS_PER_LINE] = {0};
        while(p->next){
            sprintf(temp,"%s\r\n",p->msg);
            strcat(buffer,temp);
            p = p->next;
        }
        //last line
        sprintf(temp, "%d %s\r\n",p->code,p->msg);
        strcat(buffer,temp);
    }
    count = strlen(buffer);
    if (send(fd, buffer, count, 0) < 0){
        LOG("server_reply:Server send failed\n");
        return 1;
    }
    return 0;
}

void handler_PWD(int client_id,struct FTP_Request* req){
    struct FTP_Response msg1 = RES(257,ftp_server.path[client_id].current_path);
    REPLY(ftp_server.client_control_fds[client_id],&msg1);
}

void handler_RMD(int client_id,struct FTP_Request*req){
    char real_path[MAX_PATH_LENGTH];
    get_real_path(real_path, req->param, client_id);
    if(rmdir(real_path) == 0){
        struct FTP_Response msg1 = RES(250,"Removing directory succeeded");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
    else{
        struct FTP_Response msg1 = RES(500,"Removing directory failed - Directory not exist or not empty");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
}

void handler_MKD(int client_id,struct FTP_Request*req){
    char real_path[MAX_PATH_LENGTH];
    get_real_path(real_path, req->param, client_id);
    if(mkdir(real_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0){
        struct FTP_Response msg1 = RES(257,"Making directory succeeded");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
    else{
        struct FTP_Response msg1 = RES(500,"Making directory failed");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
}

void handler_CWD(int client_id,struct FTP_Request*req){
    if (req->param[0] == '/'){
        //root path
        char real_path[MAX_PATH_LENGTH];
        get_real_path(real_path, req->param, client_id);
        if(opendir(real_path) == NULL){
            struct FTP_Response msg1 = RES(500,"Directory does not exist");
            REPLY(ftp_server.client_control_fds[client_id],&msg1);
            return;
        }
        long rlen = strlen(req->param);
        if (req->param[rlen - 1] != '/')
            req->param[rlen] = '/';
        strcpy(ftp_server.path[client_id].current_path,req->param);
        struct FTP_Response msg1 = RES(250,"Changing working directory succeeded");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
        return;
    }

    long pos[MAX_PATH_LENGTH];
    long len = strlen(req->param);
    if(req->param[len - 1] == '/'){
        req->param[len - 1] = 0;
        len --;
    }
    int count = 0;
    for (long i = 0; i < len ;i ++){
        if(req->param[i] == '/'){
            pos[count] = i;
            count++;
            req->param[i] = 0;
        }
    }
    
    char temp_path[MAX_PATH_LENGTH];
    strcpy(temp_path,ftp_server.path[client_id].current_path);
    char op[MAX_PATH_LENGTH];
    char *p = req->param;
    for (int i = 0; i <= count; i++){
        strcpy(op,p);
        if(analyse_path_one(temp_path, op) == 1){
            //error
            struct FTP_Response msg1 = RES(500,"Changing working directory failed");
            REPLY(ftp_server.client_control_fds[client_id],&msg1);
            return;
        }
        if (i < count)
            p = &req->param[pos[i] + 1];
    }
    strcpy(ftp_server.path[client_id].current_path,temp_path);
    struct FTP_Response msg1 = RES(250,"Changing working directory succeeded");
    REPLY(ftp_server.client_control_fds[client_id],&msg1);
}

int analyse_path_one(char * temp_path,char* cur_op){
    long len = strlen(temp_path);
    if(strcmp(cur_op,"..") == 0){
        if(strcmp(temp_path, "/") != 0){
            long i = len - 1;
            temp_path[i] = 0;
            i--;
            while(temp_path[i] != '/'){
                temp_path[i] = 0;
                i --;
            }
        }
    }
    else{
        if(strlen(cur_op) == 0)
            return 1;
        char real_path[MAX_PATH_LENGTH];
        strcpy(real_path,ftp_server.root_path);
        strcat(real_path,temp_path);
        strcat(real_path, cur_op);
        if(opendir(real_path) == NULL)
            return 1;
        strcat(temp_path,cur_op);
        strcat(temp_path,"/");
    }
    return 0;
}


void handler_RNFR(int client_id,struct FTP_Request*req){
    char real_path[MAX_PATH_LENGTH];
    get_real_path(real_path, req->param, client_id);
    //if(opendir(real_path) == NULL){
    //    struct FTP_Response msg1 = RES(450,"Directory does not exist");
    //    REPLY(ftp_server.client_control_fds[client_id],&msg1);
    //    return;
    //}
    ftp_server.rename_state[client_id] = 1;
    strcpy(ftp_server.rename_dir[client_id].current_path,real_path);
    struct FTP_Response msg1 = RES(350,"RNTO command needed");
    REPLY(ftp_server.client_control_fds[client_id],&msg1);
}

void handler_RNTO(int client_id,struct FTP_Request*req){
    if(ftp_server.rename_state[client_id] == 1){
        ftp_server.rename_state[client_id] = 0;
        char real_path[MAX_PATH_LENGTH];
        get_real_path(real_path, req->param, client_id);
        if(rename(ftp_server.rename_dir[client_id].current_path,real_path) != 0){
            struct FTP_Response msg1 = RES(500,"Rename Directory failed");
            REPLY(ftp_server.client_control_fds[client_id],&msg1);
            return;
        }
        struct FTP_Response msg1 = RES(250,"Rename Directory succeeded");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
    else{
        struct FTP_Response msg1 = RES(421,"RNFR command needed before");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
}

void handler_USER(int client_id,struct FTP_Request* req){
    if(strcmp(req->param,"anonymous") == 0){
        //client id = 0
        ftp_server.client_ID[client_id] = 0;
        struct FTP_Response msg1 = RES(331,"Anonymous user - please enter your email address as password");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
    //to do: other user mode
    else{
        struct FTP_Response msg1 = RES(504,"Anonymous user only");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
}

void handler_RETR(int client_id,struct FTP_Request*req){
    if(ftp_server.mode[client_id].md == 1)
        port_connect(client_id);
    else if(ftp_server.mode[client_id].md == 2){
        //pasv mode
        if (ftp_server.client_data_fds[client_id] == 0){
            struct FTP_Response msg1 = RES(425,"Can't open data connection");
            REPLY(ftp_server.client_control_fds[client_id],&msg1);
            ftp_server.close_data_fds[client_id] = 1;
            return;
        }
    }
    //data connection success
    ftp_server.download_files[client_id] += 1;
    //extract file name
    char filename [MAX_FILE_NAME];
    for (int i = 0; i<MAX_FILE_NAME;i++){
        filename[i] = 0;
    }
    strcpy(filename,ftp_server.root_path);
    strcat(filename,ftp_server.path[client_id].current_path);
    strcat(filename,req->param);
    FILE *fp = fopen(filename,"rb");
    //open file
    if (fp == NULL){
        struct FTP_Response msg1 = RES(551,"Can't open file from disk");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
        ftp_server.close_data_fds[client_id] = 1;
        ftp_server.mode[client_id].md = 0;
        return;
    }
    else{
        char rep[MAX_REPLY_WORDS];
        sprintf(rep,"Opening BINARY mode data connection for %s ",req->param);
        struct FTP_Response msg1 = RES(150,rep);
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
    //send data - new thread
    pthread_t t;
    struct thread_params *par = (struct thread_params *)malloc(sizeof(struct thread_params));
    par->client_id = client_id;
    par->fp = fp;
    if(pthread_create(&t, NULL, send_file, par) == -1){
        fclose(fp);
        struct FTP_Response msg1 = RES(551,"Can't create thread");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
        ftp_server.close_data_fds[client_id] = 1;
    }
    ftp_server.mode[client_id].md = 0;
}

void handler_LIST(int client_id,struct FTP_Request*req){
    if(ftp_server.mode[client_id].md == 1)
        port_connect(client_id);
    else if(ftp_server.mode[client_id].md == 2){
        //pasv mode
        if (ftp_server.client_data_fds[client_id] == 0){
            struct FTP_Response msg1 = RES(425,"Can't open data connection");
            REPLY(ftp_server.client_control_fds[client_id],&msg1);
            ftp_server.close_data_fds[client_id] = 1;
            return;
        }
    }
    int fd = ftp_server.client_data_fds[client_id];

    char real_path[MAX_PATH_LENGTH];
    char temp[MAX_FILE_NAME];
    //get_real_path(real_path, req->param, client_id);
    if (strlen(req->param) == 0){
        strcpy(real_path,ftp_server.root_path);
        strcat(real_path,ftp_server.path[client_id].current_path);
    }
    else
        get_real_path(real_path, req->param, client_id);
    
    struct dirent *dp;
    DIR *dir = NULL;
    if((dir = opendir(real_path)) == NULL){
        struct FTP_Response msg1 = RES(500,"LIST query fail");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
        ftp_server.close_data_fds[client_id] = 1;
        return;
    }
    char output[MAXBUF];
    bzero(output, MAXBUF);
    struct FTP_Response msg1 = RES(150,"LIST query success");
    REPLY(ftp_server.client_control_fds[client_id],&msg1);
    while ((dp = readdir(dir)) != NULL) {
        sprintf(temp,"%s\r\n",dp->d_name);
        strcat(output,temp);
    }
    closedir(dir);
    send(fd,output,strlen(output),0);
    struct FTP_Response msg2 = RES(250,"LIST transfer success");
    REPLY(ftp_server.client_control_fds[client_id],&msg2);
    ftp_server.mode[client_id].md = 0;
    ftp_server.close_data_fds[client_id] = 1;
}

void handler_STOR(int client_id,struct FTP_Request*req){
    if(ftp_server.mode[client_id].md == 1)
        port_connect(client_id);
    else if(ftp_server.mode[client_id].md == 2){
        //pasv mode
        if (ftp_server.client_data_fds[client_id] == 0){
            struct FTP_Response msg1 = RES(425,"Can't open data connection");
            REPLY(ftp_server.client_control_fds[client_id],&msg1);
            ftp_server.close_data_fds[client_id] = 1;
            return;
        }
    }
    //data connection success
    ftp_server.upload_files[client_id] += 1;
    //extract file name
    char filename [MAX_FILE_NAME];
    for (int i = 0; i<MAX_FILE_NAME;i++){
        filename[i] = 0;
    }
    strcpy(filename,ftp_server.root_path);
    strcat(filename,ftp_server.path[client_id].current_path);
    strcat(filename,req->param);
    FILE *fp = fopen(filename,"wb");
    //open file
    if (fp == NULL){
        struct FTP_Response msg1 = RES(551,"Can't create file");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
        ftp_server.close_data_fds[client_id] = 1;
        return;
    }
    else{
        char rep[MAX_REPLY_WORDS];
        sprintf(rep,"Opening BINARY mode data connection for %s ",req->param);
        struct FTP_Response msg1 = RES(150,rep);
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
    //receive data - new thread
    pthread_t t;
    struct thread_params *par = (struct thread_params *)malloc(sizeof(struct thread_params));
    par->client_id = client_id;
    par->fp = fp;
    if(pthread_create(&t, NULL, receive_file, par) == -1){
        fclose(fp);
        struct FTP_Response msg1 = RES(551,"Can't create thread");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
        ftp_server.close_data_fds[client_id] = 1;
    }
    ftp_server.mode[client_id].md = 0;
}

void handler_PASS(int client_id,struct FTP_Request*req){
    if (ftp_server.client_ID[client_id] == 0){
        //if validate
        ftp_server.client_login[client_id] = 0;
        //welcome message
        struct FTP_Response msg1 = RES(230,"Welcome to");
        struct FTP_Response msg2 = RES(230,"leiyiran's ftp server");
        CAT(&msg1,&msg2);
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
    else if (ftp_server.client_ID[client_id] == -1){
        //not input username
        struct FTP_Response msg1 = RES(332,"Need account for login");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
    else{
        //to do: other user mode
        struct FTP_Response msg1 = RES(530,"Anonymous user only");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
}

void handler_PORT(int client_id,struct FTP_Request*req){
    set_port_mode(client_id, req->param);
    if (ftp_server.client_data_connection_fds[client_id] != 0){
        if (close(ftp_server.client_data_connection_fds[client_id]) < 0){
            LOG("handler_PORT:Close connection fail");
        }
        ftp_server.client_data_connection_fds[client_id] = 0;
    }
    if (ftp_server.client_data_fds[client_id] != 0){
        if (close(ftp_server.client_data_fds[client_id]) < 0){
            LOG("handler_PORT:Close connection fail");
        }
        ftp_server.client_data_fds[client_id] = 0;
    }
    struct FTP_Response msg1 = RES(200,"PORT command successful");
    REPLY(ftp_server.client_control_fds[client_id],&msg1);
}

void handler_PASV(int client_id,struct FTP_Request*req){
    if (ftp_server.client_data_connection_fds[client_id] != 0){
        if (close(ftp_server.client_data_connection_fds[client_id]) < 0){
            LOG("handler_PASV:Close client_data_connection_fds fail");
        }
        ftp_server.client_data_connection_fds[client_id] = 0;
    }
    if (ftp_server.client_data_fds[client_id] != 0){
        if (close(ftp_server.client_data_fds[client_id]) < 0){
            LOG("handler_PASV:Close client_data_fds fail");
        }
        ftp_server.client_data_fds[client_id] = 0;
    }
    set_pasv_mode(client_id);
    //reply message
    int port = ntohs(ftp_server.mode[client_id].addr.sin_port);
    struct FTP_Port p = P2FP(port);
    char rep[MAX_REPLY_WORDS];
    char tmp[MAX_REPLY_WORDS];
    for (int i = 0; i < MAX_REPLY_WORDS; i++){
        tmp[i] = 0;
        rep[i] = 0;
    }
    strcpy(rep,"Entering Passive Mode(");
    sprintf(tmp,"%d,%d,%d,%d,%d,%d)",(local_ip>>24)&0xff,(local_ip>>16)&0xff,(local_ip>>8)&0xff,local_ip&0xff,p.p1,p.p2);
    strcat(rep, tmp);
    struct FTP_Response msg1 = RES(227,rep);
    REPLY(ftp_server.client_control_fds[client_id],&msg1);
}

void handler_SYST(int client_id,struct FTP_Request*req){
    struct FTP_Response msg1 = RES(215,"UNIX Type: L8");
    REPLY(ftp_server.client_control_fds[client_id],&msg1);
}
void handler_TYPE(int client_id,struct FTP_Request*req){
    if(strcmp(req->param,"I") == 0){
        struct FTP_Response msg1 = RES(200,"Type set to I.");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
    else{
        struct FTP_Response msg1 = RES(501,"Only Type I supported");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
    }
}

void handler_QUIT(int client_id,struct FTP_Request*req){
    char buffer[MAX_REPLY_WORDS],buffer2[MAX_REPLY_WORDS];
    for (int i = 0; i< MAX_REPLY_WORDS;i++){
        buffer2[i] = 0;
        buffer[i] = 0;
    }
    sprintf(buffer,"You have transferred %d bytes",ftp_server.bytes[client_id]);
    struct FTP_Response msg1 = RES(221,buffer);
    sprintf(buffer2,"Upload %d files and download %d files",ftp_server.upload_files[client_id],ftp_server.download_files[client_id]);
    struct FTP_Response msg2 = RES(221,buffer2);
    struct FTP_Response msg3 = RES(221,"Thank you for using leiyiran's ftp server");
    struct FTP_Response msg4 = RES(221,"Goodbye!");
    CAT(&msg1,&msg2);
    CAT(&msg2,&msg3);
    CAT(&msg3,&msg4);
    REPLY(ftp_server.client_control_fds[client_id],&msg1);
    printf("Client %d disconnect\n",client_id);
    client_disconnection(client_id);
}

void handler_ABOR(int client_id,struct FTP_Request*req){
    handler_QUIT(client_id, req);
}

int get_random_number(){
    srand((int)time(NULL));
    return (double)rand()/(double)RAND_MAX *45535 + 20000;
}

struct FTP_Port number_to_ftp_port(int pnumber){
    struct FTP_Port ftp_p;
    ftp_p.p1 = pnumber/256;
    ftp_p.p2 = pnumber - ftp_p.p1*256;
    return ftp_p;
}

int ftp_port_to_number(int p1,int p2){
    return p1*256 + p2;
}


int set_port_mode(int client_id,char * param){
    long len = strlen(param);
    int comma_pos[5];
    int comma_count = 0;
    for(int i = 0; i < len;i++){
        if (param[i] == ','){
            comma_pos[comma_count] = i;
            param[i] = 0;
            comma_count += 1;
        }
    }
    if (comma_count != 5)
        return 1;
    int ip[6];
    char * p = param;
    for (int i = 0; i < 5; i++){
        ip[i] = atoi(p);
        p = &param[comma_pos[i]+1];
    }
    ip[5] = atoi(p);
    
    int ip_address = (ip[0] << 24) + (ip[1]<<16) + (ip[2]<<8) + ip[3];
    int port = FP2P(ip[4], ip[5]);

    
    //set mode
    ftp_server.mode[client_id].md = 1;
    ftp_server.mode[client_id].addr.sin_family = AF_INET;
    ftp_server.mode[client_id].addr.sin_port = htons(port);
    ftp_server.mode[client_id].addr.sin_addr.s_addr = htonl(ip_address);
    return 0;
}

int set_pasv_mode(int client_id){
    int port = 0;
    ftp_server.mode[client_id].md = 2;
    ftp_server.mode[client_id].addr.sin_family = AF_INET;
    ftp_server.mode[client_id].addr.sin_addr.s_addr = htonl(INADDR_ANY);
    int ld = 0;
    if ((ld = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG("set_pasv_mode:Problem creating socket\n");
        return 1;
    }
    
    do{
        port = Random();
        ftp_server.mode[client_id].addr.sin_port = htons(port);
    }while(bind(ld, (struct sockaddr *) &ftp_server.mode[client_id].addr, sizeof(ftp_server.mode[client_id].addr)) < 0);
    
    if(listen(ld,1) == -1)
    {
        LOG("set_pasv_mode:Problem listening\n");
        return 1;
    }
    ftp_server.client_data_connection_fds[client_id] = ld;
    return 0;
}

void* send_file(void *param){
    struct thread_params* par = (struct thread_params*)param;
    int fd = ftp_server.client_data_fds[par->client_id];
    
    ftp_server.file_count[par->client_id] = 0;
    while (!feof (par->fp)){
        ftp_server.file_count[par->client_id] = fread (ftp_server.file_buffer, sizeof (char), MAXBUF, par->fp);
        send(fd,ftp_server.file_buffer,ftp_server.file_count[par->client_id],0);
        ftp_server.bytes[par->client_id] += ftp_server.file_count[par->client_id];
    }
    fclose(par->fp);
    struct FTP_Response msg1 = RES(226,"Transfer complete");
    REPLY(ftp_server.client_control_fds[par->client_id],&msg1);
    ftp_server.close_data_fds[par->client_id] = 1;
    free(par);
    return NULL;
}

void* receive_file(void*param){
    struct thread_params* par = (struct thread_params*)param;
    int fd = ftp_server.client_data_fds[par->client_id];
    
    ftp_server.file_count[par->client_id] = 0;
    while((ftp_server.file_count[par->client_id] = recv(fd,ftp_server.file_buffer,MAXBUF,0))){
        fwrite(ftp_server.file_buffer,sizeof(char), ftp_server.file_count[par->client_id], par->fp);
        ftp_server.bytes[par->client_id] += ftp_server.file_count[par->client_id];
    }
    fclose(par->fp);
    struct FTP_Response msg1 = RES(226,"Transfer complete");
    REPLY(ftp_server.client_control_fds[par->client_id],&msg1);
    ftp_server.close_data_fds[par->client_id] = 1;
    free(par);
    return NULL;
}


void set_root_path(char *rp){
    if (rp == NULL){
        strcpy(ftp_server.root_path,"/tmp");
        return;
    }
    if (opendir(rp) == NULL){
        strcpy(ftp_server.root_path,"/tmp");
        LOG("Input root path does not exist");
        return;
    }
    long len = strlen(rp);
    if (rp[len - 1] == '/')
        rp[len - 1] = 0;
    strcpy(ftp_server.root_path,rp);
}

void get_real_path(char *dest,char *input,int client_id){
    if (input[0] == '/'){
        strcpy(dest,ftp_server.root_path);
        strcat(dest,input);
    }
    else{
        strcpy(dest, ftp_server.root_path);
        strcat(dest,ftp_server.path[client_id].current_path);
        strcat(dest,input);
    }
}

void port_connect(int client_id){
    //connect
    ftp_server.client_data_fds[client_id] = socket(AF_INET, SOCK_STREAM, 0);
    if(ftp_server.client_data_fds[client_id] == -1)
    {
        struct FTP_Response msg1 = RES(425,"Can't open data connection");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
        LOG("handler_RETR:socket error");
        return;
    }
    if(connect(ftp_server.client_data_fds[client_id], (struct sockaddr *)&ftp_server.mode[client_id].addr, sizeof(struct sockaddr_in))!=0){
        struct FTP_Response msg1 = RES(425,"Can't open data connection");
        REPLY(ftp_server.client_control_fds[client_id],&msg1);
        LOG("handler_RETR:connection error");
        return;
    }
}
