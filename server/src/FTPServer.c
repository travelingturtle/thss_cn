#include "FTPServer.h"

// 设置服务器套接字
void setFTPServer(struct FTPServer* ftps)
{
	// 初始化参数
	ftps->_connfd = 0;
	if (ftps->_path[strnlen(ftps->_path, 256) - 1] != '/')
	{
		strcat(ftps->_path, "/");
	}

	// 创建根目录
	if (access(ftps->_path, F_OK))
	{
		mkdir(ftps->_path, 0777);
	}

    // 创建socket
	if ((ftps->_listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		perror("Error opening socket");
		exit(EXIT_FAILURE);
	}

    // 设置套接字复用
    int reuse = 1;
    if (setsockopt(ftps->_listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
    {
        perror("Error setting socket-reuse");
        exit(EXIT_FAILURE);
    }

	// 设置本机的ip和port
	ftps->_server_addr.sin_family = AF_INET;
	ftps->_server_addr.sin_port = htons(ftps->_port);
	ftps->_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// 将本机的ip和port与socket绑定
	if (bind(ftps->_listenfd, (struct sockaddr*)(&(ftps->_server_addr)), sizeof(struct sockaddr)) == -1)
	{
		perror("Error binding");
        exit(EXIT_FAILURE);
	}

	// 开始监听socket（最大10个）
	if (listen(ftps->_listenfd, 10) == -1)
	{
		perror("Error listening");
        exit(EXIT_FAILURE);
	}
}

// 运行服务器
void runFTPServer(struct FTPServer* ftps)
{
	while (1) 
	{
		struct sockaddr_in temp_addr;
		socklen_t size = sizeof(struct sockaddr);
		// 等待client的连接 -- 阻塞函数
		if ((ftps->_connfd = accept(ftps->_listenfd, (struct sockaddr*) &temp_addr, &size)) == -1)
		{
			perror("Error accept");
			continue;
		}

		// 每有一个成功连接，创建一个线程来处理客户端请求
        pthread_t one_thread;
        if (pthread_create(&one_thread, NULL, clientService, (void*)ftps) != 0)
		{
            perror("Error creating thread");
            close(ftps->_connfd);
            continue;
        }
	}

	// 关闭监听端口，关闭服务器
	close(ftps->_listenfd);
}

// 接收客户端请求信息并处理成需要的指令格式
int getClientMsg(struct FTPClient* ftpc, struct FTPMsg* msg)
{
	// 清空消息窗口
	msg->_cmd[0] = '\0';
	char* buffer = (char*)malloc(BUFFER_SIZE*sizeof(char));
	int recv_size = 0;
	while(1) 
	{
        if (recv_size == BUFFER_SIZE) 
		{
            return -1;
        }
		int n = read(ftpc->_connfd, buffer + recv_size, BUFFER_SIZE - recv_size);
		if (n < 0) 
		{
			return -1;
		}
		else if (n == 0) 
		{
			break;
		}
		else 
		{
			recv_size += n;
            if (buffer[recv_size-2] == '\r' && buffer[recv_size-1] == '\n')
			{
                recv_size -= 2;
                break;
            }
		}
	}
	buffer[recv_size] = '\0';
	char *space_pos = strchr(buffer, ' ');
	if (space_pos != NULL)
	{
		int types = space_pos - buffer;
		strncpy(msg->_cmd, buffer, types);
		msg->_cmd[types] = '\0';
		strcpy(msg->_info, space_pos + 1);
	}
	else
	{
		strncpy(msg->_cmd, buffer, BUFFER_SIZE);
	}
	free(buffer);
	return recv_size;
}

// 将指定的字符串发送给客户端
int sendClientMsg(struct FTPClient* ftpc, struct FTPMsg* msg)
{
	char* buffer = (char*)malloc(BUFFER_SIZE*sizeof(char));
	buffer[0] = '\0';
	strncat(buffer, msg->_cmd, 5);
	strcat(buffer, " ");
	strncat(buffer, msg->_info, BUFFER_SIZE);
	strcat(buffer, "\r\n");
	int len = strlen(buffer), p = 0;
	while (p < len)
	{
		int n = write(ftpc->_connfd, buffer + p, len - p);
		if (n < 0)
		{
			return -1;
		} 
		else {
			p += n;
		}
	}
	if (p <= 0)
	{
		perror("Error sending");
		return 0;
	}
	free(buffer);
	return (p - 1);
}

// 设置客户端基本信息
void setFTPClient(struct FTPServer* ftps, struct FTPClient* ftpc)
{
	ftpc->_connfd = ftps->_connfd;
	ftpc->_id[0] = '\0';
	ftpc->_password[0] = '\0';
	ftpc->_isLogin = 0;
	ftpc->_data_ip[0] = '\0';
	ftpc->_data_listenfd = 0;
	ftpc->_data_connfd = 0;
	ftpc->_mode = -1;
	ftpc->_rnfr_path[0] = '\0';
	strncpy(ftpc->_root_path, ftps->_path, 256);
	strncpy(ftpc->_cur_path, ftps->_path, 256);
}

// 设置报文初始数据
void setFTPMsg(struct FTPMsg* msg)
{
	msg->_cmd[0] = '\0';
	msg->_info[0] = '\0';
}

// 处理客户端请求
void* clientService(void* arg)
{
	struct FTPServer* ftps = (struct FTPServer*) arg;

	// 创建一个客户端对象，记录客户端数据
	struct FTPClient ftpclient;
	struct FTPMsg req;
	struct FTPMsg res;
	struct FTPMsg* request = &req;
	struct FTPMsg* response = &res;
	struct FTPClient* ftpc = &ftpclient;
	setFTPClient(ftps, ftpc);
	setFTPMsg(request);
	setFTPMsg(response);

	// 首先发送友好的交流信息
	strcpy(response->_cmd, "220");
	strcpy(response->_info, "Anonymous FTP server ready.");
	sendClientMsg(ftpc, response);

	// 循环读取指令处理客户端请求
	while(1)
	{
		getClientMsg(ftpc, request);
		// 接收登录指令
		if (strncmp(request->_cmd, "USER", 5) == 0)
		{
			FTP_USER(ftpc, request);
			continue;
		}
		else
		{
			// 未登录且不是要输入密码则发送提示信息
			if ((strncmp(request->_cmd, "PASS", 5) != 0) && (!ftpc->_isLogin))
			{
				strcpy(response->_cmd, "530");
				strcpy(response->_info, "Not logged in.");
				sendClientMsg(ftpc, response);
				continue;
			}
		}

		// 确认登录后才能进行其他指令的发送
		if (strncmp(request->_cmd, "PASS", 5) == 0)
		{
			// 处理PASS指令
			FTP_PASS(ftpc, request);
		}
		else if (strncmp(request->_cmd, "SYST", 5) == 0)
		{
			// 处理SYST指令
			FTP_SYST(ftpc);
		}
		else if (strncmp(request->_cmd, "TYPE", 5) == 0)
		{
			// 处理TYPE指令
			FTP_TYPE(ftpc, request);	
		}
		else if (strncmp(request->_cmd, "PORT", 5) == 0)
		{
			// 处理PORT指令
			FTP_PORT(ftpc, request);
		}
		else if ((strncmp(request->_cmd, "PASV", 5) == 0))
		{
			// 处理PASV指令
			FTP_PASV(ftpc, request);
		}
		else if (strncmp(request->_cmd, "RETR", 5) == 0)
		{
			// 处理RETR指令
			FTP_RETR(ftpc, request);
		}
		else if (strncmp(request->_cmd, "STOR", 5) == 0)
		{
			// 处理STOR指令
			FTP_STOR(ftpc, request);
		}
		else if (strncmp(request->_cmd, "LIST", 5) == 0)
		{
			// 处理LIST指令
			FTP_LIST(ftpc);
		}
		else if (strncmp(request->_cmd, "MKD", 4) == 0)
		{
			// 处理MKD指令
			FTP_MKD(ftpc, request);
		}
		else if (strncmp(request->_cmd, "CWD", 4) == 0)
		{
			// 处理CWD指令
			FTP_CWD(ftpc, request);
		}
		else if (strncmp(request->_cmd, "PWD", 4) == 0)
		{
			// 处理PWD指令
			FTP_PWD(ftpc, request);
		}
		else if (strncmp(request->_cmd, "RMD", 4) == 0)
		{
			// 处理PWD指令
			FTP_RMD(ftpc, request);
		}
		else if (strncmp(request->_cmd, "RNFR", 5) == 0)
		{
			// 处理PWD指令
			FTP_RNFR(ftpc, request);
		}
		else if (strncmp(request->_cmd, "RNTO", 5) == 0)
		{
			// 处理PWD指令
			FTP_RNTO(ftpc, request);
		}
		else if (strncmp(request->_cmd, "QUIT", 5) == 0 || strncmp(request->_cmd, "ABOR", 5) == 0)
		{
			// 发送信息后关闭连接
			FTP_QUIT(ftpc);
			break;
		}
		else
		{
			// 未知命令
			FTP_ERR(ftpc);
		}
	}

	// 处理完毕，关闭套接字，释放动态内存
	close(ftpc->_connfd);
	return 0;
}

// 检查用户是否存在
int checkUser(char* username)
{
	if (!strcmp(username, "anonymous"))
	{
		return 1;
	}
	return 0;
}

// 处理登录指令
int FTP_USER(struct FTPClient* ftpc, struct FTPMsg* req)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);
	strcpy(ftpc->_id, req->_info);
	int isUser = checkUser(req->_info);
	if (isUser)
	{
		strcpy(response->_cmd, "331");
		strcpy(response->_info, "Guest login ok, send your complete e-mail address as password.");
	}
	else
	{
		strcpy(response->_cmd, "530");
		strcpy(response->_info, "User not found.");		
	}
	sendClientMsg(ftpc, response);
	free(response);
	return isUser;
}

// 检查密码是否正确
int checkPass(struct FTPClient* ftpc)
{
	if (strncmp(ftpc->_id, "Anonymous", 9))
		return 1;
	return 0;
}

// 处理密码指令
int FTP_PASS(struct FTPClient* ftpc, struct FTPMsg* req)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);
	strncpy(ftpc->_password, req->_info, BUFFER_SIZE);
	int isPass = checkPass(ftpc);
	if (isPass)
	{
		ftpc->_isLogin = 1;
		strcpy(response->_cmd, "230");
		strcpy(response->_info, "Guest login ok, access restrictions apply.");
	}
	else
	{
		strcpy(response->_cmd, "530");
		strcpy(response->_info, "Password error.");		
	}
	sendClientMsg(ftpc, response);
	free(response);
	return isPass;
}

// 处理异常
void FTP_ERR(struct FTPClient* ftpc)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);
	strcpy(response->_cmd, "500");
	strcpy(response->_info, "Unknown command.");
	sendClientMsg(ftpc, response);
	free(response);
	return;
}

// 处理SYST
void FTP_SYST(struct FTPClient* ftpc)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);
	strcpy(response->_cmd, "215");
	strcpy(response->_info, "UNIX Type: L8");
	sendClientMsg(ftpc, response);
	free(response);
}

// 处理TYPE
void FTP_TYPE(struct FTPClient* ftpc, struct FTPMsg* req)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);
	strcpy(response->_cmd, "200");
	strcpy(response->_info, "Type set to ");
	strncat(response->_info, req->_info, BUFFER_SIZE);
	strcat(response->_info, ".");
	sendClientMsg(ftpc, response);
	free(response);
}

// 处理QUIT
void FTP_QUIT(struct FTPClient* ftpc)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);
	strcpy(response->_cmd, "221");
	strcpy(response->_info, "Goodbye~");
	sendClientMsg(ftpc, response);
	free(response);
}

// 处理PORT
void FTP_PORT(struct FTPClient* ftpc, struct FTPMsg* req)
{
	// 关闭已经存在的套接字
	if (ftpc->_data_connfd != 0)
		close(ftpc->_data_connfd);
	if (ftpc->_data_listenfd != 0)
		close(ftpc->_data_listenfd);
	
	// 读取ip和端口数据并记录
	int ip[4], port[2];
	sscanf(req->_info, "%d,%d,%d,%d,%d,%d", ip, ip+1, ip+2, ip+3, port, port+1);
	sprintf(ftpc->_data_ip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	ftpc->_data_port = port[0]*256+port[1];

	// 设置回复信息
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);
	strcpy(response->_cmd, "200");
	strcpy(response->_info, "Port command succeddful.");
	sendClientMsg(ftpc, response);
	ftpc->_mode = 1;
	free(response);
}

// 处理PASV
void FTP_PASV(struct FTPClient* ftpc, struct FTPMsg* req)
{
	// 关闭已经存在的套接字
	if (ftpc->_data_connfd != 0)
		close(ftpc->_data_connfd);
	if (ftpc->_data_listenfd != 0)
		close(ftpc->_data_listenfd);

	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);

	// 创建新的套接字（由于此时是由服务器提供端口，因此需要先创建好等待客户端来连）
	if ((ftpc->_data_listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		perror("Error opening socket");
		strcpy(response->_cmd, "426");
		strcpy(response->_info, "Pasv command fail.");
		sendClientMsg(ftpc, response);
		free(response);
		return;
	}

    // 设置套接字复用
    int reuse = 1;
    if (setsockopt(ftpc->_data_listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
    {
        perror("Error setting socket-reuse");
		free(response);
        return;
    }

	// 获取服务器的局域网ip
	struct ifaddrs *tmpaddrs;
	getifaddrs(&tmpaddrs);
	while (tmpaddrs)
	{
		if (tmpaddrs->ifa_addr && tmpaddrs->ifa_addr->sa_family == AF_INET)
		{
			struct sockaddr_in *pAddr = (struct sockaddr_in *)tmpaddrs->ifa_addr;
			strncpy(ftpc->_data_ip, inet_ntoa(pAddr->sin_addr), 32);
		}
		tmpaddrs = tmpaddrs->ifa_next;
	}
	freeifaddrs(tmpaddrs);

	// 获取随机端口
	int random_port[2];
	random_port[0] = (rand()%176)+79;
	random_port[1] = rand()%255;

	// 设置ip和port
	struct sockaddr_in data_server_addr;
	data_server_addr.sin_family = AF_INET;
	data_server_addr.sin_port = htons(random_port[0]*256+random_port[1]);
	data_server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	//data_server_addr.sin_addr.s_addr = inet_addr(ftpc->_data_ip);

	// 将取得的ip和port与socket绑定
	if (bind(ftpc->_data_listenfd, (struct sockaddr*)(&(data_server_addr)), sizeof(struct sockaddr)) == -1)
	{
		strcpy(response->_cmd, "426");
		strcpy(response->_info, "Pasv command fail.");
		sendClientMsg(ftpc, response);
		free(response);
		return;
	}

	// 开始监听socket
	if (listen(ftpc->_data_listenfd, 10) == -1)
	{
		strcpy(response->_cmd, "426");
		strcpy(response->_info, "Pasv command fail.");
		sendClientMsg(ftpc, response);
		free(response);
		return;
	}

	// 创建完成，可以返回响应信息了
	strcpy(response->_cmd, "227");
	char res_msg[100] = "Entering Passive Mode (\0";
	int temp_ip[4];
	char res_addr[50];
	sscanf(ftpc->_data_ip, "%d.%d.%d.%d", temp_ip, temp_ip+1, temp_ip+2, temp_ip+3);
	sprintf(res_addr, "%d,%d,%d,%d,", temp_ip[0], temp_ip[1], temp_ip[2], temp_ip[3]);
	strncat(res_msg, res_addr, 50);	
	sprintf(res_addr, "%d,%d", random_port[0], random_port[1]);
	strncat(res_msg, res_addr, 50);
	strcat(res_msg, ")");
	strncpy(response->_info, res_msg, BUFFER_SIZE);
	sendClientMsg(ftpc, response);
	ftpc->_mode = 0;
	free(response);
}

// 连接服务器套接字
int connectDataSocket(struct FTPClient* ftpc)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);
	
	if (ftpc->_mode == 1)
	{
		// 主动模式，服务器需创建并连接socket
		// 发送确认信息后再尝试连接
		strcpy(response->_cmd, "150");
		strcpy(response->_info, "Data connection accepted; transfer starting.");
		sendClientMsg(ftpc, response);

		struct sockaddr_in data_addr;
		if ((ftpc->_data_connfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
		{
			perror("Error opening socket");
			free(response);
			return 1;
		}
		int reuse = 1;
		if (setsockopt(ftpc->_data_connfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
		{
			perror("Error setting socket-reuse");
			free(response);
			return 1;
		}
		data_addr.sin_family = AF_INET;
		data_addr.sin_port = htons(ftpc->_data_port);
		inet_pton(AF_INET, ftpc->_data_ip, &(data_addr.sin_addr));

		if (connect(ftpc->_data_connfd, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0 )
		{
			perror("Error connecting");
			close(ftpc->_data_connfd);
			free(response);
			return 1;
    	}
	}
	else
	{
		// 被动模式，等待客户端连接即可
		struct sockaddr_in temp_addr;
		socklen_t size = sizeof(struct sockaddr);
		// 等待client的连接 -- 阻塞函数
		if ((ftpc->_data_connfd = accept(ftpc->_data_listenfd, (struct sockaddr*) &temp_addr, &size)) == -1)
		{
			perror("Error accept");
			free(response);
			return 1;
		}
		strcpy(response->_cmd, "150");
		strcpy(response->_info, "Data connection accepted; transfer starting.");
		sendClientMsg(ftpc, response);
	}

	free(response);
	return 0;
}

// 断开服务器套接字
void disconnectDataSocket(struct FTPClient* ftpc)
{
	if (ftpc->_data_connfd)
	{
		close(ftpc->_data_connfd);
	}
	if ((ftpc->_mode == -1) && (ftpc->_data_listenfd))
	{
		close(ftpc->_data_listenfd);
	}
}

// 处理RETR
void FTP_RETR(struct FTPClient* ftpc, struct FTPMsg* req)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);

	if ((ftpc->_mode) == -1 || !strncmp(response->_info, "../", 3))
	{
		strcpy(response->_cmd, "503");
		strcpy(response->_info, "Bad sequence of commands.");
		sendClientMsg(ftpc, response);
		free(response);
		return;
	}

	// 创建数据传输socket
	if (connectDataSocket(ftpc))
	{
		strcpy(response->_cmd, "425");
		strcpy(response->_info, "Failed to create a connection.");
		sendClientMsg(ftpc, response);
		free(response);
		return;
	}

	// 设置文件路径并检查文件
	char path[256] = "";
	strncpy(path, ftpc->_cur_path, 256);
	strncat(path, req->_info, 256);
	struct stat file_info;
    if ((stat(path, &file_info) == 0) && S_ISREG(file_info.st_mode)) 
	{
        // 存在文件，可以进行传输
		FILE* file = NULL;
		file = fopen(path, "rb");
		if (file == NULL)
		{
			strcpy(response->_cmd, "451");
			strcpy(response->_info, "Failed to open the file.");
		}
		else
		{
			char buffer[BUFFER_SIZE];
			while (!feof(file))
			{
				int n = fread(buffer, 1, BUFFER_SIZE, file);
				int i = 0;
				while (i < n) 
				{
					i += send(ftpc->_data_connfd, buffer + i, n - i, 0);
				}
			}
			fclose(file);
			disconnectDataSocket(ftpc);
			strcpy(response->_cmd, "226");
			strcpy(response->_info, "File sent ok.");
			ftpc->_mode = -1;
			sendClientMsg(ftpc, response);
			free(response);
			return;
		}
	}
	else 
	{
		close(ftpc->_data_connfd);
        strcpy(response->_cmd, "451");
		strcpy(response->_info, "Failed to get the file.");
    }
	disconnectDataSocket(ftpc);
	sendClientMsg(ftpc, response);
	free(response);
	return;
}

// 处理STOR
void FTP_STOR(struct FTPClient* ftpc, struct FTPMsg* req)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);

	if (ftpc->_mode == -1 || !strncmp(response->_info, "../", 3))
	{
		strcpy(response->_cmd, "503");
		strcpy(response->_info, "Bad sequence of commands.");
		sendClientMsg(ftpc, response);
		free(response);
		return;
	}

	// 创建数据传输socket
	if (connectDataSocket(ftpc))
	{
		strcpy(response->_cmd, "425");
		strcpy(response->_info, "Failed to create a connection.");
		sendClientMsg(ftpc, response);
		free(response);
		return;
	}

	// 设置文件路径并检查文件
	char path[256] = "";
	strncpy(path, ftpc->_cur_path, 256);
	char* file_name = strrchr(req->_info, '/');
    if (file_name != NULL)
	{
		strncat(path, file_name, 256);
	}
	else
	{
		strncat(path, req->_info, 256);
	}
	struct stat file_info;
    if (stat(path, &file_info) == 0) 
	{
        close(ftpc->_data_connfd);
        strcpy(response->_cmd, "550");
		strcpy(response->_info, "File already exists.");
	}
	else 
	{
		// 不存在文件，可以进行传输
		FILE* file = NULL;
		file = fopen(path, "wb");
		if (file == NULL)
		{
			strcpy(response->_cmd, "451");
			strcpy(response->_info, "Failed to create the file.");
		}
		else
		{
			char buffer[BUFFER_SIZE];
			int i = 0;
			while ((i = recv(ftpc->_data_connfd, buffer, BUFFER_SIZE, 0)) > 0)
			{
				fwrite(buffer, 1, i, file);
			}
			fclose(file);
			disconnectDataSocket(ftpc);
			strcpy(response->_cmd, "226");
			strcpy(response->_info, "File received ok.");
			ftpc->_mode = -1;
			sendClientMsg(ftpc, response);
			free(response);
			return;
		}
    }
	disconnectDataSocket(ftpc);
	sendClientMsg(ftpc, response);
	free(response);
	return;
}

// 处理LIST
void FTP_LIST(struct FTPClient* ftpc)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);

	if (ftpc->_mode == -1)
	{
		strcpy(response->_cmd, "503");
		strcpy(response->_info, "Bad sequence of commands.");
		sendClientMsg(ftpc, response);
		free(response);
		return;
	}

	// 创建数据传输socket
	if (connectDataSocket(ftpc))
	{
		strcpy(response->_cmd, "425");
		strcpy(response->_info, "Failed to create a connection.");
		sendClientMsg(ftpc, response);
		free(response);
		return;
	}

	// 获取当前位置的目录信息
	struct dirent *entry;
    struct stat file_stat;
    DIR *dir = opendir(ftpc->_cur_path);

    while ((entry = readdir(dir)))
	{
        char full_path[BUFFER_SIZE];
        snprintf(full_path, BUFFER_SIZE, "%s/%s", ftpc->_cur_path, entry->d_name);

		stat(full_path, &file_stat);
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char permissions[11];
		permissions[0] = S_ISDIR(file_stat.st_mode) ? 'd' : '-';
		permissions[1] = (file_stat.st_mode & S_IRUSR) ? 'r' : '-';
		permissions[2] = (file_stat.st_mode & S_IWUSR) ? 'w' : '-';
		permissions[3] = (file_stat.st_mode & S_IXUSR) ? 'x' : '-';
		permissions[4] = (file_stat.st_mode & S_IRGRP) ? 'r' : '-';
		permissions[5] = (file_stat.st_mode & S_IWGRP) ? 'w' : '-';
		permissions[6] = (file_stat.st_mode & S_IXGRP) ? 'x' : '-';
		permissions[7] = (file_stat.st_mode & S_IROTH) ? 'r' : '-';
		permissions[8] = (file_stat.st_mode & S_IWOTH) ? 'w' : '-';
		permissions[9] = (file_stat.st_mode & S_IXOTH) ? 'x' : '-';
		permissions[10] = '\0';

		struct passwd *owner = getpwuid(file_stat.st_uid);
		struct group *group = getgrgid(file_stat.st_gid);
		char date[80];
		strftime(date, sizeof(date), "%b %d %Y %H:%M", localtime(&(file_stat.st_mtime)));
		char buffer[BUFFER_SIZE];
		sprintf(buffer, "%s %2d %s %s %6ld %s %s\r\n",
			permissions, (int)file_stat.st_nlink, owner->pw_name, group->gr_name,
			(long)file_stat.st_size, date, entry->d_name);
		send(ftpc->_data_connfd, buffer, strnlen(buffer, BUFFER_SIZE), 0);
    }

    closedir(dir);

	disconnectDataSocket(ftpc);
	strcpy(response->_cmd, "226");
	strcpy(response->_info, "Transfer ok");
	ftpc->_mode = -1;
	disconnectDataSocket(ftpc);
	sendClientMsg(ftpc, response);
	free(response);
	return;
}

// 处理MKD
void FTP_MKD(struct FTPClient* ftpc, struct FTPMsg* req)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);

	// 设置文件夹路径
	char path[256] = "";
	strncpy(path, ftpc->_cur_path, 256);
	strncat(path, req->_info, 256);
	strcat(path, "/");

	// 创建文件夹
	int is_mkdir = mkdir(path, S_IRWXU | S_IRGRP | S_IXOTH);
	if (!is_mkdir)
	{
		strcpy(response->_cmd, "257");
		strcpy(response->_info, "Directory created successfully.");
	}
	else
	{
		if (errno == EEXIST)
		{
			strcpy(response->_cmd, "550");
			strcpy(response->_info, "Directory or file already exists.");
		}
		else
		{
			strcpy(response->_cmd, "553");
			strcpy(response->_info, "Failed to create the directory.");
		}
	}
	sendClientMsg(ftpc, response);
	free(response);
}

// 处理CWD
void FTP_CWD(struct FTPClient* ftpc, struct FTPMsg* req)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);

	// 创建路径
	char path[256] = "";
	strncpy(path, ftpc->_cur_path, 256);
	if (req->_info[0] == '/')
		path[strnlen(path, 256)-1] = '\0';
	if (req->_info[strnlen(req->_info, 256)-1] != '/')
		strcat(req->_info, "/");
	strncat(path, req->_info, 256);

	// 查找文件夹是否存在
	struct stat info;
	if (stat(path, &info) != 0)
	{
		strcpy(response->_cmd, "550");
		strcpy(response->_info, "Directory not found.");
	}
	else
	{
		strncpy(ftpc->_cur_path, path, 256);
		strcpy(response->_cmd, "250");
		strcpy(response->_info, "Okay. Current path: ");
		strcat(response->_info, path);
	}

	sendClientMsg(ftpc, response);
	free(response);
}

// 处理PWD
void FTP_PWD(struct FTPClient* ftpc, struct FTPMsg* req)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);

	strcpy(response->_cmd, "257");
	strcpy(response->_info, ftpc->_cur_path);

	sendClientMsg(ftpc, response);
	free(response);
}

// 处理RMD
void FTP_RMD(struct FTPClient* ftpc, struct FTPMsg* req)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);

	// 创建路径
	char path[256] = "";
	strncpy(path, ftpc->_cur_path, 256);
	if (req->_info[0] == '/')
		path[strnlen(path, 256)-1] = '\0';
	if (req->_info[strnlen(req->_info, 256)-1] != '/')
		strcat(path, "/");
	strncat(path, req->_info, 256);

	// 删除文件夹
	int is_rmdir = rmdir(path);
	if (!is_rmdir)
	{
		strcpy(response->_cmd, "250");
		strcpy(response->_info, "Directory removed successfully.");
	}
	else
	{
		strcpy(response->_cmd, "550");
		strcpy(response->_info, "Invalid directory name.");
	}

	sendClientMsg(ftpc, response);
	free(response);
}

// 处理RNFR
void FTP_RNFR(struct FTPClient* ftpc, struct FTPMsg* req)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);

	// 创建路径
	char path[256] = "";
	strncpy(path, ftpc->_cur_path, 256);
	if (req->_info[0] == '/')
		path[strnlen(path, 256)-1] = '\0';
	if (req->_info[strnlen(req->_info, 256)] == '/')
		req->_info[strnlen(req->_info, 256)] = '\0';
	strncat(path, req->_info, 256);

	// 查找文件或文件夹是否存在
	struct stat info;
	if (stat(path, &info) != 0)
	{
		strcpy(response->_cmd, "550");
		strcpy(response->_info, "Directory or file not found.");
	}
	else
	{
		strncpy(ftpc->_rnfr_path, path, 256);
		strcpy(response->_cmd, "350");
		strcpy(response->_info, "Directory or file exists, ready for destination name.");
	}

	sendClientMsg(ftpc, response);
	free(response);
}

// 处理RNTO
void FTP_RNTO(struct FTPClient* ftpc, struct FTPMsg* req)
{
	struct FTPMsg* response = (struct FTPMsg*) malloc(sizeof(struct FTPMsg));
	setFTPMsg(response);

	// 创建路径
	char path_to[256] = "";
	if (req->_info[strnlen(req->_info, 256)] == '/')
		req->_info[strnlen(req->_info, 256)] = '\0';

	// 输入可能是名或地址，因此先获取名
	char *filename = strrchr(req->_info, '/');
	if (filename == NULL)     // 输入的是名，可以修改
	{
		char *filename_fr = strrchr(ftpc->_rnfr_path, '/');
		strncpy(path_to, ftpc->_rnfr_path, filename_fr-ftpc->_rnfr_path);
		path_to[filename_fr-ftpc->_rnfr_path]='/';
		path_to[filename_fr-ftpc->_rnfr_path+1]='\0';
		strncat(path_to, req->_info, 256);
	}
	else                      // 输入的是地址
	{
		char *filename_fr = strrchr(ftpc->_rnfr_path, '/');
		if (strncmp(req->_info, ftpc->_rnfr_path, filename_fr-ftpc->_rnfr_path))
		{
			// 新地址与原地址对不上，不修改
			strcpy(response->_cmd, "550");
			strcpy(response->_info, "Invalid directory name.");
			sendClientMsg(ftpc, response);
			free(response);
			return;
		}
		else
		{
			// 新地址正确，可以修改
			int types = filename_fr-ftpc->_rnfr_path;
			strncpy(path_to, ftpc->_rnfr_path, types);
			path_to[types]='/';
			path_to[types+1]='\0';
			strncat(path_to, filename, 256);
		}
	}

	// 查找文件或文件夹是否存在
	struct stat info;
	if (stat(path_to, &info) != 0)
	{
		if (rename(ftpc->_rnfr_path, path_to) == 0)
		{
        	strcpy(response->_cmd, "250");
			strcpy(response->_info, "Directory or file renamed successfully.");
			ftpc->_rnfr_path[0] = '\0';
		}
		else
		{
			strcpy(response->_cmd, "550");
			strcpy(response->_info, "Invalid directory or file name.");
		}
	}
	else
	{
		strcpy(response->_cmd, "553");
		strcpy(response->_info, "Directory or file name already exists.");
	}

	sendClientMsg(ftpc, response);
	free(response);
}
