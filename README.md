# 12-Pulley Network Controller 🚡

Isang advanced na ESP32-C3 based controller para sa **12 independent cable car pulleys** (na nahahati sa Green, Yellow, at Red groups para sa Q1, Q2, Q3, at Q4). Nagho-host ito ng local Wi-Fi Access Point at nagpapatakbo ng HTTP server na may interactive at premium na Web Dispatch Portal para sa manual UP/DOWN control at live persistent calibration.

---

## 🏗️ Hardware Architecture & Pin Assignments

Upang mapagana ang 12 independent pulleys gamit ang ESP32-C3, ang mga servos ay nakakonekta sa mga sumusunod na GPIO pins:

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

Ang ESP32-C3 ay may hardware limit na **6 LEDC PWM channels** lamang. Upang patakbuhin ang 12 servo motors nang sabay-sabay at ligtas:
1. **Dynamic Attach**: Kapag pinindot ang **UP** o **DOWN** button para sa isang pulley, doon pa lamang ito ia-attach sa software (`servos[p].attach()`) upang kumuha ng LEDC channel.
2. **Signal Teardown & Detach**: Matapos ang travel duration (o kapag pinindot ang **STOP**), ang servo ay sumusulat ng stop pulse, tinatanggal sa GPIO (`servos[p].detach()`), at ang pin ay hinahila sa **LOW (0V)**.
3. **Mga Benepisyo**:
   * Walang limitasyon sa bilang ng servos na kayang kontrolin.
   * Ganap na tinatanggal ang signal drift o motor creep (hindi gigising ang motor kapag stationary).
   * Mas mababa ang konsumo sa kuryente.

---

## 🚦 Default Timing Configuration

Naka-embed sa controller ang mga sumusunod na default travel durations:

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

Kumonekta sa Wi-Fi AP ng board:
* **SSID**: `CableCar_12_Pulleys`
* **Password**: `12345678`
* **Access Portal**: `http://192.168.4.1`

### 1. Serves Controller Interface
* **Endpoint**: `GET /`
* **Response**: Responsive Glassmorphism dashboard na may control panels at calibration system.

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
* **Behavior**: Pinapatakbo ang pulley `p` sa tinukoy na direksyon batay sa naka-save na duration at speed.

### 4. Stop Pulley / Emergency Stop
* **Endpoint**: `GET /api/stop?p=<index | -1>`
* **Behavior**: Patigilin ang isang pulley. Kapag `p=-1`, ito ay mag-ti-trigger ng **Emergency Stop All** (sabay na hihinto ang lahat ng 12 motors).

### 5. Persistent Calibration Config
* **Endpoint**: `GET /api/cal?p=<0..11>&durUp=<ms>&durDown=<ms>&fwd=<us>&rev=<us>&stop=<us>`
* **Behavior**: Sinesave ang bagong configurations ng pulley `p` sa Preferences (Flash). Mananatili itong naka-save kahit patayin o i-reboot ang ESP32.

### 6. Reset All Pulleys
* **Endpoint**: `GET /api/resetall`
* **Behavior**: Ibababa ang lahat ng pulleys patungong BOTTOM state sequentially para sa kaligtasan ng mechanical assembly.

---

## 🚀 Setup & Upload Instructions

1. Buksan ang folder na ito sa **VS Code** na may **PlatformIO extension**.
2. Ikonekta ang ESP32-C3 sa inyong computer gamit ang USB.
3. I-click ang **Upload** (o `Ctrl+Alt+U`) para i-flash ang firmware.
4. I-reboot ang device at kumonekta sa SSID ng portal.
