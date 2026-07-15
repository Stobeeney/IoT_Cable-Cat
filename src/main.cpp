#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Preferences.h>

#define LED_PIN 8

// --- Configuration Constants ---
const int NUM_PULLEYS = 12;

// GPIO assignments for 12 Pulleys (ESP32-C3)
// 0-3: Green (Q1-Q4), 4-7: Yellow (Q1-Q4), 8-11: Red (Q1-Q4)
const int servoPins[NUM_PULLEYS] = {
  1, 2, 3, 4,       // Green Group (Q1, Q2, Q3, Q4)
  5, 6, 7, 8,       // Yellow Group (Q1, Q2, Q3, Q4)
  9, 10, 20, 21     // Red Group (Q1, Q2, Q3, Q4)
};

// Default Travel Durations (in milliseconds)
const unsigned long defaultDurationsUp[NUM_PULLEYS] = {
  8000, 3500, 6000, 3000, // Green Group (Q1-Q4)
  16000, 4000, 1500, 2000, // Yellow Group (Q1-Q4)
  300, 300, 300, 300      // Red Group (Q1-Q4)
};
const unsigned long defaultDurationsDown[NUM_PULLEYS] = {
  8000, 3500, 6000, 3000, // Green Group (Q1-Q4)
  1500, 4000, 1500, 2000, // Yellow Group (Q1-Q4)
  300, 300, 300, 300      // Red Group (Q1-Q4)
};

// Default Speeds (microseconds)
const int defaultFwd[NUM_PULLEYS] = {
  1200, 1000, 2000, 3000, // Green Group
  1000, 1000, 1000, 1000, // Yellow Group
  1000, 1000, 1000, 1000  // Red Group
};
const int defaultRev[NUM_PULLEYS] = {
  1800, 3000, 1000, 400,  // Green Group
  400, 2000, 2000, 2000,  // Yellow Group (Q1 Yellow reverse is 400)
  2000, 2000, 2000, 2000  // Red Group
};
const int defaultStop[NUM_PULLEYS] = {
  1500, 1500, 1500, 800,  // Green Group
  1500, 1500, 1500, 1500, // Yellow Group
  1500, 1500, 1500, 1500  // Red Group
};

// Active settings loaded from Flash (Preferences)
unsigned long durationUp[NUM_PULLEYS];
unsigned long durationDown[NUM_PULLEYS];
int speedFwd[NUM_PULLEYS];
int speedRev[NUM_PULLEYS];
int speedStop[NUM_PULLEYS];

// State variables for 12 Pulleys
// 0 = DOWN (Red/Bottom), 1 = UP (Green/Top), 2 = MOVING UP, 3 = MOVING DOWN
int currentStates[NUM_PULLEYS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// Independent Asynchronous Timer arrays
Servo servos[NUM_PULLEYS];
bool isMoving[NUM_PULLEYS] = {false, false, false, false, false, false, false, false, false, false, false, false};
unsigned long moveStartTime[NUM_PULLEYS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
unsigned long moveDuration[NUM_PULLEYS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// Create WebServer on port 80
WebServer server(80);
Preferences preferences;

// Forward Declarations
void loadSettings();
void saveSettings(int p);
void handleRoot();
void handleStatus();
void handleMove();
void handleStop();
void handleCal();
void handleResetAll();
void stopServo(int s);
void startServo(int s, bool forward);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- ESP32-C3 12-Pulley System Booting ---");
  
  pinMode(LED_PIN, OUTPUT);
  
  // Quick LED blink to show boot activity
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, LOW); delay(100);
    digitalWrite(LED_PIN, HIGH); delay(100);
  }

  // Load calibrated settings from flash
  loadSettings();

  // Let power stabilize before WiFi
  delay(1000);

  // Start WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.softAP("CableCar_12_Pulleys", "12345678");

  // Setup WebServer
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/move", HTTP_GET, handleMove);
  server.on("/api/stop", HTTP_GET, handleStop);
  server.on("/api/cal", HTTP_GET, handleCal);
  server.on("/api/resetall", HTTP_GET, handleResetAll);
  server.begin();
  Serial.println("Web server started successfully!");

  // Ensure all 12 servo pins are initialized to STOP (LOW) state immediately
  for (int i = 0; i < NUM_PULLEYS; i++) {
    stopServo(i);
  }
  Serial.println("All 12 pulleys initialized to STOP state.");
}

void loop() {
  server.handleClient();

  // Asynchronous timer state machine checks for all 12 pulleys
  for (int i = 0; i < NUM_PULLEYS; i++) {
    if (isMoving[i]) {
      if (millis() - moveStartTime[i] >= moveDuration[i]) {
        stopServo(i);
        currentStates[i] = (currentStates[i] == 2) ? 1 : 0; // 2 (MOVING_UP) -> 1 (UP), 3 (MOVING_DOWN) -> 0 (DOWN)
        isMoving[i] = false;
        Serial.printf("Pulley %d async movement completed.\n", i);
      }
    }
  }
}

// Load configurations from non-volatile flash storage
void loadSettings() {
  preferences.begin("pulleys", false);
  
  // Reset memory once to apply new defaults
  int version = preferences.getInt("v", 0);
  if (version < 18) {
    preferences.clear();
    preferences.putInt("v", 18);
  }

  for (int i = 0; i < NUM_PULLEYS; i++) {
    String kDurUp = "uDur" + String(i);
    String kDurDn = "dDur" + String(i);
    String kFwd = "fwd" + String(i);
    String kRev = "rev" + String(i);
    String kStop = "stp" + String(i);
    
    durationUp[i] = preferences.getULong(kDurUp.c_str(), defaultDurationsUp[i]);
    durationDown[i] = preferences.getULong(kDurDn.c_str(), defaultDurationsDown[i]);
    speedFwd[i] = preferences.getInt(kFwd.c_str(), defaultFwd[i]);
    speedRev[i] = preferences.getInt(kRev.c_str(), defaultRev[i]);
    speedStop[i] = preferences.getInt(kStop.c_str(), defaultStop[i]);
  }
  preferences.end();
  Serial.println("Settings successfully loaded from Preferences flash.");
}

// Save specific pulley configurations to flash
void saveSettings(int p) {
  preferences.begin("pulleys", false);
  preferences.putULong(("uDur" + String(p)).c_str(), durationUp[p]);
  preferences.putULong(("dDur" + String(p)).c_str(), durationDown[p]);
  preferences.putInt(("fwd" + String(p)).c_str(), speedFwd[p]);
  preferences.putInt(("rev" + String(p)).c_str(), speedRev[p]);
  preferences.putInt(("stp" + String(p)).c_str(), speedStop[p]);
  preferences.end();
  Serial.printf("Settings for Pulley %d saved to flash.\n", p);
}

// HTML, CSS, JS Portal content
const char HTML_CONTENT[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>12 Pulley Network Controller</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-grad: linear-gradient(135deg, #090d16 0%, #111428 100%);
            --glass-bg: rgba(255, 255, 255, 0.03);
            --glass-border: rgba(255, 255, 255, 0.08);
            --primary: #4f46e5;
            --success: #10b981;
            --warning: #f59e0b;
            --danger: #ef4444;
            --text: #f8fafc;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; }
        body {
            font-family: 'Outfit', sans-serif;
            background: var(--bg-grad);
            color: var(--text);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 30px 15px;
        }
        .container {
            width: 100%; max-width: 900px;
            background: var(--glass-bg);
            backdrop-filter: blur(20px);
            -webkit-backdrop-filter: blur(20px);
            border: 1px solid var(--glass-border);
            border-radius: 28px;
            padding: 30px;
            box-shadow: 0 30px 60px rgba(0, 0, 0, 0.4);
            text-align: center;
        }
        h1 {
            font-weight: 800; font-size: 28px; margin-bottom: 5px;
            background: linear-gradient(to right, #818cf8, #a78bfa);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        .subtitle { font-size: 14px; color: #94a3b8; margin-bottom: 30px; }
        
        /* Grid Layout */
        .pulley-grid {
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 20px;
            margin-bottom: 30px;
        }
        @media(max-width: 768px) {
            .pulley-grid { grid-template-columns: repeat(2, 1fr); }
        }
        @media(max-width: 480px) {
            .pulley-grid { grid-template-columns: 1fr; }
        }

        .pulley-card {
            background: rgba(0,0,0,0.25);
            border: 1px solid rgba(255,255,255,0.05);
            border-radius: 18px;
            padding: 18px;
            display: flex;
            flex-direction: column;
            gap: 12px;
            position: relative;
            transition: all 0.3s ease;
        }
        .pulley-card.green-group { border-left: 4px solid var(--success); }
        .pulley-card.yellow-group { border-left: 4px solid var(--warning); }
        .pulley-card.red-group { border-left: 4px solid var(--danger); }
        
        .pulley-card:hover {
            transform: translateY(-2px);
            border-color: rgba(255,255,255,0.15);
            box-shadow: 0 10px 20px rgba(0,0,0,0.2);
        }

        .pulley-info {
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .pulley-title { font-weight: 600; font-size: 15px; color: #cbd5e1; }
        .pulley-group-badge {
            font-size: 10px; font-weight: 800; padding: 3px 8px; border-radius: 20px;
            text-transform: uppercase; letter-spacing: 0.5px;
        }
        .green-badge { background: rgba(16, 185, 129, 0.15); color: var(--success); }
        .yellow-badge { background: rgba(245, 158, 11, 0.15); color: var(--warning); }
        .red-badge { background: rgba(239, 68, 68, 0.15); color: var(--danger); }

        .status-indicator {
            font-size: 12px; font-weight: 600; padding: 6px; border-radius: 8px;
            background: rgba(255, 255, 255, 0.02); text-align: center; color: #cbd5e1;
        }
        .status-indicator.up { background: rgba(99, 102, 241, 0.12); color: #818cf8; }
        .status-indicator.down { background: rgba(255,255,255,0.05); color: #94a3b8; }
        .status-indicator.moving-up {
            background: rgba(16, 185, 129, 0.12); color: var(--success);
            animation: pulse-green 1.5s infinite;
        }
        .status-indicator.moving-down {
            background: rgba(239, 68, 68, 0.12); color: var(--danger);
            animation: pulse-red 1.5s infinite;
        }

        @keyframes pulse-green {
            0% { opacity: 0.7; } 50% { opacity: 1; box-shadow: 0 0 10px rgba(16, 185, 129, 0.2); } 100% { opacity: 0.7; }
        }
        @keyframes pulse-red {
            0% { opacity: 0.7; } 50% { opacity: 1; box-shadow: 0 0 10px rgba(239, 68, 68, 0.2); } 100% { opacity: 0.7; }
        }

        .btn-group {
            display: flex; gap: 8px;
        }
        .btn-control {
            flex: 1; padding: 10px; font-size: 13px; font-weight: 700;
            border: 1px solid var(--glass-border); border-radius: 10px;
            background: rgba(255, 255, 255, 0.05); color: white;
            cursor: pointer; transition: all 0.2s ease; font-family: inherit;
        }
        .btn-control:hover:not(:disabled) { background: rgba(255,255,255,0.12); }
        .btn-control:active:not(:disabled) { transform: scale(0.96); }
        .btn-control:disabled { opacity: 0.3; cursor: not-allowed; }

        .btn-stop {
            width: 100%; padding: 8px; font-size: 12px; font-weight: 700;
            border: none; border-radius: 8px; background: rgba(239, 68, 68, 0.15); color: var(--danger);
            cursor: pointer; transition: all 0.2s ease; font-family: inherit;
        }
        .btn-stop:hover:not(:disabled) { background: rgba(239, 68, 68, 0.25); }
        .btn-stop:active:not(:disabled) { transform: scale(0.96); }
        
        .global-actions {
            display: flex; gap: 15px; margin-top: 10px; justify-content: center;
        }
        .btn-global {
            padding: 12px 24px; font-size: 14px; font-weight: 700;
            border: 1px solid var(--glass-border); border-radius: 12px;
            background: rgba(255,255,255,0.05); color: #cbd5e1;
            cursor: pointer; transition: all 0.2s ease; font-family: inherit;
        }
        .btn-global:hover { background: rgba(255,255,255,0.12); color: white; }
        .btn-global.stop-all { background: rgba(239, 68, 68, 0.2); border-color: rgba(239, 68, 68, 0.3); color: var(--danger); }
        .btn-global.stop-all:hover { background: rgba(239, 68, 68, 0.3); }

        .calib-trigger {
            margin-top: 25px; font-size: 13px; color: #94a3b8; cursor: pointer; text-decoration: underline;
        }
        
        #calib-panel {
            display: none; background: rgba(0,0,0,0.3); border: 1px solid var(--glass-border);
            border-radius: 20px; padding: 20px; margin-top: 20px; text-align: left;
        }
        .calib-row {
            display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; margin-bottom: 12px;
        }
        @media(max-width: 500px) {
            .calib-row { grid-template-columns: 1fr; }
        }
        .calib-field {
            display: flex; flex-direction: column; gap: 5px;
        }
        .calib-field label { font-size: 11px; color: #94a3b8; font-weight: 600; text-transform: uppercase; }
        .calib-field input, .calib-field select {
            background: rgba(0,0,0,0.4); border: 1px solid var(--glass-border); border-radius: 8px;
            padding: 8px 12px; color: white; font-family: inherit; font-size: 13px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>12 Pulley Network</h1>
        <div class="subtitle">Manual Up & Down Dispatcher</div>
        
        <div class="pulley-grid" id="grid">
            <!-- Dynamically generated via JS -->
        </div>

        <div class="global-actions">
            <button class="btn-global stop-all" onclick="stopAll()">Emergency Stop All</button>
            <button class="btn-global" onclick="resetAll()">Reset All to Bottom</button>
        </div>

        <div class="calib-trigger" onclick="toggleCalib()">Open Calibration Settings</div>
        
        <div id="calib-panel">
            <h3 style="font-size:16px;font-weight:600;margin-bottom:20px;color:#818cf8;border-bottom:1px solid rgba(255,255,255,0.05);padding-bottom:10px;">Pulley Calibration System (Saved to Flash)</h3>
            <div class="calib-row">
                <div class="calib-field">
                    <label>Select Pulley:</label>
                    <select id="calib-select" onchange="loadCalibFields()">
                        <!-- Dynamic list -->
                    </select>
                </div>
                <div class="calib-field">
                    <label>UP Travel Duration (ms):</label>
                    <input type="number" id="calib-dur-up">
                </div>
                <div class="calib-field">
                    <label>DOWN Travel Duration (ms):</label>
                    <input type="number" id="calib-dur-down">
                </div>
            </div>
            <div class="calib-row">
                <div class="calib-field">
                    <label>Forward (UP) Speed (us):</label>
                    <input type="number" id="calib-fwd">
                </div>
                <div class="calib-field">
                    <label>Reverse (DOWN) Speed (us):</label>
                    <input type="number" id="calib-rev">
                </div>
                <div class="calib-field">
                    <label>Calibrated Stop Pulse (us):</label>
                    <input type="number" id="calib-stop">
                </div>
            </div>
            <button onclick="saveCalib()" style="width:100%;padding:12px;font-size:14px;border-radius:10px;background:var(--success);color:white;font-weight:700;border:none;cursor:pointer;margin-top:10px;">Save Pulley Settings</button>
        </div>
    </div>

    <script>
        const NUM_PULLEYS = 12;
        let pulleysData = {};
        
        const labels = [
            "Q1 Green", "Q2 Green", "Q3 Green", "Q4 Green",
            "Q1 Yellow", "Q2 Yellow", "Q3 Yellow", "Q4 Yellow",
            "Q1 Red", "Q2 Red", "Q3 Red", "Q4 Red"
        ];

        const groupClasses = [
            "green-group", "green-group", "green-group", "green-group",
            "yellow-group", "yellow-group", "yellow-group", "yellow-group",
            "red-group", "red-group", "red-group", "red-group"
        ];

        const badgeClasses = [
            "green-badge", "green-badge", "green-badge", "green-badge",
            "yellow-badge", "yellow-badge", "yellow-badge", "yellow-badge",
            "red-badge", "red-badge", "red-badge", "red-badge"
        ];

        const badgeTexts = [
            "Green", "Green", "Green", "Green",
            "Yellow", "Yellow", "Yellow", "Yellow",
            "Red", "Red", "Red", "Red"
        ];

        window.addEventListener('load', () => {
            initDOM();
            updateStatus();
            setInterval(updateStatus, 3000); // background polling every 3s
        });

        function initDOM() {
            const grid = document.getElementById('grid');
            grid.innerHTML = "";
            
            const select = document.getElementById('calib-select');
            select.innerHTML = "";

            for (let i = 0; i < NUM_PULLEYS; i++) {
                // Populate grid
                const card = document.createElement('div');
                card.className = `pulley-card ${groupClasses[i]}`;
                card.id = `card-${i}`;
                card.innerHTML = `
                    <div class="pulley-info">
                        <span class="pulley-title">${labels[i]}</span>
                        <span class="pulley-group-badge ${badgeClasses[i]}">${badgeTexts[i]}</span>
                    </div>
                    <div class="status-indicator down" id="status-${i}">BOTTOM</div>
                    <div class="btn-group">
                        <button class="btn-control" id="btn-up-${i}" onclick="move(${i}, 'up')">UP</button>
                        <button class="btn-control" id="btn-down-${i}" onclick="move(${i}, 'down')">DOWN</button>
                    </div>
                    <button class="btn-stop" id="btn-stop-${i}" onclick="stop(${i})">STOP</button>
                `;
                grid.appendChild(card);

                // Populate select option
                const opt = document.createElement('option');
                opt.value = i;
                opt.innerText = labels[i];
                select.appendChild(opt);
            }
        }

        function updateStatus() {
            fetch('/api/status')
                .then(r => r.json())
                .then(d => {
                    pulleysData = d;
                    for (let i = 0; i < NUM_PULLEYS; i++) {
                        const statusIndicator = document.getElementById(`status-${i}`);
                        const btnUp = document.getElementById(`btn-up-${i}`);
                        const btnDown = document.getElementById(`btn-down-${i}`);
                        const btnStop = document.getElementById(`btn-stop-${i}`);
                        
                        const state = d.s[i];
                        
                        // State handling: 0=DOWN, 1=UP, 2=MOVING_UP, 3=MOVING_DOWN
                        if (state === 0) {
                            statusIndicator.innerText = "BOTTOM";
                            statusIndicator.className = "status-indicator down";
                            btnUp.disabled = false;
                            btnDown.disabled = true;
                            btnStop.disabled = true;
                        } else if (state === 1) {
                            statusIndicator.innerText = "TOP";
                            statusIndicator.className = "status-indicator up";
                            btnUp.disabled = true;
                            btnDown.disabled = false;
                            btnStop.disabled = true;
                        } else if (state === 2) {
                            statusIndicator.innerText = "MOVING UP...";
                            statusIndicator.className = "status-indicator moving-up";
                            btnUp.disabled = true;
                            btnDown.disabled = true;
                            btnStop.disabled = false;
                        } else if (state === 3) {
                            statusIndicator.innerText = "MOVING DOWN...";
                            statusIndicator.className = "status-indicator moving-down";
                            btnUp.disabled = true;
                            btnDown.disabled = true;
                            btnStop.disabled = false;
                        }
                    }
                    if (document.getElementById('calib-panel').style.display === 'block') {
                        // Refresh active calibration fields if open
                        loadCalibFields();
                    }
                })
                .catch(e => console.error(e));
        }

        function move(p, dir) {
            const statusIndicator = document.getElementById(`status-${p}`);
            const btnUp = document.getElementById(`btn-up-${p}`);
            const btnDown = document.getElementById(`btn-down-${p}`);
            const btnStop = document.getElementById(`btn-stop-${p}`);

            btnUp.disabled = true;
            btnDown.disabled = true;
            btnStop.disabled = false;

            if (dir === 'up') {
                statusIndicator.innerText = "MOVING UP...";
                statusIndicator.className = "status-indicator moving-up";
            } else {
                statusIndicator.innerText = "MOVING DOWN...";
                statusIndicator.className = "status-indicator moving-down";
            }

            fetch(`/api/move?p=${p}&dir=${dir}`)
                .then(r => r.json())
                .then(d => {
                    updateStatus();
                })
                .catch(e => {
                    console.error(e);
                    updateStatus();
                });
        }

        function stop(p) {
            fetch(`/api/stop?p=${p}`)
                .then(r => r.json())
                .then(d => {
                    updateStatus();
                })
                .catch(e => {
                    console.error(e);
                    updateStatus();
                });
        }

        function stopAll() {
            fetch('/api/stop?p=-1')
                .then(r => r.json())
                .then(d => {
                    updateStatus();
                })
                .catch(e => console.error(e));
        }

        function resetAll() {
            fetch('/api/resetall')
                .then(r => r.json())
                .then(d => {
                    updateStatus();
                })
                .catch(e => console.error(e));
        }

        function toggleCalib() {
            const panel = document.getElementById('calib-panel');
            if (panel.style.display === 'none' || panel.style.display === '') {
                panel.style.display = 'block';
                loadCalibFields();
            } else {
                panel.style.display = 'none';
            }
        }

        function loadCalibFields() {
            const p = parseInt(document.getElementById('calib-select').value);
            if (!pulleysData || !pulleysData.durUp) return;

            document.getElementById('calib-dur-up').value = pulleysData.durUp[p];
            document.getElementById('calib-dur-down').value = pulleysData.durDown[p];
            document.getElementById('calib-fwd').value = pulleysData.fwd[p];
            document.getElementById('calib-rev').value = pulleysData.rev[p];
            document.getElementById('calib-stop').value = pulleysData.stop[p];
        }

        function saveCalib() {
            const p = parseInt(document.getElementById('calib-select').value);
            const durUp = parseInt(document.getElementById('calib-dur-up').value);
            const durDown = parseInt(document.getElementById('calib-dur-down').value);
            const fwd = parseInt(document.getElementById('calib-fwd').value);
            const rev = parseInt(document.getElementById('calib-rev').value);
            const stop = parseInt(document.getElementById('calib-stop').value);

            if (isNaN(p) || isNaN(durUp) || isNaN(durDown) || isNaN(fwd) || isNaN(rev) || isNaN(stop)) {
                alert("Please fill in all calibration fields correctly.");
                return;
            }

            fetch(`/api/cal?p=${p}&durUp=${durUp}&durDown=${durDown}&fwd=${fwd}&rev=${rev}&stop=${stop}`)
                .then(r => r.json())
                .then(d => {
                    if (d.ok) {
                        alert(`Pulley ${labels[p]} settings saved successfully!`);
                        updateStatus();
                    }
                })
                .catch(e => console.error(e));
        }
    </script>
</body>
</html>
)rawhtml";

void handleRoot() {
  server.send(200, "text/html", HTML_CONTENT);
}

void handleStatus() {
  String j = "{\"s\":[";
  for (int i=0; i<NUM_PULLEYS; i++) { if(i) j+=","; j+=String(currentStates[i]); }
  j += "],\"durUp\":[";
  for (int i=0; i<NUM_PULLEYS; i++) { if(i) j+=","; j+=String(durationUp[i]); }
  j += "],\"durDown\":[";
  for (int i=0; i<NUM_PULLEYS; i++) { if(i) j+=","; j+=String(durationDown[i]); }
  j += "],\"fwd\":[";
  for (int i=0; i<NUM_PULLEYS; i++) { if(i) j+=","; j+=String(speedFwd[i]); }
  j += "],\"rev\":[";
  for (int i=0; i<NUM_PULLEYS; i++) { if(i) j+=","; j+=String(speedRev[i]); }
  j += "],\"stop\":[";
  for (int i=0; i<NUM_PULLEYS; i++) { if(i) j+=","; j+=String(speedStop[i]); }
  j += "]}";
  server.send(200, "application/json", j);
}

void handleMove() {
  if (!server.hasArg("p") || !server.hasArg("dir")) {
    server.send(400, "application/json", "{\"err\":1}"); return;
  }
  int p = server.arg("p").toInt();
  String dir = server.arg("dir");
  if (p < 0 || p >= NUM_PULLEYS || (dir != "up" && dir != "down")) {
    server.send(400, "application/json", "{\"err\":2}"); return;
  }
  
  // Re-entry / Busy check: if currently moving, reject new moves for THIS pulley
  if (isMoving[p]) {
    server.send(409, "application/json", "{\"err\":\"busy\"}"); return;
  }

  bool fwd = (dir == "up");
  int targetState = fwd ? 2 : 3; // 2 = MOVING_UP, 3 = MOVING_DOWN
  unsigned long dur = fwd ? durationUp[p] : durationDown[p];

  if (dur > 0) {
    isMoving[p] = true;
    moveStartTime[p] = millis();
    moveDuration[p] = dur;
    currentStates[p] = targetState;

    Serial.printf("Pulley %d starting movement: dir=%s, pin=%d, duration=%lu ms\n", 
                  p, dir.c_str(), servoPins[p], dur);
    startServo(p, fwd);
  }

  handleStatus();
}

void handleStop() {
  if (!server.hasArg("p")) {
    server.send(400, "application/json", "{\"err\":1}"); return;
  }
  int p = server.arg("p").toInt();

  // If p is -1, trigger EMERGENCY STOP ALL
  if (p == -1) {
    Serial.println("EMERGENCY STOP: Stopping all 12 pulleys immediately!");
    for (int i = 0; i < NUM_PULLEYS; i++) {
      if (isMoving[i]) {
        stopServo(i);
        isMoving[i] = false;
        // Keep current state as is (it remains whatever it was: moving states resolved to last state or stopped)
        currentStates[i] = (currentStates[i] == 2) ? 1 : 0;
      }
    }
    handleStatus();
    return;
  }

  if (p < 0 || p >= NUM_PULLEYS) {
    server.send(400, "application/json", "{\"err\":2}"); return;
  }

  if (isMoving[p]) {
    stopServo(p);
    isMoving[p] = false;
    currentStates[p] = (currentStates[p] == 2) ? 1 : 0;
    Serial.printf("Pulley %d manually stopped.\n", p);
  }

  handleStatus();
}

void handleCal() {
  if (!server.hasArg("p") || !server.hasArg("durUp") || !server.hasArg("durDown") || 
      !server.hasArg("fwd") || !server.hasArg("rev") || !server.hasArg("stop")) {
    server.send(400, "application/json", "{\"err\":1}"); return;
  }

  int p = server.arg("p").toInt();
  unsigned long durUpVal = server.arg("durUp").toInt();
  unsigned long durDnVal = server.arg("durDown").toInt();
  int fwdVal = server.arg("fwd").toInt();
  int revVal = server.arg("rev").toInt();
  int stopVal = server.arg("stop").toInt();

  if (p < 0 || p >= NUM_PULLEYS) {
    server.send(400, "application/json", "{\"err\":2}"); return;
  }

  // Update RAM values
  durationUp[p] = durUpVal;
  durationDown[p] = durDnVal;
  speedFwd[p] = fwdVal;
  speedRev[p] = revVal;
  speedStop[p] = stopVal;

  // Save to Preferences (Flash)
  saveSettings(p);

  // Instantly apply stop pulse calibration to physical servo if not moving
  if (!isMoving[p]) {
    stopServo(p);
  }

  server.send(200, "application/json", "{\"ok\":1}");
}

void handleResetAll() {
  Serial.println("Resetting all 12 pulleys to BOTTOM (DOWN) state...");
  
  for (int i = 0; i < NUM_PULLEYS; i++) {
    // If a pulley is currently moving, stop it first
    if (isMoving[i]) {
      stopServo(i);
      isMoving[i] = false;
    }
    
    // Always move downward for durationDown[i] to guarantee bottoming out
    Serial.printf("Resetting pulley %d: dir=DOWN, duration=%lu ms, pin=%d\n", 
                  i, durationDown[i], servoPins[i]);
    startServo(i, false); // false = DOWN/Reverse
    delay(durationDown[i]);
    stopServo(i);
    currentStates[i] = 0; // Set to bottom (0)
  }
  
  Serial.println("Reset all 12 complete.");
  handleStatus();
}

// Low-level servo operations: dynamic attach and detach to respect C3 channel limitations
void stopServo(int s) {
  // 1. Send STOP pulse first
  if (servos[s].attached()) {
    servos[s].writeMicroseconds(speedStop[s]);
  } else {
    // Temp attach to send the stop pulse, then detach
    servos[s].attach(servoPins[s], 300, 3000);
    servos[s].writeMicroseconds(speedStop[s]);
  }
  delay(15);
  
  // 2. Detach pin to free up the LEDC channel
  servos[s].detach();
  
  // 3. Force control line low to prevent any pot creep / noise
  pinMode(servoPins[s], OUTPUT);
  digitalWrite(servoPins[s], LOW);
}

void startServo(int s, bool forward) {
  int us = forward ? speedFwd[s] : speedRev[s];
  
  // 1. Configure pin as output and drag low
  pinMode(servoPins[s], OUTPUT);
  digitalWrite(servoPins[s], LOW);
  
  // 2. Dynamically attach using extended ranges
  servos[s].attach(servoPins[s], 300, 3000);
  
  // 3. Write active travel speed
  servos[s].writeMicroseconds(us);
}
