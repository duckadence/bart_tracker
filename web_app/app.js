const SERVICE_UUID =
  "4fafc201-1fb5-459e-8fcc-c5c9c331914b";

const CHARACTERISTIC_UUID =
  "beb5483e-36e1-4688-b7f5-ea07361b26a8";

let characteristic = null;
let device = null;

const status = document.getElementById("status");
const connectButton = document.getElementById("connect");
const saveButton = document.getElementById("save");

connectButton.addEventListener("click", connect);
saveButton.addEventListener("click", saveConfiguration);

// --------------------------------------------------
// CONNECT
// --------------------------------------------------

async function connect() {

  if (!navigator.bluetooth) {
    status.textContent =
      "Web Bluetooth is not supported by this browser.";
    return;
  }

  try {

    status.textContent = "Searching for ESP32...";

    device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }]
    });

    status.textContent = "Connecting...";

    const server = await device.gatt.connect();

    const service =
      await server.getPrimaryService(SERVICE_UUID);

    characteristic =
      await service.getCharacteristic(CHARACTERISTIC_UUID);

    status.textContent =
      "Connected to " + (device.name || "ESP32");

    connectButton.textContent = "Connected";
    connectButton.disabled = true;
    saveButton.disabled = false;

    device.addEventListener(
      "gattserverdisconnected",
      disconnected
    );

  } catch (error) {

    console.error(error);
    status.textContent =
      "Connection cancelled or failed.";
  }
}

// --------------------------------------------------
// DISCONNECT
// --------------------------------------------------

function disconnected() {

  characteristic = null;

  connectButton.disabled = false;
  connectButton.textContent = "Connect ESP32";

  saveButton.disabled = true;

  status.textContent = "Disconnected";
}

// --------------------------------------------------
// SEND COMMAND
// --------------------------------------------------

async function sendCommand(command) {

  if (!characteristic) {
    status.textContent = "ESP32 is not connected.";
    return false;
  }

  try {

    const data =
      new TextEncoder().encode(command);

    await characteristic.writeValue(data);

    console.log("Sent:", command);

    return true;

  } catch (error) {

    console.error(error);

    status.textContent =
      "Failed to send configuration.";

    return false;
  }
}

// --------------------------------------------------
// SAVE CONFIGURATION
// --------------------------------------------------

async function saveConfiguration() {

  if (!characteristic) {
    status.textContent =
      "Connect to ESP32 first.";
    return;
  }

  const ssid =
    document.getElementById("wifi-ssid").value.trim();

  const password =
    document.getElementById("wifi-password").value;

  const stations = [
    document.getElementById("station-1").value,
    document.getElementById("station-2").value,
    document.getElementById("station-3").value
  ];

  if (!ssid) {
    status.textContent =
      "Enter a Wi-Fi network.";
    return;
  }

  status.textContent =
    "Saving configuration...";

  // Wi-Fi SSID
  if (!await sendCommand(
    "WIFI_SSID=" + ssid
  )) return;

  await delay(100);

  // Wi-Fi password
  if (!await sendCommand(
    "WIFI_PASS=" + password
  )) return;

  await delay(100);

  // Stations
  for (let i = 0; i < 3; i++) {

    if (!await sendCommand(
      `STATION_${i + 1}=${stations[i]}`
    )) return;

    await delay(100);
  }

  // Tell ESP32 to connect
  await delay(100);

  if (!await sendCommand(
    "WIFI_CONNECT"
  )) return;

  status.textContent =
    "Configuration saved. Connecting to Wi-Fi...";
}

// --------------------------------------------------
// DELAY
// --------------------------------------------------

function delay(ms) {
  return new Promise(resolve =>
    setTimeout(resolve, ms)
  );
}
