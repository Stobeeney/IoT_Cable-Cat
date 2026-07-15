
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

#define LED_PIN 8

// --- Configuration Constants ---
const int NUM_SERVOS = 4;

// GPIO assignments for ESP32-C3
const int servoPins[NUM_SERVOS] = {1, 2, 3, 4};

// Servo continuous rotation speed constants (in microseconds)
const int SERVO_STOP = 1500;
const int SPEED_FORWARD[NUM_SERVOS] = {1600, 1600, 1600, 1600};
const int SPEED_REVERSE[NUM_SERVOS] = {1400, 1400, 1400, 1400};

// Calibrated stop microseconds for each servo
int stopMicroseconds[NUM_SERVOS] = {1500, 1500, 1500, 1500};

// Travel Durations (in milliseconds)
unsigned long durationGreen = 5000; // Default to 5 seconds as requested
unsigned long durationYellow = 2500;

// Station States: 0=RED, 1=YELLOW, 2=GREEN
#define STATE_RED 0
#define STATE_YELLOW 1
#define STATE_GREEN 2

// State variables
Servo servos[NUM_SERVOS];
int currentStations[NUM_SERVOS] = {STATE_RED, STATE_RED, STATE_RED, STATE_RED};

// Asynchronous Movement State Machine
bool isMoving = false;
int movingServo = -1;
int movingTarget = -1;
unsigned long moveStartTime = 0;
unsigned long moveDuration = 0;

// Create WebServer on port 80
WebServer server(80);

// HTML, CSS, JS Portal content
const char HTML_CONTENT[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Cable Car Controller</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-grad: linear-gradient(135deg, #0f172a 0%, #1e1b4b 100%);
            --glass-bg: rgba(255, 255, 255, 0.05);
            --glass-border: rgba(255, 255, 255, 0.1);
            --primary: #6366f1;
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
            justify-content: center;
            padding: 20px;
        }
        .container {
            width: 100%; max-width: 480px;
            background: var(--glass-bg);
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
            border: 1px solid var(--glass-border);
            border-radius: 24px;
            padding: 30px;
            box-shadow: 0 20px 40px rgba(0, 0, 0, 0.3);
            text-align: center;
        }
        h1 {
            font-weight: 800; font-size: 24px; margin-bottom: 5px;
            background: linear-gradient(to right, #a5b4fc, #818cf8);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        .subtitle { font-size: 14px; color: #94a3b8; margin-bottom: 30px; }
        .visual-area {
            display: grid; grid-template-columns: repeat(4, 1fr); gap: 15px;
            height: 220px; margin-bottom: 30px;
            background: rgba(0, 0, 0, 0.2); border-radius: 16px; padding: 15px; position: relative;
        }
        .track {
            position: relative; background: rgba(255, 255, 255, 0.03);
            border-radius: 12px; display: flex; flex-direction: column;
            justify-content: space-between; align-items: center; padding: 15px 0;
            overflow: hidden; border: 1px solid rgba(255, 255, 255, 0.05);
        }
        .track.active { background: rgba(99, 102, 241, 0.08); border-color: rgba(99, 102, 241, 0.2); }
        .track::before {
            content: ''; position: absolute; top: 20px; bottom: 20px; left: 50%; width: 2px;
            background: linear-gradient(to top, rgba(255,255,255,0.05) 0%, rgba(255,255,255,0.2) 50%, rgba(255,255,255,0.05) 100%);
            border-left: 1px dashed rgba(255, 255, 255, 0.25); transform: translateX(-50%); z-index: 1;
        }
        .track.active::before {
            border-left-color: rgba(99, 102, 241, 0.5);
            background: linear-gradient(to top, rgba(99, 102, 241, 0.1) 0%, rgba(99, 102, 241, 0.6) 50%, rgba(99, 102, 241, 0.1) 100%);
        }
        .station-label { font-size: 10px; font-weight: 800; text-transform: uppercase; letter-spacing: 0.5px; z-index: 2; }
        .label-green { color: var(--success); }
        .label-yellow { color: var(--warning); }
        .label-red { color: var(--danger); }
        .track-name { position: absolute; bottom: -22px; font-size: 11px; font-weight: 600; color: #cbd5e1; }
        .cable-car {
            position: absolute; width: 24px; height: 24px; background: #475569; border-radius: 6px;
            display: flex; align-items: center; justify-content: center;
            font-size: 10px; font-weight: bold; color: white;
            box-shadow: 0 4px 10px rgba(0,0,0,0.5);
            bottom: 12px; left: 50%; transform: translateX(-50%); z-index: 10;
        }
        .cable-car::after {
            content: ''; position: absolute; top: -8px; left: 50%;
            width: 2px; height: 8px; background: #94a3b8; transform: translateX(-50%);
        }
        .cable-car.green-dest { bottom: 165px; background: var(--success); box-shadow: 0 4px 15px rgba(16, 185, 129, 0.4); }
        .cable-car.yellow-dest { bottom: 90px; background: var(--warning); box-shadow: 0 4px 15px rgba(245, 158, 11, 0.4); }
        .status-box {
            background: rgba(255, 255, 255, 0.02); border: 1px solid var(--glass-border);
            border-radius: 16px; padding: 15px; margin-bottom: 25px;
            display: flex; justify-content: space-around;
        }
        .status-item { display: flex; flex-direction: column; align-items: center; }
        .status-value { font-size: 24px; font-weight: 800; color: #818cf8; margin-top: 5px; }
        .status-title { font-size: 11px; text-transform: uppercase; color: #94a3b8; letter-spacing: 1px; }
        .pulley-selector { display: flex; justify-content: space-between; gap: 8px; margin-bottom: 20px; }
        .btn-pulley {
            flex: 1; padding: 12px; font-size: 13px; font-weight: 700;
            border: 1px solid var(--glass-border); border-radius: 10px;
            background: rgba(255, 255, 255, 0.03); color: #94a3b8;
            cursor: pointer; transition: all 0.3s ease; font-family: 'Outfit', sans-serif;
        }
        .btn-pulley.active {
            background: linear-gradient(135deg, #6366f1 0%, #4f46e5 100%);
            color: white; border-color: #6366f1; box-shadow: 0 4px 15px rgba(99, 102, 241, 0.3);
        }
        .action-buttons { display: flex; flex-direction: column; gap: 12px; }
        .btn-action {
            width: 100%; padding: 14px; font-size: 15px; font-weight: 700;
            border: none; border-radius: 12px; color: white; cursor: pointer;
            transition: all 0.2s ease; font-family: 'Outfit', sans-serif;
            display: flex; align-items: center; justify-content: center; gap: 8px;
            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.2);
        }
        .green-btn { background: var(--success); box-shadow: 0 4px 15px rgba(16, 185, 129, 0.25); }
        .yellow-btn { background: var(--warning); box-shadow: 0 4px 15px rgba(245, 158, 11, 0.25); }
        .red-btn { background: var(--danger); box-shadow: 0 4px 15px rgba(239, 68, 68, 0.25); }
        .btn-action:active:not(:disabled) { transform: scale(0.98); }
        .btn-action:disabled, .btn-pulley:disabled { opacity: 0.5; cursor: not-allowed; transform: none !important; }
        .reset-area { display: flex; gap: 10px; margin-top: 15px; }
        .btn-reset {
            flex: 1; padding: 12px; font-size: 13px; font-weight: 700;
            border: 1px solid rgba(148, 163, 184, 0.3); border-radius: 12px;
            background: rgba(148, 163, 184, 0.08); color: #94a3b8;
            cursor: pointer; transition: all 0.2s ease; font-family: 'Outfit', sans-serif;
        }
        .btn-reset:active:not(:disabled) { transform: scale(0.98); background: rgba(148, 163, 184, 0.15); }
        .btn-reset:disabled { opacity: 0.5; cursor: not-allowed; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Mountain Cable Car</h1>
        <div class="subtitle">Manual Station Dispatcher</div>
        <div class="visual-area">
            <div class="track" id="track-0">
                <span class="station-label label-green">G</span>
                <span class="station-label label-yellow">Y</span>
                <span class="station-label label-red">R</span>
                <div class="cable-car" id="car-0">Q1</div>
                <div class="track-name">Q1</div>
            </div>
            <div class="track" id="track-1">
                <span class="station-label label-green">G</span>
                <span class="station-label label-yellow">Y</span>
                <span class="station-label label-red">R</span>
                <div class="cable-car" id="car-1">Q2</div>
                <div class="track-name">Q2</div>
            </div>
            <div class="track" id="track-2">
                <span class="station-label label-green">G</span>
                <span class="station-label label-yellow">Y</span>
                <span class="station-label label-red">R</span>
                <div class="cable-car" id="car-2">Q3</div>
                <div class="track-name">Q3</div>
            </div>
            <div class="track" id="track-3">
                <span class="station-label label-green">G</span>
                <span class="station-label label-yellow">Y</span>
                <span class="station-label label-red">R</span>
                <div class="cable-car" id="car-3">Q4</div>
                <div class="track-name">Q4</div>
            </div>
        </div>
        <div class="status-box">
            <div class="status-item">
                <span class="status-title">Selected Pulley</span>
                <span class="status-value" id="status-pulley">Q1</span>
            </div>
            <div class="status-item">
                <span class="status-title">Current Position</span>
                <span class="status-value" id="status-target">RED</span>
            </div>
        </div>
        <div class="pulley-selector">
            <button class="btn-pulley active" id="btn-p-0" onclick="selectPulley(0)">Q1</button>
            <button class="btn-pulley" id="btn-p-1" onclick="selectPulley(1)">Q2</button>
            <button class="btn-pulley" id="btn-p-2" onclick="selectPulley(2)">Q3</button>
            <button class="btn-pulley" id="btn-p-3" onclick="selectPulley(3)">Q4</button>
        </div>
        <div class="action-buttons">
            <button class="btn-action green-btn" onclick="moveTo(2)">Go to Green (Top)</button>
            <button class="btn-action yellow-btn" onclick="moveTo(1)">Go to Yellow (Middle)</button>
            <button class="btn-action red-btn" onclick="moveTo(0)">Go to Red (Bottom)</button>
        </div>
        <div class="reset-area">
            <button class="btn-reset" onclick="resetSelected()">Reset Selected (Q<span id="reset-q">1</span>)</button>
            <button class="btn-reset" onclick="resetAll()">Reset All to Red</button>
        </div>
        <div style="margin-top:25px;font-size:13px;color:#94a3b8;cursor:pointer;text-decoration:underline;" onclick="toggleCalib()">Open Calibration Settings</div>
        <div id="calib-panel" style="display:none;background:rgba(0,0,0,0.2);border:1px solid var(--glass-border);border-radius:16px;padding:15px;margin-top:15px;text-align:left;">
            <h3 style="font-size:14px;font-weight:600;margin-bottom:12px;color:#a5b4fc;text-align:center;">Delay Calibration (Seconds)</h3>
            <div style="display:flex;flex-direction:column;gap:10px;margin-bottom:15px;">
                <div style="display:flex;justify-content:space-between;align-items:center;">
                    <label style="font-size:12px;color:var(--success);font-weight:600;">Green (Top):</label>
                    <input type="number" id="input-green" step="0.1" style="width:70px;background:rgba(0,0,0,0.4);border:1px solid var(--glass-border);border-radius:6px;padding:4px 8px;color:white;font-family:inherit;text-align:center;">
                </div>
                <div style="display:flex;justify-content:space-between;align-items:center;">
                    <label style="font-size:12px;color:var(--warning);font-weight:600;">Yellow (Middle):</label>
                    <input type="number" id="input-yellow" step="0.1" style="width:70px;background:rgba(0,0,0,0.4);border:1px solid var(--glass-border);border-radius:6px;padding:4px 8px;color:white;font-family:inherit;text-align:center;">
                </div>
            </div>
            <h3 style="font-size:14px;font-weight:600;margin-top:15px;margin-bottom:12px;color:#a5b4fc;text-align:center;">Stop Calibration (Microseconds)</h3>
            <div style="display:grid;grid-template-columns:repeat(2,1fr);gap:10px;margin-bottom:15px;">
                <div style="display:flex;justify-content:space-between;align-items:center;gap:5px;">
                    <label style="font-size:11px;color:#cbd5e1;">Q1:</label>
                    <input type="number" id="input-s0" style="width:65px;background:rgba(0,0,0,0.4);border:1px solid var(--glass-border);border-radius:6px;padding:4px;color:white;font-family:inherit;text-align:center;">
                </div>
                <div style="display:flex;justify-content:space-between;align-items:center;gap:5px;">
                    <label style="font-size:11px;color:#cbd5e1;">Q2:</label>
                    <input type="number" id="input-s1" style="width:65px;background:rgba(0,0,0,0.4);border:1px solid var(--glass-border);border-radius:6px;padding:4px;color:white;font-family:inherit;text-align:center;">
                </div>
                <div style="display:flex;justify-content:space-between;align-items:center;gap:5px;">
                    <label style="font-size:11px;color:#cbd5e1;">Q3:</label>
                    <input type="number" id="input-s2" style="width:65px;background:rgba(0,0,0,0.4);border:1px solid var(--glass-border);border-radius:6px;padding:4px;color:white;font-family:inherit;text-align:center;">
                </div>
                <div style="display:flex;justify-content:space-between;align-items:center;gap:5px;">
                    <label style="font-size:11px;color:#cbd5e1;">Q4:</label>
                    <input type="number" id="input-s3" style="width:65px;background:rgba(0,0,0,0.4);border:1px solid var(--glass-border);border-radius:6px;padding:4px;color:white;font-family:inherit;text-align:center;">
                </div>
            </div>
            <button onclick="saveCalib()" style="width:100%;padding:10px;font-size:12px;border-radius:8px;background:var(--success);color:white;font-weight:600;border:none;cursor:pointer;">Save Calibration</button>
        </div>
    </div>
    <script>
        let activePulley = 0;
        let stations = [0, 0, 0, 0];
        let durG = 5000, durY = 2500;
        window.addEventListener('load', () => { updateStatus(); });
        function updateStatus() {
            fetch('/api/status').then(r=>r.json()).then(d => {
                stations = d.s; durG = d.g; durY = d.y;
                document.getElementById('input-green').value = (durG/1000).toFixed(1);
                document.getElementById('input-yellow').value = (durY/1000).toFixed(1);
                document.getElementById('input-s0').value = d.stops[0];
                document.getElementById('input-s1').value = d.stops[1];
                document.getElementById('input-s2').value = d.stops[2];
                document.getElementById('input-s3').value = d.stops[3];
                renderCars(); selectPulley(activePulley);
            }).catch(e => console.error(e));
        }
        function selectPulley(i) {
            activePulley = i;
            for (let j=0;j<4;j++) {
                document.getElementById('btn-p-'+j).classList.toggle('active', j===i);
                document.getElementById('track-'+j).classList.toggle('active', j===i);
            }
            const names = ["RED (Bottom)","YELLOW (Middle)","GREEN (Top)"];
            document.getElementById('status-pulley').innerText = "Q"+(i+1);
            document.getElementById('status-target').innerText = names[stations[i]];
            document.getElementById('reset-q').innerText = (i+1);
        }
        function renderCars() {
            for (let i=0;i<4;i++) {
                const car = document.getElementById('car-'+i);
                car.style.display='flex'; car.style.transition='none';
                if (stations[i]===0) car.className='cable-car';
                else if (stations[i]===1) car.className='cable-car yellow-dest';
                else car.className='cable-car green-dest';
            }
        }
        function toggleCalib() {
            const p = document.getElementById('calib-panel');
            p.style.display = p.style.display==='none'?'block':'none';
        }
        function saveCalib() {
            const g=parseFloat(document.getElementById('input-green').value);
            const y=parseFloat(document.getElementById('input-yellow').value);
            const s0=parseInt(document.getElementById('input-s0').value);
            const s1=parseInt(document.getElementById('input-s1').value);
            const s2=parseInt(document.getElementById('input-s2').value);
            const s3=parseInt(document.getElementById('input-s3').value);
            if(isNaN(g)||isNaN(y)||isNaN(s0)||isNaN(s1)||isNaN(s2)||isNaN(s3)){alert("Enter valid numbers.");return;}
            fetch('/api/cal?g='+Math.round(g*1000)+'&y='+Math.round(y*1000)+'&s0='+s0+'&s1='+s1+'&s2='+s2+'&s3='+s3)
                .then(r=>r.json()).then(d=>{if(d.ok){alert("Saved!");durG=d.g;durY=d.y;}})
                .catch(e=>console.error(e));
        }
        function moveTo(target) {
            const cur = stations[activePulley];
            if (cur===target) return;
            const gMs=parseFloat(document.getElementById('input-green').value)*1000;
            const yMs=parseFloat(document.getElementById('input-yellow').value)*1000;
            let dur=0;
            if(cur===0){dur=target===1?yMs:gMs;}
            else if(cur===1){dur=target===0?yMs:(gMs-yMs);}
            else{dur=target===0?gMs:(gMs-yMs);}
            
            // Disable UI
            document.querySelectorAll('.btn-pulley,.btn-action,.btn-reset').forEach(b=>b.disabled=true);
            const car=document.getElementById('car-'+activePulley);
            car.style.transition='all '+dur+'ms cubic-bezier(0.4,0,0.2,1)';
            car.offsetHeight;
            if(target===0)car.className='cable-car';
            else if(target===1)car.className='cable-car yellow-dest';
            else car.className='cable-car green-dest';
            
            fetch('/api/move?s='+activePulley+'&t='+target).then(r=>r.json()).then(d=>{
                setTimeout(()=>{
                    stations=d.s; renderCars(); selectPulley(activePulley);
                    document.querySelectorAll('.btn-pulley,.btn-action,.btn-reset').forEach(b=>b.disabled=false);
                },dur);
            }).catch(e=>{
                console.error(e);
                document.querySelectorAll('.btn-pulley,.btn-action,.btn-reset').forEach(b=>b.disabled=false);
                updateStatus();
            });
        }
        function resetSelected() {
            if (stations[activePulley]===0) return;
            moveTo(0);
        }
        function resetAll() {
            document.querySelectorAll('.btn-pulley,.btn-action,.btn-reset').forEach(b=>b.disabled=true);
            fetch('/api/resetall').then(r=>r.json()).then(d=>{
                stations=d.s; renderCars(); selectPulley(activePulley);
                document.querySelectorAll('.btn-pulley,.btn-action,.btn-reset').forEach(b=>b.disabled=false);
            }).catch(e=>{
                console.error(e);
                document.querySelectorAll('.btn-pulley,.btn-action,.btn-reset').forEach(b=>b.disabled=false);
                updateStatus();
            });
        }
    </script>
</body>
</html>
)rawhtml";

// Forward Declarations
void handleRoot();
void handleStatus();
void handleMove();
void handleCal();
void handleResetAll();
void stopServo(int s);
void startServo(int s, bool forward);

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- ESP32-C3 Cable Car System Booting ---");

    pinMode(LED_PIN, OUTPUT);

    // Quick LED blink to show we're alive
    for (int i = 0; i < 3; i++)
    {
        digitalWrite(LED_PIN, LOW);
        delay(150);
        digitalWrite(LED_PIN, HIGH);
        delay(150);
    }

    // Let power stabilize before WiFi
    delay(1000);

    // Start WiFi AP
    WiFi.mode(WIFI_AP);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.softAP("CableCar_Controller", "12345678");

    // Setup WebServer
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/move", HTTP_GET, handleMove);
    server.on("/api/cal", HTTP_GET, handleCal);
    server.on("/api/resetall", HTTP_GET, handleResetAll);
    server.begin();
    Serial.println("Web server started successfully!");

    // Initialize and stop all servos (hold lines low)
    Serial.println("Initializing servos...");
    for (int i = 0; i < NUM_SERVOS; i++)
    {
        stopServo(i);
    }
    Serial.println("Servos initialized to STOP state!");
}

void loop()
{
    server.handleClient();

    // Asynchronous timer state machine checks
    if (isMoving)
    {
        if (millis() - moveStartTime >= moveDuration)
        {
            stopServo(movingServo);
            currentStations[movingServo] = movingTarget;
            isMoving = false;
            Serial.printf("Async move done: Servo %d stopped at station %d\n", movingServo, movingTarget);
        }
    }
}

void handleRoot()
{
    server.send(200, "text/html", HTML_CONTENT);
}

void handleStatus()
{
    String j = "{\"s\":[";
    for (int i = 0; i < 4; i++)
    {
        if (i)
            j += ",";
        j += String(currentStations[i]);
    }
    j += "],\"g\":" + String(durationGreen) + ",\"y\":" + String(durationYellow);
    j += ",\"stops\":[";
    for (int i = 0; i < 4; i++)
    {
        if (i)
            j += ",";
        j += String(stopMicroseconds[i]);
    }
    j += "]}";
    server.send(200, "application/json", j);
}

void handleMove()
{
    if (!server.hasArg("s") || !server.hasArg("t"))
    {
        server.send(400, "application/json", "{\"err\":1}");
        return;
    }
    int s = server.arg("s").toInt();
    int target = server.arg("t").toInt();
    if (s < 0 || s >= NUM_SERVOS || target < 0 || target > 2)
    {
        server.send(400, "application/json", "{\"err\":2}");
        return;
    }

    // Re-entry check: if already moving, reject
    if (isMoving)
    {
        server.send(409, "application/json", "{\"err\":\"busy\"}");
        return;
    }

    int cur = currentStations[s];
    if (cur == target)
    {
        handleStatus();
        return;
    }

    unsigned long dur = 0;
    bool fwd = true;
    if (cur == STATE_RED)
    {
        dur = (target == STATE_YELLOW) ? durationYellow : durationGreen;
        fwd = true;
    }
    else if (cur == STATE_YELLOW)
    {
        if (target == STATE_RED)
        {
            dur = durationYellow;
            fwd = false;
        }
        else
        {
            dur = durationGreen - durationYellow;
            fwd = true;
        }
    }
    else
    {
        if (target == STATE_RED)
        {
            dur = durationGreen;
            fwd = false;
        }
        else
        {
            dur = durationGreen - durationYellow;
            fwd = false;
        }
    }

    if (dur > 0)
    {
        movingServo = s;
        movingTarget = target;
        moveStartTime = millis();
        moveDuration = dur;
        isMoving = true;
        currentStations[s] = target; // Update immediately so status response has correct destination

        Serial.printf("Servo %d: cur=%d, target=%d, dur=%d ms, dir=%s, pin=%d\n",
                      s, cur, target, dur, fwd ? "FORWARD" : "REVERSE", servoPins[s]);
        startServo(s, fwd);
    }
    else
    {
        currentStations[s] = target;
    }

    handleStatus();
}

void handleCal()
{
    if (server.hasArg("g") && server.hasArg("y"))
    {
        durationGreen = server.arg("g").toInt();
        durationYellow = server.arg("y").toInt();
        if (server.hasArg("s0"))
            stopMicroseconds[0] = server.arg("s0").toInt();
        if (server.hasArg("s1"))
            stopMicroseconds[1] = server.arg("s1").toInt();
        if (server.hasArg("s2"))
            stopMicroseconds[2] = server.arg("s2").toInt();
        if (server.hasArg("s3"))
            stopMicroseconds[3] = server.arg("s3").toInt();

        // Instantly update the physical stops for real-time calibration feedback
        for (int i = 0; i < NUM_SERVOS; i++)
        {
            stopServo(i);
        }

        String j = "{\"ok\":1,\"g\":" + String(durationGreen) + ",\"y\":" + String(durationYellow) + "}";
        server.send(200, "application/json", j);
    }
    else
    {
        server.send(400, "application/json", "{\"err\":1}");
    }
}

void handleResetAll()
{
    Serial.println("Resetting all servos to RED...");
    // Move each servo that is NOT at RED back to RED one by one (blocking reset is acceptable)
    for (int i = 0; i < NUM_SERVOS; i++)
    {
        if (currentStations[i] != STATE_RED)
        {
            unsigned long dur = 0;
            if (currentStations[i] == STATE_YELLOW)
                dur = durationYellow;
            else
                dur = durationGreen; // GREEN

            Serial.printf("Resetting servo %d: current=%d, dur=%d ms, pin=%d\n",
                          i, currentStations[i], dur, servoPins[i]);

            if (dur > 0)
            {
                startServo(i, false); // Move REVERSE
                delay(dur);
                stopServo(i); // Absolute stop
            }
            currentStations[i] = STATE_RED;
        }
    }
    Serial.println("Reset all complete.");
    handleStatus();
}

// Helper functions for absolute stop and smooth start
void stopServo(int s)
{
    servos[s].writeMicroseconds(stopMicroseconds[s]); // Send stop pulse
    delay(100);                                       // Allow physical motor to settle
    servos[s].detach();
    pinMode(servoPins[s], OUTPUT);
    digitalWrite(servoPins[s], LOW); // Force control line to 0V (no signal, no creep)
}

void startServo(int s, bool forward)
{
    pinMode(servoPins[s], OUTPUT);
    digitalWrite(servoPins[s], LOW);
    servos[s].attach(servoPins[s]);
    servos[s].writeMicroseconds(forward ? SPEED_FORWARD[s] : SPEED_REVERSE[s]);
}
