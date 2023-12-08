import socket
import re
import os
import sys

# 将输入信息处理后发送给服务器
def send_request(connect_socket, send_msg):
    connect_socket.send(send_msg.encode() + b"\r\n")

# 将服务器给的信息处理后返回，并打印出来
def recv_response(connect_socket):
    response = connect_socket.recv(8192).decode().rstrip('\r\n')
    print("From Server:", response)
    return response

# 被动模式下设置文件传输套接字
def set_pasv_addr(response):
    match = re.search(r'\((\d+,\d+,\d+,\d+,\d+,\d+)\)', response)
    if match:
        numbers = match.group(1).split(',')
        ip = ".".join(numbers[:4])
        ports = list(map(int, numbers[-2:]))
        port = ports[0]*256+ports[1]
        return (ip, port)
    else:
        return (0, 0)
    
# 主动模式下根据输入信息设置套接字
def set_port_addr(request):
    numbers = re.findall(r'\d+', request)
    ip = 0
    port = 0
    if (len(numbers) == 6):
        ip = ".".join(numbers[:4])
        port = int(numbers[4])*256+int(numbers[5])
    else:
        return (0, 0)
    return (ip, port)

def main():
    # 服务器的信息，默认为回环地址
    if len(sys.argv) < 3:
        server_host = "127.0.0.1"
        server_port = 21
    else:
        server_host = sys.argv[1]
        server_port = int(sys.argv[2])

    #server_host = "166.111.83.113"
    #server_port = 21

    # 客户端运行需要的一些基本信息
    root_path = "tmp/"
    file_path_retr = root_path
    file_path_stor = root_path
    data_socket = 0
    data_listen_socket = 0
    (data_ip, data_port) = (0, 0)
    os.makedirs(root_path, exist_ok=True)
    is_Login = False
    is_DataTrans = False
    data_Mode = False

    # 创建套接字并与服务器连接
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.connect((server_host, server_port))
    response = recv_response(server_socket)

    # 持续向服务器输入请求
    while True:
        request = input("From Client: ")

        # 处理不同指令
        if (is_Login and request.startswith("PORT")):
            # 主动模式得到识别，需创建监听套接字
            if (data_listen_socket != 0):
                data_listen_socket.close()
            if (data_socket != 0):
                data_socket.close()
            (data_ip, data_port) = set_port_addr(request)
            data_listen_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            data_listen_socket.bind((data_ip, data_port))
            data_listen_socket.listen(1)
            send_request(server_socket, request)
            response = recv_response(server_socket)
            if (response.startswith("200")):
                data_Mode = True
                is_DataTrans = True
        elif (is_Login and request.startswith("PASV")):
            # 被动模式得到识别，设置文件传输套接字response.startswith("227")
            if (data_socket != 0):
                data_socket.close()
            send_request(server_socket, request)
            response = recv_response(server_socket)
            if (response.startswith("227")):
                (data_ip, data_port) = set_pasv_addr(response)
                is_DataTrans = True
                data_Mode = False
        elif (is_Login and request.startswith("RETR") and is_DataTrans):
            # RETR指令得到识别，被动模式前要先连接上服务器，服务器才会响应
            send_request(server_socket, request)
            if not (data_Mode):
                data_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                data_socket.connect((data_ip, data_port))
            response = recv_response(server_socket)
            # 得到服务器的确认信息，主动模式等待服务器连接
            if (response.startswith("150")):
                if(data_Mode):
                    (data_socket, data_addr) = data_listen_socket.accept()
                file_data = b''
                temp = request.find(" ")
                file_path_retr += os.path.basename(request[temp+1:])
                # 若文件存在则重命名
                while (os.path.exists(file_path_retr)):
                    temp = file_path_retr.find(".")
                    file_path_retr = file_path_retr[:temp] + "_copy" + file_path_retr[temp:]
                # 若文件不存在则可以开始传输   
                with open(file_path_retr, 'wb') as file:
                    while True:
                        file_data = data_socket.recv(8192)
                        if not file_data:
                            break
                        file.write(file_data)
                    file.close()
                # 传输完毕，关闭服务器
                data_socket.close()
                data_socket = 0
                file_path_retr = root_path
                if(data_Mode):
                    data_listen_socket.close()
                    data_listen_socket = 0
                    data_Mode = False
                response = recv_response(server_socket)
                is_DataTrans = False
        elif (is_Login and request.startswith("STOR") and is_DataTrans):
            # STOR指令得到识别
            send_request(server_socket, request)
            if not (data_Mode):
                data_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                data_socket.connect((data_ip, data_port))
            response = recv_response(server_socket)
            # 得到服务器的确认信息，主动模式等待服务器连接
            if (response.startswith("150")):
                if(data_Mode):
                    (data_socket, data_addr) = data_listen_socket.accept()
                file_data = b''
                temp = request.find(" ")
                file_path_stor += request[temp+1:]
                # 文件是否存在应当由用户自行确认
                with open(file_path_stor, 'rb') as file:
                    while True:
                        file_data = file.read(8192)
                        if not file_data:
                            break
                        data_socket.send(file_data)
                    file.close()
                # 传输完毕
                data_socket.close()
                file_path_stor = root_path
                data_socket = 0
                if(data_Mode):
                    data_listen_socket.close()
                    data_listen_socket = 0
                    data_Mode = False
                # 接收传输完毕的信息
                response = recv_response(server_socket)
                is_DataTrans = False
        elif (is_Login and request.startswith("LIST") and is_DataTrans):
            # 列表指令
            send_request(server_socket, request)
            if not (data_Mode):
                data_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                data_socket.connect((data_ip, data_port))
            response = recv_response(server_socket)
            # 接收列表数据
            if (response.startswith("150")):
                if(data_Mode):
                    (data_socket, data_addr) = data_listen_socket.accept()
                print(data_socket.recv(data_port).decode())
                data_socket.close()
                data_socket = 0
                if(data_Mode):
                    data_listen_socket.close()
                    data_listen_socket = 0
                    data_Mode = False
                response = recv_response(server_socket)
                is_DataTrans = False
        elif (request.startswith("QUIT")):
            # 退出指令
            send_request(server_socket, request)
            # 关闭套接字 
            response = recv_response(server_socket)
            server_socket.close()
            break
        else:
            # 一般性指令，一问一答即可
            send_request(server_socket, request)
            response = recv_response(server_socket)
            if (response.startswith("230")):
                is_Login = True

if __name__ == "__main__":
    main()
