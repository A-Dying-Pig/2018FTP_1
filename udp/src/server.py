import socket

size = 8192

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', 9876))

count = 0

try:
    while True:
        data, address = sock.recvfrom(size)
        data = '%d %s'%(count,data)
        count += 1
        print "Return Data is: %s"%data
        sock.sendto(data, address)
finally:
    sock.close()