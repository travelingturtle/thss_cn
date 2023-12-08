#ifndef _FTPSERVER_H
#define _FTPSERVER_H

#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <ctype.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 8192

// 定义FTPServer结构体
struct FTPServer
{
	struct sockaddr_in _server_addr;                             // 服务器地址信息
	int _port;                                                   // 监听的端口号
    int _listenfd;                                               // 监听套接字
	int _connfd;                                                 // 连接套接字
	char _path[256];                                             // 根路径
};

// 定义FTPClient结构体
struct FTPClient
{
	int _connfd;                                                 // 连接套接字
	char _abs_root_path[256];
	char _root_path[256];                                        // 根路径
	char _cur_path[256];                                         // 根路径
	char _id[50];                                                // 用户账号
	char _password[50];                                          // 用户密码
	int _isLogin;                                                // 是否登录
	char _data_ip[50];                                           // 文件传输的ip
	int _data_port;                                              // 文件传输套接字
	int _data_listenfd;                                          // 文件传输监听套接字
	int _data_connfd;                                            // 文件传输连接套接字
	int _mode;                                                   // 主动模式(1)和被动模式(0)和未设置(-1)
	char _rnfr_path[256];                                        // 需要改名的文件夹
};

// 定义报文格式
struct FTPMsg
{
	char _cmd[5];                                                 // 指令
	char _info[BUFFER_SIZE];                                      // 指令信息
};

void setFTPServer(struct FTPServer* ftps);                       // 设置服务器
void runFTPServer(struct FTPServer* ftps);                       // 运行服务器
void* clientService(void* ftpc);                                 // 处理客户端请求
void setFTPClient(struct FTPServer* ftps, 
					struct FTPClient* ftpc);                     // 设置客户端初始数据
void sefFTPMsg(struct FTPMsg* msg);                              // 设置报文初始数据
int getClientMsg(struct FTPClient* ftpc, struct FTPMsg* req);    // 接收客户端请求信息并处理成字符串，返回字符串长度                  
int sendClientMsg(struct FTPClient* ftpc, struct FTPMsg* req);   // 将指定的字符串发送给客户端
int FTP_USER(struct FTPClient* ftpc, struct FTPMsg* req);        // 处理登录指令
int checkUser(char* username);                                   // 检查用户是否存在
int FTP_PASS(struct FTPClient* ftpc, struct FTPMsg* req);        // 处理密码指令
int checkPass(struct FTPClient* ftpc);                           // 检查密码是否正确
void FTP_ERR(struct FTPClient* ftpc);                            // 处理异常
void FTP_SYST(struct FTPClient* ftpc);                           // 处理SYST指令
void FTP_TYPE(struct FTPClient* ftpc, struct FTPMsg* req);       // 处理TYPE指令
void FTP_QUIT(struct FTPClient* ftpc);                           // 处理QUIT指令
void FTP_PORT(struct FTPClient* ftpc, struct FTPMsg* req);       // 处理PORT指令
void FTP_PASV(struct FTPClient* ftpc, struct FTPMsg* req);       // 处理PASV指令
int connectDataSocket(struct FTPClient* ftpc);                   // 连接文件传输套接字
void disconnectDataSocket(struct FTPClient* ftpc);               // 断开服务器套接字
void FTP_RETR(struct FTPClient* ftpc, struct FTPMsg* req);       // 处理RETR指令
void FTP_STOR(struct FTPClient* ftpc, struct FTPMsg* req);       // 处理STOR指令
void FTP_LIST(struct FTPClient* ftpc);                           // 处理LIST指令
void FTP_MKD(struct FTPClient* ftpc, struct FTPMsg* req);        // 处理MKD
void FTP_CWD(struct FTPClient* ftpc, struct FTPMsg* req);        // 处理CWD
void FTP_PWD(struct FTPClient* ftpc, struct FTPMsg* req);        // 处理PWD
void FTP_RMD(struct FTPClient* ftpc, struct FTPMsg* req);        // 处理RMD
void FTP_RNFR(struct FTPClient* ftpc, struct FTPMsg* req);       // 处理RNFR
void FTP_RNTO(struct FTPClient* ftpc, struct FTPMsg* req);       // 处理RNTO


#endif