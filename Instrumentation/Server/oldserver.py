import socket
import struct
import time

# Set up the socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('0.0.0.0', 65432))  # IP of this PC

dataframe_format = '18H'  # 18 unsigned shorts (H), https://docs.python.org/3/library/struct.html

packet_count = 0
start_time = time.time()

while True:
    print("loop")
    data, addr = sock.recvfrom(36)  # Arbitrarily selected buffer size
    if data:
        df = struct.unpack(dataframe_format, data)
        print(df)
        packet_count += 1

    # Print packet rate for performance profiling every 1s
    if time.time() - start_time > 1:
        packet_rate = packet_count / (time.time() - start_time)
        print(f"Rate: {packet_rate} s/sec")
        packet_count = 0
        start_time = time.time()