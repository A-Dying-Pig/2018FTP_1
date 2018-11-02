//
//  ftp_server.cpp
//  ftp_client
//
//  Created by Poo Lei on 2018/10/29.
//  Copyright © 2018 Poo Lei. All rights reserved.
//

#include "ftp_client.hpp"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>

ftp_client::ftp_client(int ip,int prt){
    this->ip_address = ip;
    this->control_fd = 0;
    this->data_fd = 0;
    this->control_port = prt;
    this->mode = 0;
    this->max_fd = 0;
    bzero(sys_path, MAX_FILE_NAME);

}

int ftp_client::connect_to_server(){
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(this->control_port);
    server_addr.sin_addr.s_addr = htonl(this->ip_address);
    this->control_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(this->control_fd == -1)
    {
        perror("socket error");
        exit(1);
    }
    if(connect(this->control_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in))!=0){
        perror("connection error");
        exit(1);
    }
    return 0;
}

int ftp_client::process(){
    while(1){
        update_select();
        select(max_fd + 1, &this->set, NULL, NULL, &this->timeout);
        if(FD_ISSET(STDIN_FILENO, &set))
            handler_send_message();
        else if (FD_ISSET(data_port_fd,&set))
            handler_port_data();
        else if(FD_ISSET(control_fd, &set))
            handler_receive_message();
        else if (FD_ISSET(data_fd,&set))
            handler_receive_data();
    }
    return 0;
}

void ftp_client::handler_port_data(){
    struct sockaddr_in server_addr;
    socklen_t length = sizeof(server_addr);
    int new_client_fd = accept(data_port_fd,(struct sockaddr *) &server_addr,&length);
    
    if (new_client_fd < 0 ){
        perror("Accept failed\n");
        return;
    }
    data_fd = new_client_fd;
    if (strcmp(current_request.verb,"STOR") == 0 && mode == 1)
        handler_STOR();
}

void ftp_client::update_select(){
    this->max_fd = 0;
    this->timeout.tv_sec = 5;
    this->timeout.tv_usec = 0;
    FD_ZERO(&this->set);
    FD_SET(STDIN_FILENO, &this->set);
    FD_SET(control_fd, &this->set);
    if(max_fd < control_fd)
        max_fd = control_fd;
    if (data_fd > 0){
        FD_SET(data_fd,&set);
        if (max_fd < data_fd)
            max_fd = data_fd;
    }
    if (data_port_fd > 0){
        FD_SET(data_port_fd,&set);
        if (max_fd < data_port_fd)
            max_fd = data_port_fd;
    }
}

void ftp_client::handler_send_message(){
    bzero(this->input_buffer, INPUT_BUFFER);
    std::cin.getline(this->input_buffer, INPUT_BUFFER);
    
    char temp[INPUT_BUFFER];
    strcpy(temp,input_buffer);
    strcat(this->input_buffer,"\r\n");
    
    //parser
    for(int i = 0; i < INPUT_BUFFER;i++){
        this->current_request.param[i] = 0;
        this->current_request.verb[i] = 0;
    }
    long bytes = strlen(temp);
    int j = 0;
    for (j = 0; j < bytes; j++){
        if(temp[j] == ' '){
            //there is params
            temp[j] = 0;
            strcpy(this->current_request.verb,temp);
            strcpy(this->current_request.param,&temp[j+1]);
            break;
        }
    }
    if (j == bytes){
        //no params
        strcpy(this->current_request.verb, temp);
    }
   
    //different situation
    if (strcmp(current_request.verb, "PASV") == 0)
        mode = 2;
    else if (strcmp(current_request.verb,"PORT") == 0)
        handler_PORT();
    if (strcmp(current_request.verb,"STOR") == 0 && mode == 2){
        pasv_connect();
    }
    else if (strcmp(current_request.verb,"RETR") == 0 && mode == 2)
        pasv_connect();
    else if (strcmp(current_request.verb,"LIST") == 0 && mode == 2)
        pasv_connect();
    
    //send cmdU
    if(send(this->control_fd, this->input_buffer, strlen(this->input_buffer), 0) == -1)
    {
        printf("send message error\n");
        return;
    }
    if (strcmp(current_request.verb,"STOR") == 0 && mode == 2){
        handler_STOR();
    }
}

void ftp_client::handler_STOR(){
    if (data_fd <= 0){
        printf("error:data connection not established\n");
        return;
    }
    //file name
    char filename[INPUT_BUFFER];
    strcpy(filename,sys_path);
    strcat(filename,current_request.param);
    FILE *fp = fopen(filename,"rb");
    if (fp != NULL){
        send_file(fp,data_fd);
    }
    //close connection
    close(data_fd);
    data_fd = 0;
    mode = 0;
}

void ftp_client::handler_receive_data(){
    //port mode
    if ((strcmp(current_request.verb,"RETR") == 0) && mode != 0)
        handler_RETR();
    else if ((strcmp(current_request.verb,"LIST") == 0) && mode != 0)
        handler_LIST();
}

void ftp_client::handler_RETR(){
    if (data_fd <= 0){
        printf("error:data connection not established\n");
        return;
    }
    char filename[MAX_FILE_NAME];
    strcpy(filename,sys_path);
    strcat(filename,current_request.param);
    FILE *fp = fopen(filename,"wb");
    if (fp != NULL)
        receive_file(fp, data_fd);
    //close connection
    close(data_fd);
    data_fd = 0;
    mode = 0;
}

void ftp_client::handler_LIST(){
    if (data_fd <= 0){
        printf("error:data connection not established\n");
        return;
    }
    char listoutput[FILE_BUFFER];
    bzero(listoutput,FILE_BUFFER);
    recv(data_fd,listoutput,FILE_BUFFER,0);
    std::cout<<listoutput;
    close(data_fd);
    data_fd = 0;
    mode = 0;
}

void ftp_client::handler_receive_message(){
    bzero(this->output_buffer, OUTPUT_BUFFER);
    long bytes = recv(this->control_fd, this->output_buffer, OUTPUT_BUFFER, 0);
    if(bytes > 0)
    {
        //print server reply
        if(bytes > OUTPUT_BUFFER)
        {
            bytes = OUTPUT_BUFFER;
        }
        this->output_buffer[bytes - 1] = '\0';
        printf(">>%s\n", this->output_buffer);
        
        //situations
        if (strcmp(this->current_request.verb,"QUIT") == 0)
            exit(0);
        else if (strcmp(this->current_request.verb,"ABOR") == 0)
            exit(0);
        else if (strcmp(this->current_request.verb,"PASV") == 0)
            handler_PASV();
    }
    else if(bytes < 0)
        printf("error when accepting message!\n");
    else
    {
        printf("server disconnect!\n");
        exit(0);
    }
}

void ftp_client::handler_PASV(){
    output_buffer[3] = 0;
    if(atoi(output_buffer) != 227){
        printf("error in pasv command\n");
        return;
    }
    output_buffer[3] = ' ';
    long len = strlen(output_buffer);
    int comma_pos[5];
    int comma_count = 0;
    for(int i = 0; i < len;i++){
        if (output_buffer[i] == ','){
            comma_pos[comma_count] = i;
            output_buffer[i] = 0;
            comma_count += 1;
        }
    }
    if (comma_count != 5){
        printf("error in pasv command\n");
        return;
    }
    int ip[6];
    //227 =xx,xx,xx,xx,yy,yy
    char *p = &output_buffer[comma_pos[0] - 1];
    while(*p <= '9' && *p >='0')
        p--;
    p++;
    //char * p = &output_buffer[5];
    for (int i = 0; i < 5; i++){
        ip[i] = atoi(p);
        p = &output_buffer[comma_pos[i]+1];
    }
    ip[5] = atoi(p);
    
    pasv_addr.ip = (ip[0] << 24) + (ip[1]<<16) + (ip[2]<<8) + ip[3];
    pasv_addr.port = ip[4]*256 + ip[5];
}

void ftp_client::handler_PORT(){
    long len = strlen(current_request.param);
    int comma_pos[5];
    int comma_count = 0;
    for(int i = 0; i < len;i++){
        if (current_request.param[i] == ','){
            comma_pos[comma_count] = i;
            current_request.param[i] = 0;
            comma_count += 1;
        }
    }
    int ip[6];
    //227 =xx,xx,xx,xx,yy,yy
    char * p = current_request.param;
    for (int i = 0; i < 5; i++){
        ip[i] = atoi(p);
        p = &current_request.param[comma_pos[i]+1];
    }
    ip[5] = atoi(p);
    
    int port = ip[4]*256 + ip[5];
    mode = 1;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    
    int ld = 0;
    if ((ld = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Problem creating socket\n");
        return;
    }
    if(bind(ld, (struct sockaddr *) &addr, sizeof(addr)) < 0){
        perror("Problem binding\n");
        return;
    }
  
    if(listen(ld,1) == -1)
    {
        perror("Problem listening\n");
        return ;
    }
    data_port_fd = ld;
}


void ftp_client::send_file(FILE *fp,int fd){
    if (fp == NULL)
        return;
    char buff[FILE_BUFFER];
    bzero(buff, FILE_BUFFER);
    long count = 0;
    while (!feof (fp)){
        count = fread (buff, sizeof (char), FILE_BUFFER, fp);
        send(fd,buff,count,0);
    }
    fclose(fp);
}

void ftp_client::receive_file(FILE *fp,int fd){
    char buff[FILE_BUFFER];
    bzero(buff, FILE_BUFFER);
    long count = 0;
    while((count = recv(fd,buff,FILE_BUFFER,0))){
        fwrite(buff,sizeof(char), count, fp);
    }
    fclose(fp);
}

void ftp_client::pasv_connect(){
    //connect to the data port
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(pasv_addr.port);
    server_addr.sin_addr.s_addr = htonl(pasv_addr.ip);
    data_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(data_fd == -1)
    {
        perror("socket error");
        return;
    }
    if(connect(data_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in))!=0){
        perror("connection error");
        return;
    }
}
