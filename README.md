# INC
 CPSS Test Stand Instrumentation & Control

# Instrumentation (LEGACY) 
- /Instrumentation
- legacy code for data collection and connection to ground station.
## Client
- /Instrumentation/Client
- code onboard the teensy 4.1 microcontroller that runs the test stand from the test stand control box.
- the teensy is known as the Test Stand Controller (TSC)
## Server
- /Instrumentation/Server
- code onboard the Ground Station Computer (GSC) that recieves data sent from the TSC, parses it, and displays it

# UI (LEGACY)
## Assets
- UI assets for the legacy UI implemented using imgui, see /Instrumentation/Server/imgui-master for the
## Main Dashboard
- /UI/main_dashboard.ui
- markup file that defines the UI dashboard webpage on the GSC

