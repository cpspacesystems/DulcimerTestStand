// Dear ImGui: standalone example application for Win32 + OpenGL 3

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// This is provided for completeness, however it is strongly recommended you use OpenGL with SDL or GLFW.

#define NOMINMAX

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <GL/GL.h>
#include <tchar.h>
#include <vector>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <thread>
#include <future>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>
#include <cstdint>


#include "Sensor.h"
#include "Valve.h"



// Data stored per platform window
struct WGL_WindowData { HDC hDC; };

// Data
static HGLRC            g_hRC;
static WGL_WindowData   g_MainWindow;
static int              g_Width;
static int              g_Height;

// Forward declarations of helper functions
bool CreateDeviceWGL(HWND hWnd, WGL_WindowData* data);
void CleanupDeviceWGL(HWND hWnd, WGL_WindowData* data);
void ResetDeviceWGL();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define MAXLINE 1024

struct receive_dataframe {
    uint16_t analogValues[18]; // Teensy 4.1 has 18 analog pins
    float load_cells[3];
    float thermocouples[2];
    unsigned long timestamp;
};

std::vector<unsigned long> timestamps = {};

bool valve_lockout = true;
bool send_valve_packets = false;
// Valve(const std::string& name, int dataframeIndex, int relayPin, bool enabled);
std::vector<Valve> valve_list = {
    Valve("Oxidizer Run", -1, 12, false),
    Valve("Fuel Run", -1, 13, false),
    Valve("Oxidizer Press", -1, 16, false),
    Valve("Fuel Press", -1, 14, false),
    Valve("Purge", -1, 1, false),
    Valve("Oxidizer Fill", -1, 15, false),
    Valve("Oxidizer Vent", -1, 4, false),
    Valve("Oxidizer Dump", -1, 3, false),
    Valve("Fuel Dump", -1, 2, false),
    Valve("Igniter", -1, 11, false)
};

// Sensor(const std::string& name, const std::string& type, int dataframeIndex, int minValue, int maxValue, const std::string& units);
std::vector<Sensor> sensors = {
    Sensor("Oxidizer Press Regulator", "Pressure Transducer", 2, 0, 1500, "psi"),
    Sensor("Fuel Press Regulator", "Pressure Transducer", 3, 0, 1500, "psi"),
    Sensor("Oxidizer Injector", "Pressure Transducer", 0, 0, 1000, "psi"),
    Sensor("Fuel Injector", "Pressure Transducer", 1, 0, 1000, "psi"),
    Sensor("Combustion Chamber", "Pressure Transducer", 6, 0, 1000, "psi"),
    Sensor("Fuel Flow", "Flow Meter", 7, 0, 1024, "raw voltage (0-1024)"),
    Sensor("Oxidizer Flow", "Flow Meter", 8, 0, 1024, "raw voltage (0-1024)"),

    Sensor("Engine Loadcell #1", "Loadcell", 18, 0, 500, "lbf"),
    Sensor("Engine Loadcell #2", "Loadcell", 19, 0, 500, "lbf"),
    Sensor("Engine Loadcell #3", "Loadcell", 20, 0, 500, "lbf"),

    Sensor("Engine Thermocouple", "Thermocouple", 21, 0, 100, "deg C"),
    Sensor("Oxidizer Tank Thermocouple", "Thermocouple", 22, 0, 100, "deg C"),
};


int create_net_interface(const char* ip_address, int port) {
    WSADATA wsaData;
    int sockfd;
    struct sockaddr_in servaddr;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        exit(EXIT_FAILURE);
    }

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET; // IPv4
    inet_pton(AF_INET, ip_address, &servaddr.sin_addr); // Specific IP address
    servaddr.sin_port = htons(port); // Specific port

    // Bind the socket with the server address
    if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(sockfd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 1;

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&read_timeout, sizeof read_timeout);

    return sockfd;
}


bool read_dataframe(int sockfd, receive_dataframe& dataframe) {
    char buffer[MAXLINE];
    struct sockaddr_in cliaddr;
    int len = sizeof(cliaddr);
    int n;

    fd_set readfds;
    struct timeval tv;
    tv.tv_sec = 0;  // 0 seconds
    tv.tv_usec = 0;

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    int retval = select(sockfd + 1, &readfds, NULL, NULL, &tv);

    if (retval == SOCKET_ERROR) {
        std::cerr << "select() failed: " << WSAGetLastError() << std::endl;
        return false;
    }
    else if (retval == 0) {
        // No data available
        return false;
    }

    if (FD_ISSET(sockfd, &readfds)) {
        n = recvfrom(sockfd, buffer, MAXLINE, 0, (struct sockaddr*)&cliaddr, &len);
        if (n == SOCKET_ERROR) {
            std::cerr << "recvfrom failed: " << WSAGetLastError() << std::endl;
            return false;
        }

        if (n < sizeof(receive_dataframe)) {
            std::cerr << "Received packet is too small" << std::endl;
            return false;
        }

        // Copy the received data into the dataframe struct
        memcpy(&dataframe, buffer, sizeof(receive_dataframe));

        return true;

        // Print the received data for debugging
        //std::cout << "Received dataframe:" << std::endl;
        //std::cout << "Analog Values: ";
        //for (int i = 0; i < 18; ++i) {
        //    std::cout << dataframe.analogValues[i] << " ";
        //}
        //std::cout << std::endl;

        //std::cout << "Load Cells: ";
        //for (int i = 0; i < 3; ++i) {
        //    std::cout << dataframe.load_cells[i] << " ";
        //}
        //std::cout << std::endl;

        //std::cout << "Thermocouples: ";
        //for (int i = 0; i < 2; ++i) {
        //    std::cout << dataframe.thermocouples[i] << " ";
        //}
        //std::cout << std::endl;

        //std::cout << "Timestamp: " << dataframe.timestamp << std::endl;
    }
}

std::atomic<int> packet_count(0);
std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
double intake_fps = 0.f;

double calculate_packet_rate() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto duration = duration_cast<seconds>(now - start_time).count();

    if (duration == 0) {
        return 0.0; // Avoid division by zero
    }

    double rate = static_cast<double>(packet_count) / duration;

    // Reset the counters periodically (e.g., every second)
    if (duration >= 1) {
        packet_count = 0;
        start_time = now;
    }

    intake_fps = rate;
}


bool retain_data = false;

void clearAllSensorData() {
    for (auto& sensor : sensors) {
        sensor.clearValues();
    }

    for (auto& valve : valve_list) {
        valve.clearValues();
    }

    timestamps.clear();
}

void ingest_data(const receive_dataframe& df) {

    if (!retain_data)
    {
        clearAllSensorData(); // don't keep the old data around unless we want to, eats memory fast
    }

    packet_count++;
    calculate_packet_rate();

    timestamps.push_back(df.timestamp);

    for (auto& sensor : sensors) {
        int index = sensor.getDataframeIndex();
        if (index >= 0 && index < 18) {
            // Analog values
            if (sensor.getType() == "Pressure Transducer")
            {
                float reading = df.analogValues[index];
                float new_value = reading / 1024; // scale it 0-1, teensy outputs 0-1024
                new_value = new_value / (2 / 3.3); // correct for 5x voltage divider in the elec box
                new_value = new_value * sensor.getMaxValue(); // scale it to the PTs range, some are 0-1000, some 0-1500
                sensor.addValue(new_value);
            }
            else
            {
                sensor.addValue(df.analogValues[index]);
            }
        }
        else if (index >= 18 && index < 21) {
            // Load cells
            sensor.addValue(df.load_cells[index - 18]);
        }
        else if (index >= 21 && index < 23) {
            // Thermocouples
            sensor.addValue(df.thermocouples[index - 21]);
        }
        else {
            std::cerr << "Invalid dataframe index for sensor: " << sensor.getName() << std::endl;
        }
    }

    for (auto& valve : valve_list)
    {
        valve.addValue(valve.isEnabled());
    }
}

bool last_packet[16] = { false };

void send_dataframe(int sockfd, const struct sockaddr_in& servaddr) {
    if (!send_valve_packets)
    {
        return;
    }

    // Create a packet of 16 bools
    bool packet[16] = { false };

    // Iterate through the valve list and set the corresponding bits in the packet
    for (const auto& valve : valve_list) {
        int relayPin = valve.getRelayPin();
        packet[relayPin - 1] = valve.isEnabled();
    }

    // Compare the current packet with the last packet
    if (std::memcmp(packet, last_packet, sizeof(packet)) != 0) {
        // Packets are different, so send the new packet
        int n = sendto(sockfd, reinterpret_cast<const char*>(packet), sizeof(packet), 0,
            (const struct sockaddr*)&servaddr, sizeof(servaddr));
        if (n == SOCKET_ERROR) {
            std::cerr << "sendto failed: " << WSAGetLastError() << std::endl;
        }
        else {
            // Update the last_packet to the current packet
            std::memcpy(last_packet, packet, sizeof(packet));
        }
    }
}


std::string generateUniqueFilename(const std::string& baseName) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << baseName << "_";
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    ss << ".csv";
    return ss.str();
}

void saveDataToCSV(const std::string& baseName,
    std::vector<unsigned long> timestampsCopy,
    std::vector<Sensor> sensorsCopy,
    std::vector<Valve> valveListCopy) {
    std::string filename = generateUniqueFilename(baseName);
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    // Write the header
    file << "Timestamp";
    for (const auto& sensor : sensorsCopy) {
        file << "," << sensor.getName();
    }
    for (const auto& valve : valveListCopy) {
        file << "," << valve.getName();
    }
    file << "\n";

    // Determine the number of rows to write
    size_t numRows = timestampsCopy.size();
    for (const auto& sensor : sensorsCopy) {
        numRows = std::min(numRows, sensor.getValues().size());
    }
    for (const auto& valve : valveListCopy) {
        numRows = std::min(numRows, valve.getValues().size());
    }

    // Write the data
    for (size_t i = 0; i < numRows; ++i) {
        file << timestampsCopy[i];
        for (const auto& sensor : sensorsCopy) {
            const auto& values = sensor.getValues();
            if (i < values.size()) {
                file << "," << values[i];
            }
            else {
                file << ",";
            }
        }
        for (const auto& valve : valveListCopy) {
            const auto& values = valve.getValues();
            if (i < values.size()) {
                file << "," << (values[i] ? "1" : "0");
            }
            else {
                file << ",";
            }
        }
        file << "\n";
    }

    file.close();
    std::cout << "Data saved to " << filename << std::endl;
}

// This function was implemented because saving the data would freeze the DAQ.
// Now it saves on another thread in the background 
void saveDataToCSVAsync(const std::string& baseName,
    std::vector<unsigned long> timestampsCopy,
    std::vector<Sensor> sensorsCopy,
    std::vector<Valve> valveListCopy) {
    std::thread saveThread(saveDataToCSV, baseName, timestampsCopy, sensorsCopy, valveListCopy);
    saveThread.detach(); // Detach the thread to run independently.
}

bool show_sensor_readouts = true;

void renderSensorReadouts() {
    if (show_sensor_readouts) {
        ImGui::Begin("Sensor Values");
        for (const auto& sensor : sensors) {
            ImGui::Text("Sensor: %s", sensor.getName().c_str());
            const auto& values = sensor.getValues();
            if (!values.empty()) {
                ImGui::Text(" Latest Value: %.2f %s", values.back(), sensor.getUnits().c_str());
                //ImGui::PlotLines("##plot", values.data(), values.size(), 0, nullptr, sensor.getMinValue(), sensor.getMaxValue(), ImVec2(0, 80));
            }
            else {
                ImGui::Text(" Latest Value: ~ %s", sensor.getUnits().c_str());
            }
            ImGui::Separator();
        }

        ImGui::Checkbox("Retain Data", &retain_data);

        if (retain_data)
        {
            if (ImGui::Button("Save Data"))
            {
                retain_data = false;

                std::string base_name = "Carillon";
                std::vector<unsigned long> timestampsCopy = timestamps;
                std::vector<Sensor> sensorsCopy = sensors;
                std::vector<Valve> valveListCopy = valve_list;

                saveDataToCSVAsync(base_name, timestampsCopy, sensorsCopy, valveListCopy);
            }
        }

        ImGui::End();
    }
}

// Function to find a valve by name
Valve* findValveByName(const std::string& name) {
    for (auto& valve : valve_list) {
        if (valve.getName() == name) {
            return &valve;
        }
    }
    return nullptr;
}


enum ColdFlowState {
    NONE,
    INIT,
    OPEN_FUEL,
    OPEN_OX,
    WAIT,
    CLOSE_VALVES,
    DONE
};


ColdFlowState ColdFlowCurrentState = NONE;
auto ColdFlowStartTime = std::chrono::steady_clock::now();

void coldFlowRoutine(bool start) {
    if (start)
    {
        ColdFlowCurrentState = INIT;
        ColdFlowStartTime = std::chrono::steady_clock::now();
    }

    switch (ColdFlowCurrentState) {
    case INIT: {
        std::cout << "Initializing cold flow routine...\n";
        ColdFlowCurrentState = OPEN_FUEL;
        break;
    }
    case OPEN_FUEL: {
        std::cout << "Opening fuel run valve...\n";
        Valve* fuelRunValve = findValveByName("Fuel Run");
        if (fuelRunValve) {
            fuelRunValve->setEnabled(true);
            ColdFlowStartTime = std::chrono::steady_clock::now();
            ColdFlowCurrentState = OPEN_OX;
        }
        else {
            std::cerr << "Error: Valve not found!\n";
            ColdFlowCurrentState = DONE;
        }
        break;
    }
    case OPEN_OX: {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - ColdFlowStartTime).count();
        if (elapsedTime >= 185) // MILLISECONDS!!!
        {
            std::cout << "Opening oxidizer run valve...\n";
            Valve* oxidizerRunValve = findValveByName("Oxidizer Run");
            if (oxidizerRunValve) {
                oxidizerRunValve->setEnabled(true);
                ColdFlowStartTime = std::chrono::steady_clock::now();
                ColdFlowCurrentState = WAIT;
            }
            else {
                std::cerr << "Error: Valve not found!\n";
                ColdFlowCurrentState = DONE;
            }
        }
        break;
    }
    case WAIT: {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - ColdFlowStartTime).count();
        if (elapsedTime >= 0.5) { // SECONDS!!!
            ColdFlowCurrentState = CLOSE_VALVES;
        }
        break;
    }
    case CLOSE_VALVES: {
        std::cout << "Closing oxidizer Run and fuel Run valves...\n";
        Valve* oxidizerRunValve = findValveByName("Oxidizer Run");
        Valve* fuelRunValve = findValveByName("Fuel Run");
        if (oxidizerRunValve && fuelRunValve) {
            oxidizerRunValve->setEnabled(false);
            fuelRunValve->setEnabled(false);
            ColdFlowCurrentState = DONE;
        }
        else {
            std::cerr << "Error: Valves not found!\n";
            ColdFlowCurrentState = DONE;
        }
        break;
    }
    case DONE: {
        std::cout << "Process completed.\n";
        break;
    }
    }
}

enum HotFireState {
    HF_NONE,
    HF_INIT,
    HF_IGNITE,
    HF_OPEN_FUEL,
    HF_OPEN_OX,
    HF_WAIT,
    HF_CLOSE_VALVES,
    HF_DONE
};

HotFireState HotFireCurrentState = HF_NONE;
auto HotFireStartTime = std::chrono::steady_clock::now();

void HotFireRoutine(bool start) {
    if (start)
    {
        HotFireCurrentState = HF_INIT;
        HotFireStartTime = std::chrono::steady_clock::now();
    }

    switch (HotFireCurrentState) {
    case HF_INIT: {
        std::cout << "HF_INITializing hot fire routine...\n";
        HotFireCurrentState = HF_IGNITE;
        break;
    }
    case HF_IGNITE: {
        std::cout << "Igniting...\n";
        Valve* HF_IGNITER = findValveByName("Igniter");
        if (HF_IGNITER) {
            HF_IGNITER->setEnabled(true);
            HotFireStartTime = std::chrono::steady_clock::now();
            HotFireCurrentState = HF_OPEN_FUEL;
        }
        else {
            std::cerr << "Error: Igniter not found!\n";
            HotFireCurrentState = HF_DONE;
        }
        break;
    }
    case HF_OPEN_FUEL: {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - HotFireStartTime).count();
        if (elapsedTime >= 750) // MILLISECONDS!!!
        {
            std::cout << "Opening fuel run valve...\n";
            Valve* fuelRunValve = findValveByName("Fuel Run");
            Valve* HF_IGNITER = findValveByName("Igniter");
            if (fuelRunValve && HF_IGNITER) {
                fuelRunValve->setEnabled(true);
                HF_IGNITER->setEnabled(false);
                HotFireStartTime = std::chrono::steady_clock::now();
                HotFireCurrentState = HF_OPEN_OX;
            }
            else {
                std::cerr << "Error: Valve not found!\n";
                HotFireCurrentState = HF_DONE;
            }
        }
        break;
    }
    case HF_OPEN_OX: {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - HotFireStartTime).count();
        if (elapsedTime >= 185) // MILLISECONDS!!!
        {
            std::cout << "Opening oxidizer run valve...\n";
            Valve* oxidizerRunValve = findValveByName("Oxidizer Run");
            if (oxidizerRunValve) {
                oxidizerRunValve->setEnabled(true);
                HotFireStartTime = std::chrono::steady_clock::now();
                HotFireCurrentState = HF_WAIT;
            }
            else {
                std::cerr << "Error: Valve not found!\n";
                HotFireCurrentState = HF_DONE;
            }
        }
        break;
    }
    case HF_WAIT: {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - HotFireStartTime).count();
        if (elapsedTime >= 3) { // SECONDS!!!
            HotFireCurrentState = HF_CLOSE_VALVES;
        }
        break;
    }
    case HF_CLOSE_VALVES: {
        std::cout << "Closing valves and disabling HF_IGNITEr...\n";
        Valve* oxidizerRunValve = findValveByName("Oxidizer Run");
        Valve* fuelRunValve = findValveByName("Fuel Run");
        if (oxidizerRunValve && fuelRunValve) {
            oxidizerRunValve->setEnabled(false);
            fuelRunValve->setEnabled(false);
            HotFireCurrentState = HF_DONE;
        }
        else {
            std::cerr << "Error: Valves not found!\n";
            HotFireCurrentState = HF_DONE;
        }
        break;
    }
    case HF_DONE: {
        std::cout << "Process completed.\n";
        break;
    }
    }
}




std::chrono::milliseconds render_last_ms;

// Main code
int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_OWNDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"INC++ DAQ", WS_OVERLAPPEDWINDOW, 100, 100, 1920, 1080, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize OpenGL
    if (!CreateDeviceWGL(hwnd, &g_MainWindow))
    {
        CleanupDeviceWGL(hwnd, &g_MainWindow);
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    wglMakeCurrent(g_MainWindow.hDC, g_hRC);

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;    // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_InitForOpenGL(hwnd);
    ImGui_ImplOpenGL3_Init();

    io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\Consola.ttf", 18.0f);

    ((BOOL(WINAPI*)(int))wglGetProcAddress("wglSwapIntervalEXT"))(0); // disable v-sync

    const char* local_ip_address = "192.168.5.2";
    int local_port = 65432;

    const char* remote_ip_address = "192.168.5.5";
    int remote_port = 65432;

    int sockfd = create_net_interface(local_ip_address, local_port);

    // Create a network destination for the teensy (for sending)
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(remote_port);
    inet_pton(AF_INET, remote_ip_address, &servaddr.sin_addr);



    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    bool show_valve_controls = true;
    bool valve_lockout = true;

    bool show_sensor_readouts = true;

    int downsample_rate = 5; // only keep every x packets, otherwise it's too much data (~25k hz raw)
    unsigned long packet_count = 0;

    // Main loop
    bool done = false;
    while (!done)
    {
        receive_dataframe receive_df;
        if (read_dataframe(sockfd, receive_df))
        {
            packet_count++;
            if (packet_count % downsample_rate == 0)
            {
                ingest_data(receive_df);
            }
        }
        send_dataframe(sockfd, servaddr);

        using namespace std::chrono;

        const int render_fps = 30;
        const milliseconds frameTime(1000 / render_fps);

        milliseconds current_ms = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()
        );

        if (current_ms - render_last_ms < frameTime)
        {
            continue;
        }

        render_last_ms = current_ms;


        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();


        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {

            ImGui::Begin("FPS");                          // Create a window called "Hello, world!" and append into it.
            ImGui::Text("Render average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::Text("Receive packet rate: %.1f PPS", intake_fps);
            ImGui::End();
        }

        if (show_valve_controls)
        {
            ImGui::Begin("Valve Controls", &show_valve_controls);

            ImGui::Checkbox("Safety Lockout", &valve_lockout);
            ImGui::Checkbox("Send Valve Packets", &send_valve_packets);

            if (!valve_lockout)
            {
                ImGui::Separator();

                for (auto& valve : valve_list) {
                    bool enabled = valve.isEnabled();
                    if (ImGui::Checkbox(valve.getName().c_str(), &enabled)) {
                        valve.setEnabled(enabled);
                    }
                }

                if (ImGui::Button("Cold Flow"))
                {
                    coldFlowRoutine(true);
                }
                else
                {
                    coldFlowRoutine(false);
                }

                if (ImGui::Button("Hot Fire"))
                {
                    HotFireRoutine(true);
                }
                else
                {
                    HotFireRoutine(false);
                }
            }

            ImGui::End();
        }

        renderSensorReadouts();

        // Rendering
        ImGui::Render();
        glViewport(0, 0, g_Width, g_Height);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Present
        ::SwapBuffers(g_MainWindow.hDC);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceWGL(hwnd, &g_MainWindow);
    wglDeleteContext(g_hRC);
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions
bool CreateDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
    HDC hDc = ::GetDC(hWnd);
    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    const int pf = ::ChoosePixelFormat(hDc, &pfd);
    if (pf == 0)
        return false;
    if (::SetPixelFormat(hDc, pf, &pfd) == FALSE)
        return false;
    ::ReleaseDC(hWnd, hDc);

    data->hDC = ::GetDC(hWnd);
    if (!g_hRC)
        g_hRC = wglCreateContext(data->hDC);
    return true;
}

void CleanupDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
    wglMakeCurrent(nullptr, nullptr);
    ::ReleaseDC(hWnd, data->hDC);
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            g_Width = LOWORD(lParam);
            g_Height = HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
