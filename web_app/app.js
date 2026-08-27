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
let stationOptions = {
  bart: [
    { code: "EMBR", name: "Embarcadero" },
    { code: "MONT", name: "Montgomery St." },
    { code: "POWL", name: "Powell St." },
    { code: "CIVC", name: "Civic Center" },
    { code: "16TH", name: "16th St. Mission" },
    { code: "24TH", name: "24th St. Mission" },
    { code: "BALB", name: "Balboa Park" },
    { code: "BAYF", name: "Bay Fair" },
    { code: "CAST", name: "Castro Valley" },
    { code: "COLS", name: "Coliseum" },
    { code: "COLM", name: "Colma" },
    { code: "CONC", name: "Concord" },
    { code: "DALE", name: "Daly City" },
    { code: "DBRK", name: "Downtown Berkeley" },
    { code: "DELN", name: "El Cerrito del Norte" },
    { code: "PLZA", name: "El Cerrito Plaza" },
    { code: "FRMT", name: "Fremont" },
    { code: "FTVL", name: "Fruitvale" },
    { code: "GLEN", name: "Glen Park" },
    { code: "HAYW", name: "Hayward" },
    { code: "LAFY", name: "Lafayette" },
    { code: "LAKE", name: "Lake Merritt" },
    { code: "MCAR", name: "MacArthur" },
    { code: "MLBR", name: "Millbrae" },
    { code: "NBRK", name: "North Berkeley" },
    { code: "OAKL", name: "Oakland International Airport" },
    { code: "ORIN", name: "Orinda" },
    { code: "PHIL", name: "Pittsburg/Bay Point" },
    { code: "PCTR", name: "Pleasant Hill/Contra Costa Centre" },
    { code: "RICH", name: "Richmond" },
    { code: "ROCK", name: "Rockridge" },
    { code: "SANL", name: "San Leandro" },
    { code: "SSAN", name: "South San Francisco" },
    { code: "UCTY", name: "Union City" },
    { code: "WARM", name: "Warm Springs/South Fremont" },
    { code: "WDUB", name: "West Dublin/Pleasanton" },
    { code: "WOAK", name: "West Oakland" }
  ],
  muni: [
    { code: "16345", name: "Bus - Market St & 5th St" },
    { code: "13204", name: "Bus - California St & Davis St" },
    { code: "16001", name: "Rail - Civic Center" },
    { code: "16002", name: "Rail - Hayes Valley" },
    { code: "16003", name: "Rail - Van Ness" },
    { code: "16004", name: "Rail - Chinatown" },
    { code: "16005", name: "Rail - North Beach" },
    { code: "16006", name: "Rail - Fisherman's Wharf" },
    { code: "16007", name: "Rail - Embarcadero" },
    { code: "16008", name: "Rail - Ferry Building" },
    { code: "16009", name: "Rail - Transbay Terminal" },
    { code: "16010", name: "Bus - Mission & 16th" },
    { code: "16011", name: "Bus - Mission & 24th" },
    { code: "16012", name: "Rail - Balboa Park" },
    { code: "16013", name: "Rail - Daly City" }
  ],
  caltrain: [
    { code: "70012", name: "San Francisco" },
    { code: "70022", name: "22nd Street" },
    { code: "70032", name: "Bayshore" },
    { code: "70042", name: "South San Francisco" },
    { code: "70052", name: "San Bruno" },
    { code: "70062", name: "Millbrae" },
    { code: "70072", name: "Broadway" },
    { code: "70082", name: "Burlingame" },
    { "code": "70092", "name": "San Mateo" },
    { "code": "70102", "name": "Hayward Park" },
    { "code": "70112", "name": "Hillsdale" },
    { "code": "70122", "name": "Belmont" },
    { "code": "70132", "name": "San Carlos" },
    { "code": "70142", "name": "Redwood City" },
    { "code": "70152", "name": "Atherton" },
    { "code": "70162", "name": "Menlo Park" },
    { "code": "70172", "name": "Palo Alto" },
    { "code": "70182", "name": "California Avenue" },
    { "code": "70192", "name": "San Antonio" },
    { "code": "70202", "name": "Mountain View" },
    { "code": "70212", "name": "Sunnyvale" },
    { "code": "70222", "name": "Lawrence" },
    { "code": "70232", "name": "Santa Clara" },
    { "code": "70242", "name": "College Park" },
    { "code": "70252", "name": "San Jose Diridon" }
  ],
  vta: [
    { "code": "70211", "name": "Rail - Mountain View" },
    { "code": "70221", "name": "Rail - San Antonio" },
    { "code": "70231", "name": "Rail - Lawrence" },
    { "code": "70241", "name": "Rail - Sunnyvale" },
    { "code": "70251", "name": "Rail - Santa Clara" },
    { "code": "70261", "name": "Rail - College Park" },
    { "code": "70271", "name": "Rail - San Jose Diridon" },
    { "code": "70281", "name": "Rail - Tamien" },
    { "code": "70291", "name": "Bus - Capitol" },
    { "code": "70301", "name": "Bus - Blossom Hill" },
    { "code": "70311", "name": "Bus - Morgan Hill" },
    { "code": "70321", "name": "Bus - Gilroy" }
  ],
  actransit: [
    { "code": "50001", "name": "Downtown Berkeley" },
    { "code": "50002", "name": "MacArthur" }
  ]
};

// ... existing code ...

document.addEventListener('DOMContentLoaded', () => {
  // Load external stations
  try {
    fetch('data/stations.json')
      .then(response => response.json())
      .then(data => {
        stationOptions = data;
        console.log("Stations loaded successfully");
        // Trigger initial station population
        providerSelect.dispatchEvent(new Event('change'));
      })
      .catch(error => {
        console.warn("Could not load stations.json, using fallback:", error);
        // Trigger initial station population with fallback
        providerSelect.dispatchEvent(new Event('change'));
      });
  } catch (error) {
    console.warn("Fetch API not available, using fallback:", error);
    // Trigger initial station population with fallback
    providerSelect.dispatchEvent(new Event('change'));
  }
});

// Update station dropdown when provider changes
// ... existing code ...
providerSelect.addEventListener("change", () => {
  if (providerSelect.value === "custom") {
    apiUrl.classList.remove("hidden");
  } else {
    apiUrl.classList.add("hidden");
  }
    
  // Clear and repopulate station dropdown
  stationCode.innerHTML = '<option value="">Select a Station</option>';
  const options = stationOptions[providerSelect.value] || [];
  console.log(`Updating stations for ${providerSelect.value}:`, options);
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
