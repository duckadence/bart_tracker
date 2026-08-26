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

saveButton.disabled = true;


// ==================================================
// CONNECT TO ESP32
// ==================================================

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


// ==================================================
// DISCONNECT
// ==================================================

function disconnected() {

  characteristic = null;

  connectButton.disabled = false;
  connectButton.textContent = "Connect ESP32";

  saveButton.disabled = true;

  status.textContent = "Disconnected";
}


// ==================================================
// SEND BLE COMMAND
// ==================================================

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


// ==================================================
// SAVE CONFIGURATION
// ==================================================

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

  const line =
    document.getElementById("line-1").value;

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

  if (!line) {
    status.textContent =
      "Select a BART line.";
    return;
  }

  try {

    status.textContent =
      "Saving configuration...";

    // Create configuration object
    const config = {
      ssid: ssid,
      password: password,
      line: line,
      stations: stations
    };

    // Convert to JSON
    const command =
      "CONFIG=" + JSON.stringify(config);

    console.log("Configuration:", config);

    // Send to ESP32
    if (!await sendCommand(command))
      return;

    status.textContent =
      "Configuration saved. Connecting to Wi-Fi...";

  } catch (error) {

    console.error(error);

    status.textContent =
      "Failed to save configuration.";
  }
}
