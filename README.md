# Mountain Cable Car Controller 🚡

An ESP32-C3 based 4-track Mountain Cable Car system with continuous rotation servos and an interactive, modern Web Control Portal. The controller hosts a local Wi-Fi Access Point and runs an HTTP server to provide asynchronous control, status monitoring, and live calibration capabilities.

---

## 🏗️ System Architecture & Specifications

### Hardware Platform
- **Microcontroller**: ESP32-C3 (configured for ESP32-C3 DevKitM-1)
- **Actuators**: 4x Continuous Rotation Servos (referred to as **Q1**, **Q2**, **Q3**, and **Q4**)
- **Driver Library**: `ESP32Servo` for PWM pulse-width modulation.
- **Status LED**: Configured on GPIO 8 (flashes on bootup).

### Wiring Connection Table

| Device / Module | ESP32-C3 GPIO | Function / Signal |
| :--- | :---: | :--- |
| **Servo Q1** | **GPIO 1** | PWM Signal Control |
| **Servo Q2** | **GPIO 2** | PWM Signal Control |
| **Servo Q3** | **GPIO 3** | PWM Signal Control |
| **Servo Q4** | **GPIO 4** | PWM Signal Control |
| **Built-in LED**| **GPIO 8** | Status Indication & Boot Flash |

---

## 🚦 Station States & Timing Logic

Each cable car moves between three physical stations, controlled by continuous-rotation speed pulses and duration timers.

### Station State Representation
- **State 0: RED** (Bottom / Origin)
- **State 1: YELLOW** (Middle / Midpoint)
- **State 2: GREEN** (Top / Peak)

### Default Timings & Speeds
- **Red to Yellow**: `2500 ms` (2.5 seconds)
- **Red to Green**: `5000 ms` (5 seconds)
- **Yellow to Green**: `2500 ms` (Calculated dynamically as `durationGreen - durationYellow`)
- **Servo PWM Speeds**:
  - `FORWARD` (Upward): `1600 μs` (Microseconds)
  - `REVERSE` (Downward): `1400 μs` (Microseconds)
  - `STOP` (Calibrated): `1500 μs` default (Configurable per-servo to counter drift)

---

## 🔒 Anti-Creep & Jitter Protection

Continuous rotation servos are highly sensitive to PWM signal noise, which can cause slow drifting ("creep") or micro-jitter when they are supposed to be stationary. To resolve this, this project implements a **zero-creep signal teardown process**:

1. **Active Driving**: When a movement is triggered, the microcontroller attaches the servo instance to the pin and writes the target microsecond speed pulse (`1600` for Forward, `1400` for Reverse).
2. **Asynchronous Non-blocking Timer**: The main loop polls the elapsed time against target durations using a state machine (`millis()`), allowing other requests and web features to remain active.
3. **Decoupled Termination**: Once the time expires:
   - The target `stopMicroseconds` pulse is written.
   - The servo is physically detached from the GPIO using `servos[s].detach()`.
   - The GPIO pin is set to `OUTPUT` and pulled hard `LOW` (`0V`) using `digitalWrite(servoPins[s], LOW)`.
   
This completely cuts off the signal line, guaranteeing the servos remain locked in place and noise-free.

---

## 🌐 Web API Documentation

The ESP32-C3 acts as a Wi-Fi Access Point:
- **SSID**: `CableCar_Controller`
- **Password**: `12345678`
- **Port**: `80` (HTTP)

All interactions are handled via RESTful API endpoints:

### 1. Serves Web Portal
- **Endpoint**: `GET /`
- **Response**: The responsive, dark-glassmorphism HTML panel.

### 2. Retrieve Status
- **Endpoint**: `GET /api/status`
- **Response**: `application/json`
```json
{
  "s": [0, 0, 0, 0],
  "g": 5000,
  "y": 2500,
  "stops": [1500, 1500, 1500, 1500]
}
```
*(`s` represents station positions for Q1–Q4, `g`/`y` are Green/Yellow timings, `stops` are current zero-drift calibrations).*

### 3. Asynchronous Dispatch Car
- **Endpoint**: `GET /api/move?s=<0..3>&t=<0..2>`
  - `s`: Servo Index (0 for Q1, 1 for Q2, etc.)
  - `t`: Target Station (0 = RED, 1 = YELLOW, 2 = GREEN)
- **Response**: Updated JSON status object.

### 4. Calibration Config
- **Endpoint**: `GET /api/cal?g=<ms>&y=<ms>&s0=<us>&s1=<us>&s2=<us>&s3=<us>`
  - `g`: Green duration in ms
  - `y`: Yellow duration in ms
  - `s0` to `s3`: Calibration stop pulse-width (μs) for Q1 through Q4
- **Response**: `{"ok":1, "g":..., "y":...}`

### 5. Return All to Base
- **Endpoint**: `GET /api/resetall`
- **Behavior**: Moves all cable cars currently at Yellow or Green back to Red sequentially (blocking reset safety sequence) and performs a hard stop.

---

## 🎨 Interactive Control Portal

The Web interface is fully responsive, designed with **modern glassmorphism UI aesthetics**, featuring:
- **Visual Tracks**: Real-time position tracking animations for all 4 cable cars.
- **Pulley Selectors**: Interactive tabs to switch focus between Q1, Q2, Q3, and Q4.
- **Action Dashboard**: Color-coded buttons matching the target stations (Green, Yellow, Red) to trigger movement.
- **Diagnostics & Calibration Drawer**: Allows operators to calibrate travel durations and adjust servo zero-points on the fly without re-flashing the firmware.

---

## 🚀 Setup & Build Instructions

### Prerequisites
- [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE Extension](https://platformio.org/).

### Compiling and Uploading
1. Clone or open the project folder in VS Code.
2. PlatformIO will automatically read `platformio.ini` and download the necessary ESP32 toolchains and libraries (e.g., `ESP32Servo`).
3. Connect the ESP32-C3 to your computer via USB (Default configuration points to `COM12`).
4. Click **PlatformIO: Build** or use the shortcut `Ctrl+Alt+B`.
5. Click **PlatformIO: Upload** or use the shortcut `Ctrl+Alt+U`.
6. Open your serial monitor at `115200` baud to see the system boot diagnostics.
