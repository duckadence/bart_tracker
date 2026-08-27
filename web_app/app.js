const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

let characteristic = null;

const status = document.getElementById("status");
const connectButton = document.getElementById("connect");
const controls = document.getElementById("controls");

const wifiSSID = document.getElementById("wifiSSID");
const wifiPass = document.getElementById("wifiPass");
const providerSelect = document.getElementById("provider");
const apiKey = document.getElementById("apiKey");
const apiUrl = document.getElementById("apiUrl");
const stationNum = document.getElementById("stationNum");
const stationCode = document.getElementById("stationCode");

// Station options map
const stationOptions = {
  bart: [
    { code: "EMBR", name: "Embarcadero" },
    { code: "MONT", name: "Montgomery St." },
    { code: "POWL", name: "Powell St." }
  ],
  muni: [
    { code: "16345", name: "Market St & 5th St" },
    { code: "13204", name: "California St & Davis St" }
  ]
};

// Update station dropdown when provider changes
providerSelect.addEventListener("change", () => {
  if (providerSelect.value === "custom") {
    apiUrl.classList.remove("hidden");
  } else {
    apiUrl.classList.add("hidden");
  }
  
  // Clear and repopulate station dropdown
  stationCode.innerHTML = '<option value="">Select a Station</option>';
  const options = stationOptions[providerSelect.value] || [];
  options.forEach(opt => {
    const el = document.createElement("option");
    el.value = opt.code;
    el.textContent = opt.name;
    stationCode.appendChild(el);
  });
});

connectButton.addEventListener("click", connect);
document.getElementById("saveWifi").addEventListener("click", () => {
  sendBLECommand(`WIFI_SSID=${wifiSSID.value}`);
  setTimeout(() => sendBLECommand(`WIFI_PASS=${wifiPass.value}`), 200);
});
document.getElementById("connectWifi").addEventListener("click", () => sendBLECommand("WIFI_CONNECT"));

document.getElementById("saveProvider").addEventListener("click", () => {
  sendBLECommand(`PROVIDER=${providerSelect.value}`);
  if (apiKey.value) setTimeout(() => sendBLECommand(`API_KEY=${apiKey.value}`), 200);
  if (providerSelect.value === "custom" && apiUrl.value) {
    setTimeout(() => sendBLECommand(`API_URL=${apiUrl.value}`), 400);
  }
});

document.getElementById("saveStation").addEventListener("click", () => {
  const slot = stationNum.value;
  const code = stationCode.value.trim();
  if (slot >= 1 && slot <= 10 && code) {
    sendBLECommand(`STATION_${slot}=${code}`);
  } else {
    status.textContent = "Enter a valid slot (1-10) and station code.";
  }
});

document.getElementById("clearStations").addEventListener("click", () => sendBLECommand("CLEAR_STATIONS"));

async function connect() {
  if (!navigator.bluetooth) {
    status.textContent = "Web Bluetooth is not supported on this browser.";
    return;
  }

  try {
    status.textContent = "Searching for ESP32...";
    const device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }]
    });

    status.textContent = "Connecting...";
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(SERVICE_UUID);
    characteristic = await service.getCharacteristic(CHARACTERISTIC_UUID);

    status.textContent = "Connected to " + (device.name || "ESP32");
    controls.classList.remove("hidden");

    device.addEventListener("gattserverdisconnected", () => {
      characteristic = null;
      controls.classList.add("hidden");
      status.textContent = "Disconnected";
    });

  } catch (error) {
    console.error(error);
    status.textContent = "Connection failed.";
  }
}

async function sendBLECommand(cmd) {
  if (!characteristic) return;

  try {
    const encoder = new TextEncoder();
    await characteristic.writeValue(encoder.encode(cmd));
    status.textContent = `Sent: ${cmd}`;
  } catch (error) {
    console.error(error);
    status.textContent = "Failed to send BLE command.";
  }
}
