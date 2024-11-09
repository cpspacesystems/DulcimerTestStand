import socket
import struct
import time
import random

# Constants
LOCAL_IP = "127.0.0.1"
LOCAL_PORT = 65432
REMOTE_IP = "127.0.0.1"
REMOTE_PORT = 65433

# Dataframe structures
class SendDataFrame:
    def __init__(self):
        self.analogValues = [0] * 18
        self.load_cells = [0.0] * 3
        self.thermocouples = [0.0] * 2
        self.timestamp = 0

class ReceiveDataFrame:
    def __init__(self):
        self.relay_states = [0] * 16

# Initialize UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((LOCAL_IP, LOCAL_PORT))

sock.setblocking(0)
sock.settimeout(0)

# Capture the start time
program_start_time = time.time()

def read_analog_values(df):
    for i in range(18):
        df.analogValues[i] = random.randint(0, 1023)

def read_load_cells(df):
    df.load_cells = [random.uniform(0, 100) for _ in range(3)]

def read_thermocouples(df):
    df.thermocouples = [random.uniform(0, 100) for _ in range(2)]

def send_reply_packet():
    df = SendDataFrame()
    read_analog_values(df)
    # Calculate elapsed time in milliseconds since program start
    df.timestamp = int((time.time() - program_start_time) * 1e3)
    read_load_cells(df)
    read_thermocouples(df)
    
    # Debugging prints
    #print(f"Analog Values: {df.analogValues}")
    #print(f"Load Cells: {df.load_cells}")
    #print(f"Thermocouples: {df.thermocouples}")
    #print(f"Timestamp: {df.timestamp}")
    
    # Check ranges
    for value in df.analogValues:
        if not (0 <= value <= 65535):
            raise ValueError(f"Analog value {value} out of range")
    
    # Pack data into bytes
    data = struct.pack('18H3f2fI', *df.analogValues, *df.load_cells, *df.thermocouples, df.timestamp)
    
    # Send packet
    sock.sendto(data, (REMOTE_IP, REMOTE_PORT))

def receive_command_packet():
    try:
        data, addr = sock.recvfrom(1024)
        print(data)
        if data:
            if len(data) != 16:
                print("Error: Packet size mismatch")
                return
            
            r_df = ReceiveDataFrame()
            r_df.relay_states = list(struct.unpack('16B', data))
            
            # Print relay states
            relay_states_str = ", ".join(["ON" if state else "OFF" for state in r_df.relay_states])
            print(f"Relay States: {relay_states_str}")
    except BlockingIOError:
        pass
    except Exception as e:
        print(f"Error receiving data packet: {e}")

def main():
    while True:
        send_reply_packet()
        receive_command_packet()

if __name__ == "__main__":
    main()