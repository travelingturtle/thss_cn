import socket

size = 8192

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', 8192))
cnt = 0

try:
  while True:
    data, address = sock.recvfrom(size)
    resp = f"{cnt} {data.decode().upper()}"
    sock.sendto(resp.encode(), address)
    cnt += 1  # 增加序列号
finally:
  sock.close()