# credit to https://github.com/mCodingLLC/VideosSampleCode/blob/master/videos/092_pythonizing_imgui/demo.py for the imgui boilerplate

import socket
import struct
import datetime
import time
import uuid
import random
import os

import numpy as np
import array
import threading
import pandas as pd

import glfw
import OpenGL.GL as gl
import imgui
from imgui import plot as implot
from imgui.integrations.glfw import GlfwRenderer

if not os.path.exists('data'):
    os.makedirs('data')

dataframe_indexer = 0

network_socket = None
receive_dataframe_format = '18H5f' # https://docs.python.org/3/library/struct.html

previous_valve_states = None

packet_rate_time = None
packet_receive_count = 0
packet_receive_rate = -1

last_sensor_update = -1
timestamped_float = [('timestamp', 'datetime64[ns]'),
                     ('value', 'float')]  # custom type with timestamp and pressure reading
# Label, (Type, Dataframe Index, Previous_Values (array), minimum_val, maximum_val)
sensor_set = {
    "Nitrous Press Regulator": ("Pressure Transducer", 2, np.array([], dtype=timestamped_float), 0, 1500),
    "Ethanol Press Regulator": ("Pressure Transducer", 3, np.array([], dtype=timestamped_float), 0, 1500),
    #"Nitrous Tank": ("Pressure Transducer", -1, np.array([], dtype=timestamped_float), 0, 1000),
    #"Ethanol Tank": ("Pressure Transducer", -1, np.array([], dtype=timestamped_float), 0, 1000),
    "Nitrous Injector": ("Pressure Transducer", 0, np.array([], dtype=timestamped_float), 0, 1000),
    "Ethanol Injector": ("Pressure Transducer", 1, np.array([], dtype=timestamped_float), 0, 1000),
    "Combustion Chamber": ("Pressure Transducer", 6, np.array([], dtype=timestamped_float), 0, 1000),
    "Fuel Flow": ("Flow Meter", 7, np.array([], dtype=timestamped_float), 0, 1024),

    "Engine Loadcell #1": ("Loadcell", 18, np.array([], dtype=timestamped_float), 0, 500),
    "Engine Loadcell #2": ("Loadcell", 19, np.array([], dtype=timestamped_float), 0, 500),
    "Engine Loadcell #3": ("Loadcell", 20, np.array([], dtype=timestamped_float), 0, 500),

    "Engine Thermocouple": ("Thermocouple", 21, np.array([], dtype=timestamped_float), 0, 100),
    "Nitrous Thermocouple": ("Thermocouple", 22, np.array([], dtype=timestamped_float), 0, 100),
}

# eventually, set the numbers to which pin on the relay board they correspond to
valve_lockout = True
valve_set = {  # Label, (Dataframe Index, Relay Pin #, Enabled)
    "Nitrous Run": (-1, 12, False), # updated
    "Ethanol Run": (-1, 13, False), # updated
    "Nitrous Press": (-1, 16, False), # updated
    "Ethanol Press": (-1, 14, False), # updated
    "Purge": (-1, 1, False), # updated
    "Nitrous Fill": (-1, 15, False), # updated
    "Nitrous Vent": (-1, 4, False), # updated
    "Nitrous Dump": (-1, 3, False), # updated
    "Ethanol Dump": (-1, 2, False), # updated
}

send_valve_packets = False

for label, (df_index, relay_pin, enabled) in valve_set.items():
    valve_set[label] = (dataframe_indexer, relay_pin, enabled)
    dataframe_indexer += 1

active = {
    "Valve Controls": True,
    "Sensor Readouts": True,
    "Save Data": False,
    "Post to Web": False,
}

class ColdFlowRoutine:
    def __init__(self):
        self.state = 0
        self.start_time = None

    def run(self):
        if self.state == 0:
            (df_index, pin, enabled) = valve_set["Nitrous Run"]
            valve_set["Nitrous Run"] = (df_index, pin, True)

            (df_index, pin, enabled) = valve_set["Ethanol Run"]
            valve_set["Ethanol Run"] = (df_index, pin, True)

            self.start_time = time.time()
            self.state = 1
        elif self.state == 1:
            if time.time() - self.start_time >= 4:
                (df_index, pin, enabled) = valve_set["Nitrous Run"]
                valve_set["Nitrous Run"] = (df_index, pin, False)

                (df_index, pin, enabled) = valve_set["Ethanol Run"]
                valve_set["Ethanol Run"] = (df_index, pin, False)

                self.state = 2
        # Add more states for other valves
        else:
            self.state = 0  # Reset the state machine

cold_flow_routine = ColdFlowRoutine()


def ingest_data(df):
    current_time = np.datetime64(datetime.datetime.now())
    #       print(current_time)
    new_records = []
    for idx, new_value in enumerate(df):
        for label, (sensor_type, df_index, previous_values, min_val, max_val) in sensor_set.items():
            if idx == df_index:
                if sensor_type == "Pressure Transducer":
                    new_value = new_value / 1024  # scale it 0-1, teensy outputs 0-1024
                    new_value = new_value / (2 / 3.3)  # the pts run through a 5x voltage divider, correct for that
                    new_value = new_value * max_val  # scale it to the sensor's unit range
                new_record = np.array([(current_time, float(new_value))], dtype=timestamped_float)
                new_records.append((label, new_record))
                break

    for label, new_record in new_records:
        sensor_type, df_index, previous_values, min_val, max_val = sensor_set[label]
        sensor_set[label] = (sensor_type, df_index, np.concatenate((previous_values, new_record)), min_val, max_val)


def get_random_df():  # for testing
    random_floats = [random.uniform(0.0, 1000.0) for _ in range(len(sensor_set))]
    random_bools = [random.choice([True, False]) for _ in range(len(valve_set))]
    random_bools_int = [int(b) for b in random_bools]
    format_string = str(len(sensor_set)) + "f" + str(len(valve_set)) + "?"
    packed_data = struct.pack(format_string, *random_floats, *random_bools_int)
    unpacked_data = struct.unpack(format_string, packed_data)
    return unpacked_data


def generate_unique_name():
    current_time = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    unique_id = uuid.uuid4().hex
    unique_name = f"{current_time}_{unique_id}" + ".csv"
    return unique_name


unique_session_name = generate_unique_name()


def save_data(filename=unique_session_name):
    # Create a dictionary to hold the data for the DataFrame
    data_dict = {'timestamp': [], 'row_number': []}

    # Initialize columns for each sensor
    for label in sensor_set.keys():
        data_dict[label] = []

    # Initialize columns for each valve
    for label, (df_index, relay_pin, enabled) in valve_set.items():
        data_dict[label] = []

    # Find the maximum length of the data arrays
    max_length = max(len(sensor_set[label][2]) for label in sensor_set.keys())
    max_length = max(max_length, len(valve_set))

    for i in range(max_length):
        data_dict['row_number'].append(i)

        if i < len(list(sensor_set.values())[0][2]):
            timestamp = list(sensor_set.values())[0][2]['timestamp'][i]
        else:
            timestamp = pd.NaT
        data_dict['timestamp'].append(timestamp)

        for label, (sensor_type, df_index, previous_values, min_val, max_val) in sensor_set.items():
            if i < len(previous_values):
                value = previous_values['value'][i]
            else:
                value = None
            data_dict[label].append(value)

        # Populate valve states
        for label, (df_index, relay_pin, enabled) in valve_set.items():
            if i < len(valve_set):
                valve_state = valve_set[label][2]
            else:
                valve_state = False
            data_dict[label].append(valve_state)

    # Create a DataFrame from the dictionary
    df = pd.DataFrame(data_dict)

    # Convert the timestamp to milliseconds since the start of the test
    start_time = df['timestamp'].iloc[0]
    df['timestamp'] = (df['timestamp'] - start_time) / np.timedelta64(1, 'ms')

    # Save the DataFrame to a CSV file
    filepath = os.path.join('data', filename)
    df.to_csv(filepath, index=False)
    print(f"Data saved to {filepath}")

path_to_font = None  # "path/to/font.ttf"

opened_state = True


def toarr(a: np.ndarray):
    return array.array(a.dtype.char, bytearray(a.data))  # 0.10ms


def frame_commands():
    global valve_set
    global cold_flow_routine
    global send_valve_packets
    global packet_rate_time
    global packet_receive_count
    global packet_receive_rate
    global unique_session_name

    gl.glClearColor(0.1, 0.1, 0.1, 1)
    gl.glClear(gl.GL_COLOR_BUFFER_BIT)

    io = imgui.get_io()

    if io.key_ctrl and io.keys_down[glfw.KEY_Q]:
        sys.exit(0)

    if imgui.begin_main_menu_bar():
        if imgui.begin_menu("File", True):
            clicked_quit, selected_quit = imgui.menu_item("Quit", "Ctrl+Q", False, True)

            if clicked_quit:
                sys.exit(0)

            imgui.end_menu()
        imgui.end_main_menu_bar()

    # turn windows on/off
    imgui.begin("Active examples")
    for label, enabled in active.copy().items():
        _, enabled = imgui.checkbox(label, enabled)
        active[label] = enabled
    imgui.end()

    imgui.begin("Valve Controls")
    if active["Valve Controls"]:
        global valve_lockout
        _, valve_lockout = imgui.checkbox("Safety Lockout", valve_lockout)
        _, send_valve_packets = imgui.checkbox("Send Valve Packets", send_valve_packets)
        if not valve_lockout:
            imgui.separator()
            for label, (df_index, pin, enabled) in valve_set.copy().items():
                _, enabled = imgui.checkbox(label, enabled)
                valve_set[label] = (df_index, pin, enabled)

    if imgui.button("Cold Flow Routine"):
        cold_flow_routine.run()


    imgui.end()

    imgui.begin("Sensor Readouts")
    if active["Sensor Readouts"]:
        # Add numerical display of sensor values
        imgui.text("Sensor Values:")
        for label, (sensor_type, index, previous_values, min_val, max_val) in sensor_set.items():
            if len(previous_values) > 0:
                current_value = previous_values['value'][-1]
                imgui.text(f"{label}: {current_value:.2f}")
            else:
                imgui.text(f"{label}: No data")

        imgui.separator()

        current_time = time.time()
        if packet_rate_time is None:
            packet_rate_time = current_time
        time_diff = current_time - packet_rate_time
        if time_diff > 2:  # Check if time_diff is greater than 0
            packet_receive_rate = packet_receive_count / time_diff
            packet_receive_count = 0
            packet_rate_time = current_time
        imgui.text(f"Packet Receive Rate: {packet_receive_rate:.2f} Hz")

        imgui.separator()

        if imgui.button("Save Sensor Data"):
            save_data()
            unique_session_name = generate_unique_name()

        imgui.separator()

        if imgui.button("Clear Sensor Data"):
            for label in sensor_set:
                sensor_set[label] = (
                sensor_set[label][0], sensor_set[label][1], np.array([], dtype=timestamped_float), sensor_set[label][3],
                sensor_set[label][4])

        imgui.separator()

        plot_data = False
        if plot_data:
            if implot.begin_plot("Pressure Transducers"):
                for label, (sensor_type, index, previous_values, min_val, max_val) in sensor_set.items():
                    if sensor_type == "Pressure Transducer" and len(previous_values) > 0:
                        timestamps = previous_values['timestamp'].astype('float64')
                        values = previous_values['value']

                        # Get the last 30 seconds of data
                        start_time = timestamps[-1] - 30 * 1e9  # 30 seconds in nanoseconds
                        mask = timestamps >= start_time
                        timestamps = timestamps[mask]
                        values = values[mask]

                        # Normalize timestamps to start at 0, and seconds
                        timestamps = timestamps.astype('float64') / 1e9  # Convert nanoseconds to seconds
                        timestamps -= timestamps[0]  # Normalize to start at 0

                        # Convert to array.array
                        timestamps = array.array('d', timestamps)
                        values = array.array('d', values)

                        implot.plot_line2(label, timestamps, values, len(values))
                implot.end_plot()

            if implot.begin_plot("Thermocouples"):
                for label, (sensor_type, index, previous_values, min_val, max_val) in sensor_set.items():
                    if sensor_type == "Thermocouple" and len(previous_values) > 0:
                        timestamps = previous_values['timestamp'].astype('float64')
                        values = previous_values['value']

                        # Get the last 30 seconds of data
                        start_time = timestamps[-1] - 30 * 1e9  # 30 seconds in nanoseconds
                        mask = timestamps >= start_time
                        timestamps = timestamps[mask]
                        values = values[mask]

                        # Normalize timestamps to start at 0, and seconds
                        timestamps = timestamps.astype('float64') / 1e9  # Convert nanoseconds to seconds
                        timestamps -= timestamps[0]  # Normalize to start at 0

                        # Convert to array.array
                        timestamps = array.array('d', timestamps)
                        values = array.array('d', values)

                        implot.plot_line2(label, timestamps, values, len(values))
                implot.end_plot()

            if implot.begin_plot("Loadcells"):
                for label, (sensor_type, index, previous_values, min_val, max_val) in sensor_set.items():
                    if sensor_type == "Loadcell" and len(previous_values) > 0:
                        timestamps = previous_values['timestamp'].astype('float64')
                        values = previous_values['value']

                        # Get the last 30 seconds of data
                        start_time = timestamps[-1] - 30 * 1e9  # 30 seconds in nanoseconds
                        mask = timestamps >= start_time
                        timestamps = timestamps[mask]
                        values = values[mask]

                        # Normalize timestamps to start at 0, and seconds
                        timestamps = timestamps.astype('float64') / 1e9  # Convert nanoseconds to seconds
                        timestamps -= timestamps[0]  # Normalize to start at 0

                        # Convert to array.array
                        timestamps = array.array('d', timestamps)
                        values = array.array('d', values)

                        implot.plot_line2(label, timestamps, values, len(values))
                implot.end_plot()
    imgui.end()


def render_frame(impl, window, font):
    glfw.poll_events()
    impl.process_inputs()
    imgui.new_frame()

    gl.glClearColor(0.1, 0.1, 0.1, 1)
    gl.glClear(gl.GL_COLOR_BUFFER_BIT)

    if font is not None:
        imgui.push_font(font)
    frame_commands()
    if font is not None:
        imgui.pop_font()

    imgui.render()
    impl.render(imgui.get_draw_data())
    glfw.swap_buffers(window)





def impl_glfw_init():
    width, height = 1600, 900
    window_name = "minimal ImGui/GLFW3 example"

    if not glfw.init():
        print("Could not initialize OpenGL context")
        sys.exit(1)

    glfw.window_hint(glfw.CONTEXT_VERSION_MAJOR, 3)
    glfw.window_hint(glfw.CONTEXT_VERSION_MINOR, 3)
    glfw.window_hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
    glfw.window_hint(glfw.OPENGL_FORWARD_COMPAT, gl.GL_TRUE)

    window = glfw.create_window(int(width), int(height), window_name, None, None)
    glfw.make_context_current(window)

    if not window:
        glfw.terminate()
        print("Could not initialize Window")
        sys.exit(1)

    return window


framecount = 0


def main():
    c1 = imgui.create_context()
    _ = implot.create_context()
    implot.set_imgui_context(c1)

    window = impl_glfw_init()

    impl = GlfwRenderer(window)

    io = imgui.get_io()
    jb = io.fonts.add_font_from_file_ttf(path_to_font, 30) if path_to_font is not None else None
    impl.refresh_font_texture()

    last_save_time = time.time()
    save_interval = 2  # (seconds)

    while True:
        send_data_packet()
        receive_data_packet()

        global framecount
        framecount += 1

        if cold_flow_routine.state > 0:
            cold_flow_routine.run()

        if (framecount % 10 == 0):
            render_frame(impl, window, jb)

        #if (framecount % 10 == 0):
        #    if active["Save Data"]:
        #        current_time = time.time()
        #        if current_time - last_save_time >= save_interval:
        #            save_data()
        #            print("Saving data")
        #            last_save_time = time.time()


    impl.shutdown()
    glfw.terminate()


def receive_data_packet():
    global network_socket
    global receive_dataframe_format
    global packet_receive_count

    try:
        data, addr = network_socket.recvfrom(100)  # Arbitrarily selected buffer size
        if data:
            df = struct.unpack(receive_dataframe_format, data)
            ingest_data(df)
            packet_receive_count += 1
    except BlockingIOError:
        pass
    except Exception as e:
        print(f"Error receiving data packet: {e}")


def send_data_packet():
    global network_socket
    global valve_set
    global send_valve_packets
    global previous_valve_states

    if not send_valve_packets:
        return;

    # Create a list to hold the valve states in the correct order
    valve_states_ordered = [False] * 16

    # Populate the valve_states_ordered list based on the relay pin numbers
    for label, (df_index, relay_pin, enabled) in valve_set.items():
        valve_states_ordered[relay_pin - 1] = enabled

    # Check if the valve states have changed
    if previous_valve_states is None or previous_valve_states != valve_states_ordered:
        # Update the previous valve states
        previous_valve_states = valve_states_ordered.copy()

        # Pack the data
        format_string = '16B'
        packed_data = struct.pack(format_string, *[int(state) for state in valve_states_ordered])

        # Send the data packet
        target_address = ('192.168.5.5', 65432)
        network_socket.sendto(packed_data, target_address)
        print("Data packet sent:", packed_data)

def setup_ethernet():
    global network_socket
    network_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    network_socket.bind(('192.168.5.2', 65432))  # IP of this PC
    network_socket.setblocking(0)
    network_socket.settimeout(1)
    network_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 100*100)


if __name__ == "__main__":
    setup_ethernet()
    main()
