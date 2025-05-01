/******************************************************************
 * RangerOS - Tailsitter Drone Control System
 * This code creates a web interface to control a tailsitter drone
 * Features:
 * - WiFi access point for connection
 * - Web-based control interface
 * - Real-time control via WebSocket
 * - Yaw, Roll, and Throttle control
 * - Transition between vertical and horizontal flight modes
 ******************************************************************/

// Required Libraries
#include <WiFi.h>              // ESP32 WiFi library for network functionality
#include <WebServer.h>         // Standard web server library for handling HTTP requests
#include <WebSockets.h>        // WebSocket library for real-time communication
#include <WebSocketsServer.h>  // WebSocket server implementation
#include <ESP32Servo.h>        // ESP32-specific servo library with PWM support

// WiFi Configuration
const char* ssid = "RangerOS";     // Network name that will appear in WiFi list
const char* password = "12345678";  // Network password (must be at least 8 characters)

// Server Initialization
WebServer server(80);              // HTTP server on standard port 80
WebSocketsServer webSocket(81);    // WebSocket server on port 81 for real-time control

// Servo and Motor Objects
Servo servo1;      // First servo - used for both yaw and roll control
Servo servo2;      // Second servo - used for both yaw and roll control
Servo motor1;      // First brushless motor - left side
Servo motor2;      // Second brushless motor - right side

// Pin Definitions
const int motor1Pin = 12;  // ESC signal pin for first motor
const int motor2Pin = 13;  // ESC signal pin for second motor
const int servo1Pin = 14;  // Signal pin for first servo
const int servo2Pin = 15;  // Signal pin for second servo

// State Tracking Variables
int currentYaw = 90;       // Tracks current yaw position (90° is center)
int currentRoll = 90;      // Tracks current roll position (90° is center)
bool isHorizontal = false; // Tracks flight mode (false = vertical, true = horizontal)

// HTML webpage stored in program memory using PROGMEM to save RAM
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <!-- Ensure proper scaling on mobile devices -->
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RangerOS</title>
    <style>
        /* Base body styling */
        body {
            font-family: Arial, sans-serif;
            background-color: #333;      /* Dark background */
            color: #fff;                 /* White text */
            margin: 0;
            display: flex;
            flex-direction: column;
            align-items: center;         /* Center content horizontally */
            min-height: 100vh;           /* Full viewport height */
            padding: 20px;
            text-align: center;
            box-sizing: border-box;
        }
        /* Title styling */
        .title {
            font-size: 2.5em;
            margin-bottom: 30px;
            color: #FFD65C;             /* Gold color for title */
            text-transform: uppercase;
            letter-spacing: 2px;
        }
        /* Main controls container */
        .controls {
            width: 100%;
            max-width: 1200px;          /* Limit maximum width */
            display: flex;
            gap: 20px;                  /* Space between control groups */
            margin-bottom: 20px;
            justify-content: center;
        }
        /* Control group styling (left and right sections) */
        .control-group {
            flex: 1;
            max-width: 500px;
            display: flex;
            flex-direction: column;
            gap: 20px;
        }
        /* Right control group specific styling */
        .control-group.right {
            justify-content: center;     /* Center throttle vertically */
            min-height: 300px;
        }
        /* Individual control container */
        .control {
            background-color: #444;      /* Slightly lighter than background */
            padding: 20px;
            border-radius: 10px;         /* Rounded corners */
            display: flex;
            flex-direction: column;
            align-items: center;
        }
        /* Slider styling */
        .slider {
            width: 80%;
            margin: 10px 0;
            -webkit-appearance: none;     /* Remove default styling */
            height: 15px;
            border-radius: 7.5px;
            background: #666;
            outline: none;
        }
        /* Slider thumb (the draggable part) */
        .slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 25px;
            height: 25px;
            border-radius: 50%;          /* Circular thumb */
            background: #FFD65C;         /* Gold color */
            cursor: pointer;
        }
        /* Button container */
        .button-container {
            width: 100%;
            display: flex;
            gap: 20px;
            justify-content: center;
            margin: 20px 0;
        }
        /* Button styling */
        .button {
            padding: 15px 30px;
            font-size: 1.2em;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            background-color: #FFD65C;    /* Gold color */
            color: #333;
            font-weight: bold;
            transition: all 0.3s ease;    /* Smooth hover effect */
        }
        /* Button hover effect */
        .button:hover {
            background-color: #FFE584;    /* Lighter gold */
            transform: scale(1.05);       /* Slight enlargement */
        }
        /* Value display for sliders */
        .value-display {
            font-size: 1.2em;
            margin-top: 5px;
        }

        /* Responsive design for landscape mode */
        @media screen and (max-width: 900px) and (orientation: landscape) {
            /* Adjust sizes for landscape tablets and medium screens */
            body { padding: 10px; }
            .title { font-size: 1.5em; margin-bottom: 15px; }
            .controls { gap: 10px; }
            .control-group { max-width: none; }
            .control { padding: 10px; }
            .control-group.right { min-height: 200px; }
            .button-container { margin: 10px 0; }
            .button { padding: 10px 20px; font-size: 1em; }
            .value-display { font-size: 1em; margin-top: 3px; }
            .slider { height: 12px; }
            .slider::-webkit-slider-thumb { width: 20px; height: 20px; }
        }

        /* Extra small devices in landscape (phones) */
        @media screen and (max-height: 450px) and (orientation: landscape) {
            /* Further size reductions for phones */
            .title { font-size: 1.2em; margin-bottom: 10px; }
            .controls { gap: 5px; }
            .control { padding: 5px; }
            .control-group.right { min-height: 150px; }
            .button { padding: 8px 15px; font-size: 0.9em; }
            .value-display { font-size: 0.9em; }
            .button-container { margin: 5px 0; gap: 10px; }
        }
    </style>
</head>
<body>
    <!-- Main title -->
    <h1 class="title">RangerOS</h1>
    
    <!-- Main controls container -->
    <div class="controls">
        <!-- Left control group - Yaw and Roll -->
        <div class="control-group">
            <!-- Yaw control -->
            <div class="control">
                <label for="yaw">Yaw Control</label>
                <input type="range" id="yaw" class="slider" min="-100" max="100" value="0">
                <div class="value-display">Value: <span id="yawValue">0</span></div>
            </div>
            <!-- Roll control -->
            <div class="control">
                <label for="roll">Roll Control</label>
                <input type="range" id="roll" class="slider" min="-100" max="100" value="0">
                <div class="value-display">Value: <span id="rollValue">0</span></div>
            </div>
        </div>
        <!-- Right control group - Throttle -->
        <div class="control-group right">
            <div class="control">
                <label for="throttle">Throttle</label>
                <input type="range" id="throttle" class="slider" min="0" max="100" value="0">
                <div class="value-display">Value: <span id="throttleValue">0</span>%</div>
            </div>
        </div>
    </div>
    
    <!-- Transition buttons -->
    <div class="button-container">
        <button class="button" onclick="handleTransition(true)">Transition to Horizontal</button>
        <button class="button" onclick="handleTransition(false)">Transition to Vertical</button>
    </div>

    <script>
        var websocket;
        window.addEventListener('load', onLoad);
        
        function onLoad(event) {
            initWebSocket();
        }
        
        function initWebSocket() {
            console.log('Connecting to WebSocket...');
            // Connect to WebSocket on port 81
            websocket = new WebSocket('ws://' + window.location.hostname + ':81/');
            websocket.onopen = onOpen;
            websocket.onclose = onClose;
            websocket.onmessage = onMessage;
        }
        
        function onOpen(event) {
            console.log('Connected!');
        }
        
        function onClose(event) {
            console.log('Connection lost. Reconnecting...');
            setTimeout(initWebSocket, 2000);
        }
        
        function onMessage(event) {
            // Handle any incoming messages here
        }
        
        function updateValue(control, value) {
            document.getElementById(control + "Value").textContent = value;
            websocket.send(control + ":" + value);
        }
        
        function handleTransition(toHorizontal) {
            websocket.send("transition:" + (toHorizontal ? "1" : "0"));
        }
        
        // Add event listeners for sliders
        document.getElementById('yaw').addEventListener('input', function() {
            updateValue('yaw', this.value);
        });
        document.getElementById('roll').addEventListener('input', function() {
            updateValue('roll', this.value);
        });
        document.getElementById('throttle').addEventListener('input', function() {
            updateValue('throttle', this.value);
        });
        
        // Auto-center controls on release
        document.getElementById('yaw').addEventListener('mouseup', function() {
            this.value = 0;
            updateValue('yaw', 0);
        });
        document.getElementById('roll').addEventListener('mouseup', function() {
            this.value = 0;
            updateValue('roll', 0);
        });
        
        document.getElementById('yaw').addEventListener('touchend', function() {
            this.value = 0;
            updateValue('yaw', 0);
        });
        document.getElementById('roll').addEventListener('touchend', function() {
            this.value = 0;
            updateValue('roll', 0);
        });
    </script>
</body>
</html>
)rawliteral";

/**
 * Clears boot messages from serial output for cleaner display
 * This helps in reading important status messages
 */
void clearBootMessages() {
    // Clear any pending serial data
    while(Serial.available()) {
        Serial.read();
    }
    // Add blank lines to push boot messages up
    for(int i = 0; i < 10; i++) {
        Serial.println();
    }
}

/**
 * Initial setup function - runs once at startup
 * Initializes all hardware and network components
 */
void setup() {
    // Initialize serial communication for debugging
    Serial.begin(9600);         // Lower baud rate for more stable output
    delay(3000);               // Wait for serial connection to stabilize
    
    clearBootMessages();       // Clear boot messages for clean serial output
    
    // Initialize ESP32 PWM timers for servo control
    // ESP32 needs dedicated timers for PWM generation
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    // Configure servo and motor refresh rate
    // Standard 50Hz (20ms) for both servos and ESCs
    servo1.setPeriodHertz(50);
    servo2.setPeriodHertz(50);
    motor1.setPeriodHertz(50);
    motor2.setPeriodHertz(50);

    // Attach all servos and motors to their respective pins
    servo1.attach(servo1Pin);
    servo2.attach(servo2Pin);
    motor1.attach(motor1Pin);
    motor2.attach(motor2Pin);

    // Set safe initial positions
    servo1.write(currentYaw);  // Center position for servo1
    servo2.write(currentRoll); // Center position for servo2
    motor1.write(0);          // Motors off
    motor2.write(0);          // Motors off

    // Setup WiFi Access Point with improved stability
    WiFi.disconnect();         // Ensure no existing connections
    WiFi.mode(WIFI_OFF);      // Start fresh
    delay(1000);              // Give WiFi time to clear
    WiFi.mode(WIFI_AP);       // Set mode to Access Point
    // Configure Access Point with:
    // - Channel 1 (less interference)
    // - Not hidden (visible in WiFi list)
    // - Maximum 4 clients
    WiFi.softAP(ssid, password, 1, 0, 4);
    delay(1000);              // Allow AP to start up properly

    // Display connection information
    IPAddress IP = WiFi.softAPIP();
    Serial.println("RangerOS Starting...");
    Serial.println("----------------------------------------");
    Serial.println("WiFi Access Point Created");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(password);
    Serial.print("IP Address: ");
    Serial.println(IP);
    Serial.println("----------------------------------------");

    // Configure web server routes
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", index_html);
    });

    // Initialize WebSocket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    server.begin();
}

/**
 * Main program loop - runs continuously
 * Handles WebSocket and server client processing
 */
void loop() {
    webSocket.loop();         // Process WebSocket messages
    server.handleClient();    // Handle any pending HTTP requests
    delay(10);               // Small delay to prevent watchdog timer issues
}

/**
 * WebSocket Event Handler
 * Processes all WebSocket events and control messages
 * 
 * @param num     Client ID number
 * @param type    Type of WebSocket event
 * @param payload Data received in the message
 * @param length  Length of the message
 */
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            // Client disconnected - log the event
            Serial.println("----------------------------------------");
            Serial.printf("Client #%u disconnected\n", num);
            Serial.println("----------------------------------------");
            break;
        
        case WStype_CONNECTED:
            // New client connected - log the event
            Serial.println("----------------------------------------");
            Serial.printf("Client #%u connected\n", num);
            Serial.println("----------------------------------------");
            break;
        
        case WStype_TEXT:
            // Process control messages
            // Messages are formatted as "command:value"
            char* message = (char*)payload;
            String command = String(message).substring(0, String(message).indexOf(':'));
            int value = String(message).substring(String(message).indexOf(':') + 1).toInt();
            
            // Handle different control commands
            if (command == "yaw") {
                // Yaw control: maps -100 to +100 to servo angles 45° to 135°
                // Second servo moves in opposite direction for yaw
                int yawAngle = map(value, -100, 100, 45, 135);
                servo1.write(yawAngle);
                servo2.write(180 - yawAngle);
            }
            else if (command == "roll") {
                // Roll control: maps -100 to +100 to servo angles 45° to 135°
                // Both servos move in same direction for roll
                int rollAngle = map(value, -100, 100, 45, 135);
                servo1.write(rollAngle);
                servo2.write(rollAngle);
            }
            else if (command == "throttle") {
                // Throttle control: maps 0 to 100 to ESC range 0° to 180°
                // Both motors receive same value for straight flight
                int motorValue = map(value, 0, 100, 0, 180);
                motor1.write(motorValue);
                motor2.write(motorValue);
            }
            else if (command == "transition") {
                // Transition control: switches between flight modes
                if (value == 1) {
                    // Transition to horizontal flight
                    servo1.write(0);
                    servo2.write(0);
                    isHorizontal = true;
                } else {
                    // Transition to vertical flight
                    servo1.write(90);
                    servo2.write(90);
                    isHorizontal = false;
                }
            }
            break;
    }
}

