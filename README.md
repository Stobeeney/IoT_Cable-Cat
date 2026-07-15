# 12-Pulley Network Controller 🚡

An advanced ESP32-C3 based controller for **12 independent cable car pulleys** (divided into Green, Yellow, and Red groups for Q1, Q2, Q3, and Q4). The controller hosts a local Wi-Fi Access Point and runs an HTTP server with an interactive, premium Web Dispatch Portal for manual UP/DOWN control and live persistent calibration.

---

## 🏗️ Hardware Architecture & Pin Assignments

To operate the 12 independent pulleys using the ESP32-C3, the servos are connected to the following GPIO pins:

### Wiring Connection Table

| Pulley Identifier | Group | ESP32-C3 GPIO Pin | Function / Signal |
| :--- | :---: | :---: | :--- |
| **Q1 Green** | Green | **GPIO 1** | Dynamic PWM Control |
| **Q2 Green** | Green | **GPIO 2** | Dynamic PWM Control |
| **Q3 Green** | Green | **GPIO 3** | Dynamic PWM Control |
| **Q4 Green** | Green | **GPIO 4** | Dynamic PWM Control |
| **Q1 Yellow**| Yellow| **GPIO 5** | Dynamic PWM Control |
| **Q2 Yellow**| Yellow| **GPIO 6** | Dynamic PWM Control |
| **Q3 Yellow**| Yellow| **GPIO 7** | Dynamic PWM Control |
| **Q4 Yellow**| Yellow| **GPIO 8** | Dynamic PWM Control |
| **Q1 Red**   | Red   | **GPIO 9** | Dynamic PWM Control |
| **Q2 Red**   | Red   | **GPIO 10**| Dynamic PWM Control |
| **Q3 Red**   | Red   | **GPIO 20**| Dynamic PWM Control |
| **Q4 Red**   | Red   | **GPIO 21**| Dynamic PWM Control |
| **Built-in LED**| Status| **GPIO 8** | Boot indicator flash |

---

## ⚡ Dynamic Attach/Detach (LEDC Channel Optimization)

The ESP32-C3 has a hardware limitation of only **6 LEDC PWM channels**. To run 12 servo motors safely and concurrently:
1. **Dynamic Attach**: When the **UP** or **DOWN** button is clicked for a specific pulley, it dynamically attaches the servo in software (`servos[p].attach()`) to allocate an active LEDC channel on demand.
2. **Signal Teardown & Detach**: After the travel duration completes (or when the **STOP** button is pressed), the servo writes a stop pulse, detaches from the GPIO (`servos[p].detach()`), and pulls the pin hard **LOW (0V)**.
3. **Key Benefits**:
   * No limit on the number of servos that can be controlled.
   * Completely eliminates signal drift or motor creep (motor stays fully stationary when not moving).
   * Significantly lower power consumption.

---

## 🚦 Default Timing Configuration

The controller is embedded with the following default travel durations:

| Pulley Index | Pulley Name | Default UP Duration | Default DOWN Duration | Custom Calibrations |
| :---: | :--- | :---: | :---: | :--- |
| **0** | Q1 Green | 7.0s (`7000` ms) | 7.0s (`7000` ms) | Standard |
| **1** | Q2 Green | 3.0s (`3000` ms) | 3.0s (`3000` ms) | Standard |
| **2** | Q3 Green | 7.0s (`7000` ms) | 7.0s (`7000` ms) | Standard |
| **3** | Q4 Green | 3.0s (`3000` ms) | 3.0s (`3000` ms) | Standard |
| **4** | Q1 Yellow | 16.0s (`16000` ms)| 1.5s (`1500` ms) | Reverse Speed = `400` us |
| **5** | Q2 Yellow | 4.0s (`4000` ms) | 4.0s (`4000` ms) | Standard |
| **6** | Q3 Yellow | 1.5s (`1500` ms) | 1.5s (`1500` ms) | Standard |
| **7** | Q4 Yellow | 2.0s (`2000` ms) | 2.0s (`2000` ms) | Standard |
| **8..11**| Q1-Q4 Red | 0.3s (`300` ms) | 0.3s (`300` ms) | Standard |

---

## 🌐 Web API Endpoints

Connect to the board's Wi-Fi Access Point:
* **SSID**: `CableCar_12_Pulleys`
* **Password**: `12345678`
* **Access Portal**: `http://192.168.4.1`

### 1. Serves Controller Interface
* **Endpoint**: `GET /`
* **Response**: Responsive Glassmorphism dashboard with control panels and the calibration system.

### 2. Live Status
* **Endpoint**: `GET /api/status`
* **Response**: `application/json`
```json
{
  "s": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
  "durUp": [7000, 3000, 7000, 3000, 16000, 4000, 1500, 2000, 300, 300, 300, 300],
  "durDown": [7000, 3000, 7000, 3000, 1500, 4000, 1500, 2000, 300, 300, 300, 300],
  "fwd": [1200, 1000, 2000, 3000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000],
  "rev": [1800, 3000, 1000, 400, 400, 2000, 2000, 2000, 2000, 2000, 2000, 2000],
  "stop": [1500, 1500, 1500, 800, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500]
}
```
*(`s` represents state: 0=BOTTOM, 1=TOP, 2=MOVING_UP, 3=MOVING_DOWN).*

### 3. Move Pulley
* **Endpoint**: `GET /api/move?p=<0..11>&dir=<up|down>`
* **Behavior**: Runs pulley `p` in the specified direction based on saved duration and speed parameters.

### 4. Stop Pulley / Emergency Stop
* **Endpoint**: `GET /api/stop?p=<index | -1>`
* **Behavior**: Stops a specific pulley. If `p=-1`, it triggers the **Emergency Stop All** sequence (halting all 12 motors concurrently).

### 5. Persistent Calibration Config
* **Endpoint**: `GET /api/cal?p=<0..11>&durUp=<ms>&durDown=<ms>&fwd=<us>&rev=<us>&stop=<us>`
* **Behavior**: Saves the new configurations of pulley `p` to Preferences (Flash). These configurations persist across board restarts and power cycles.

### 6. Reset All Pulleys
* **Endpoint**: `GET /api/resetall`
* **Behavior**: Automatically moves all pulleys to the BOTTOM state sequentially for mechanical assembly safety.

---

## 🚀 Setup & Upload Instructions

1. Open this folder in **VS Code** equipped with the **PlatformIO extension**.
2. Connect the ESP32-C3 to your computer via USB.
3. Click **Upload** (or use the shortcut `Ctrl+Alt+U`) to flash the firmware.
4. Reboot the device and connect to the portal's SSID.
