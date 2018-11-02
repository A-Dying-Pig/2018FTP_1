//
//  FTP.h
//  FTP_server
//
//  Created by Poo Lei on 2018/10/26.
//  Copyright © 2018 Poo Lei. All rights reserved.
//

#ifndef FTP_h
#define FTP_h

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>

#define MAXBUF 1024*1024
#define COMMANDBUF 1024
#define LOCAL_HOST 0x7f000001
#define DEFAULT_IP 0x7f000001

#define MAX_CONCURRENT 10
#define MAX_QUEUE 5

#define TIMEOUT_SEC 5
#define TIMEOUT_USEC 0

#define MAX_WORDS_PER_LINE 128
#define MAX_REPLY_WORDS 512
#define MAX_REQUEST_WORDS 128
#define MAX_FILE_NAME 64
#define MAX_PATH_LENGTH 128

#define RES(code,msg) create_response(code,msg)
#define CAT(first,second) response_concat(first,second)
#define REPLY(fd,msg_ptr) server_reply(fd,msg_ptr)

#define Random() get_random_number()
#define P2FP(number) number_to_ftp_port(number)
#define FP2P(p1,p2) ftp_port_to_number(p1,p2)

struct FTP_Mode{
    //0 uninitialized,1 PORT MODE,2 PASV MODE
    int md;
    struct sockaddr_in addr;
};

struct relative_path{
    //end with "/"
    char current_path[MAX_PATH_LENGTH];
};

struct FTP_Request{
    char verb[MAX_REQUEST_WORDS];
    char param[MAX_REQUEST_WORDS];
};

struct FTP_Response{
    int code;
    char msg[MAX_WORDS_PER_LINE];
    struct FTP_Response *next;
};

struct FTP_server{
    int client_control_fds[MAX_CONCURRENT];
    int client_data_fds[MAX_CONCURRENT];
    int client_data_connection_fds[MAX_CONCURRENT];
    int close_data_fds[MAX_CONCURRENT];
    // -1 is not login, 0 is anonymous,positive number means user level
    int client_login[MAX_CONCURRENT];
    //-1 uninitialized,0 anonymous,positive number is the user id
    int client_ID[MAX_CONCURRENT];
    int data_port[MAX_CONCURRENT];
    struct FTP_Mode mode[MAX_CONCURRENT];
    int control_port_fd;
    struct timeval timeout;
    
    int upload_files[MAX_CONCURRENT];
    int download_files[MAX_CONCURRENT];
    int bytes[MAX_CONCURRENT];
    
    fd_set server_read_sets;
    fd_set server_write_sets;
    
    char file_buffer[MAXBUF];
    long file_count[MAX_CONCURRENT];
    //file system
    char root_path[MAX_PATH_LENGTH];    //end without '/'
    struct relative_path path[MAX_CONCURRENT];
    //rename
    //0 ; 1-REFR; 0- RETO
    int rename_state[MAX_CONCURRENT];
    struct relative_path rename_dir[MAX_CONCURRENT];
};
extern struct FTP_server ftp_server;

struct FTP_Port{
    int p1;
    int p2;
};

//not 0 = exit
int handlers(int select_ret);
void handler_new_connection(void);
void handler_user(int client_id);
void handler_login(int client_id);
void handler_anonymous_user(int client_id);
void handler_pasv_data_connection(int client_id);
//command handers
void handler_USER(int client_id,struct FTP_Request*req);
void handler_PASS(int client_id,struct FTP_Request*req);
void handler_PORT(int client_id,struct FTP_Request*req);
void handler_PASV(int client_id,struct FTP_Request*req);
void handler_SYST(int client_id,struct FTP_Request*req);
void handler_TYPE(int client_id,struct FTP_Request*req);
void handler_QUIT(int client_id,struct FTP_Request*req);
void handler_ABOR(int client_id,struct FTP_Request*req);
void handler_RETR(int client_id,struct FTP_Request*req);
void handler_STOR(int client_id,struct FTP_Request*req);
void handler_PWD(int client_id,struct FTP_Request*req);
void handler_MKD(int client_id,struct FTP_Request*req);
void handler_RMD(int client_id,struct FTP_Request*req);
void handler_RNFR(int client_id,struct FTP_Request*req);
void handler_RNTO(int client_id,struct FTP_Request*req);
void handler_CWD(int client_id,struct FTP_Request*req);
void handler_LIST(int client_id,struct FTP_Request*req);
//multi thread
void* send_file(void *param);
void* receive_file(void *param);
struct thread_params{
    FILE *fp;
    int client_id;
};

struct FTP_Request* read_request(int client_id);
void client_disconnection(int client_id);

void update_select_settings(int* max);
int server_start(void);
int create_TCP_socket(int queue,struct sockaddr_in addr);
extern struct sockaddr_in control_addr;
extern struct sockaddr_in data_addr;
int ftp_server_process(void);

//response
struct FTP_Response create_response(int cd,const char *message);
void response_concat(struct FTP_Response* first,struct FTP_Response* second);
int server_reply(int fd,struct FTP_Response * res);

//related to port
int get_random_number(void);
struct FTP_Port number_to_ftp_port(int pnumber);
int ftp_port_to_number(int p1,int p2);

//return 0 - success,not 0 - fail
int set_port_mode(int client_id,char * param);
int set_pasv_mode(int client_id);

//path
void set_root_path(char *rp);
void get_real_path(char *dest,char *input,int client_id);
//success return 0;fail return 1
int analyse_path_one(char * temp_path,char* cur_op); //cur-op without "/"

void port_connect(int client_id);
extern int local_ip;
extern int ftp_control_port;
extern char* rpath;
#endif /* FTP_h */
