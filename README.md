# BART Tracker (Transit Tracker)

A ESP32‑based device that fetches real‑time BART train arrival/departure information for a user‑specified station and displays it via serial output. Configuration is performed over BLE using a companion web app.

## Features

- **BLE Configuration**: Set Wi‑Fi credentials, BART station abbreviation, and trigger actions (connect, clear) from a web browser using Web Bluetooth.
- **Automatic Wi‑Fi Reconnect**: The ESP32 will periodically try to reconnect if the connection drops.
- **Periodic BART Polling**: Every 30 seconds the device queries the BART API for predictions and prints a human‑readable list to the serial monitor.
- **Simple Serial Output**: Example:

```
Station: Embarcadero
Dublin/Pleasanton | East | 12 min
Daly City         | West | 15 min
...
```

- **Modular Firmware**: Separated concerns into BLE manager, Wi‑Fi manager, and BART client for easier maintenance.
- **Web App**: A small single‑page web application (HTML/CSS/JS) to configure the device. It works in any browser that supports Web Bluetooth (Chrome, Edge, Opera).

## Hardware

- ESP32 DevKit V1 (or any ESP32 with Bluetooth Classic/BLE)
- USB‑C cable for power and serial debugging

## Software Dependencies

### Firmware (PlatformIO)

- `NimBLE-Arduino` (BLE stack)
- `ArduinoJson` (used for optional features, though current XML parsing is manual)
- Arduino core for ESP32

### Web App

- Plain HTML, CSS, and vanilla JavaScript (no build step required)
- Uses the Web Bluetooth API

## Getting Started

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/bart_tracker.git
cd bart_tracker
```

### 2. Build and Upload Firmware

Using [PlatformIO](https://platformio.org/):

```bash
pio run --target upload
```

Open a serial monitor at 115200 baud to see logs.

### 3. Configure the Device

1. Open `web_app/index.html` in a compatible browser (Chrome recommended).
2. Click **Connect** and select your ESP32 device (advertises as "TransitTracker").
3. Once connected:
   - Enter your Wi‑Fi SSID and password, then click **Save Wi‑Fi**.
   - Click **Connect Wi‑Fi** to join the network.
   - Choose the BART station abbreviation (e.g., `EMBR` for Embarcadero) and click **Save Station**.
   - (Optional) Adjust provider, API key, etc., though the firmware currently only supports BART with the demo key.
4. The device will now start fetching predictions every 30 seconds and print them to serial.

### 4. Viewing Output

Open the serial monitor (PlatformIO: `pio device monitor`) to see the station name and upcoming trains.

## Project Structure

```
bart_tracker/
├─ firmware/
│  ├─ platformio.ini          # PlatformIO project definition
│  └─ src/
│     ├─ ble_manager.h/.cpp   # BLE advertising, command handling
│     ├─ wifi_manager.h/.cpp  # Wi‑Fi connection logic
│     ├─ bart_client.h/.cpp   # BART API request + lightweight XML parse
│     └─ transit_tracker_firmware.ino  # Main sketch
└─ web_app/
   ├─ index.html               # Main UI
   ├─ app.js                   # Bluetooth + UI logic
   ├─ style.css                # Simple styling
   ├─ data/stations.json       # Station lists for search fallback
   └─ *.html                   # Debug / test pages
```

## How It Works

1. **BLE Setup**: The ESP32 advertises a custom service/characteristic. The web app writes commands like:
   - `WIFI_SSID=<ssid>`
   - `WIFI_PASS=<password>`
   - `STATION=<abbr>`
   - `WIFI_CONNECT`
   - `GET_STATION` (returns `1:<abbr>` or `none`)
   - `CLEAR`
2. **Wi‑Fi**: After receiving credentials, the device can connect manually via `WIFI_CONNECT` or automatically on startup/reconnect.
3. **BART Polling**: When a station is set and Wi‑Fi is connected, the main loop calls `BartClient::fetchPredictions()` every 30 seconds, parses the XML, and prints each estimate.
4. **Serial Output**: Human‑readable lines are printed to `Serial` for debugging or display on an attached LCD (future extension).

## Customization

- **API Key**: The demo key `MW9S-E7SL-26DU-VV8V` is hard‑coded in `bart_client.h`. Replace with your own key if desired.
- **Polling Interval**: Change `FETCH_INTERVAL` in `transit_tracker_firmware.ino`.
- **Additional Providers**: The firmware currently only handles BART. Extend `BartClient` or create new providers as needed.

## Troubleshooting

- **Device Not Found**: Ensure Web Bluetooth is enabled and you are using a compatible browser.
- **No Serial Output**: Verify baud rate is 115200 and that `Serial.begin()` succeeded.
- **Wi‑Fi Connection Fails**: Double‑check SSID/password; ensure the ESP32 is within range.
- **BART Fetch Fails**: The device prints HTTP errors to serial. Check network connectivity and that the station abbreviation is valid.

## License

This project is licensed under the MIT License – see the [LICENSE](LICENSE) file for details.

## Acknowledgements

- BART API: https://api.bart.gov/
- NimBLE-Arduino library: https://github.com/h2zero/NimBLE-Arduino
- ArduinoJson: https://arduinojson.org/