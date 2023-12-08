#include "FTPServer.h"

int main(int argc, char**argv)
{
    struct FTPServer *server = (struct FTPServer*) malloc(sizeof(struct FTPServer));
    // 接收输入参数
    if(argc >= 2)
    {
        server->_port = atoi(argv[2]);
    }
    else
    {
    	server->_port = 21;
    }
    if(argc < 3)
    {
    	strcpy(server->_path, "/tmp");
    }
    else
    {
    	strcpy(server->_path, argv[4]);
    }
    setFTPServer(server);
    runFTPServer(server);
    free(server);
    return 0;
}
