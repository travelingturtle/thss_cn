import socket
 
size = 8192
 
try:
  sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  for i in range(51):
    sock.sendto(str(i).encode(), ('localhost', 8192))
    resp = sock.recv(size)
    print(resp.decode())
  sock.close()
 
except:
  print ("cannot reach the server")