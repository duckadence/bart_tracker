console.log("App script started");
const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

let characteristic = null;
let configuredStations = new Array(10).fill(null);

const status = document.getElementById("status");
const connectButton = document.getElementById("connect");
const controls = document.getElementById("controls");

const wifiSSID = document.getElementById("wifiSSID");
const wifiPass = document.getElementById("wifiPass");
const providerSelect = document.getElementById("provider");
const apiKey = document.getElementById("apiKey");
const apiUrl = document.getElementById("apiUrl");
const switchInterval = document.getElementById("switchInterval");
const apiInterval = document.getElementById("apiInterval");
const stationSlot = document.getElementById("stationSlot");
const stationCode = document.getElementById("stationCode");

// New elements for search
const stationSearch = document.getElementById("stationSearch");
const searchResults = document.getElementById("searchResults");
const useLocation = document.getElementById("useLocation");

console.log("Elements initialized:", {
    status: !!status,
    connectButton: !!connectButton,
    controls: !!controls,
    wifiSSID: !!wifiSSID,
    wifiPass: !!wifiPass,
    providerSelect: !!providerSelect,
    apiKey: !!apiKey,
    apiUrl: !!apiUrl,
    stationSlot: !!stationSlot,
    stationCode: !!stationCode,
    stationSearch: !!stationSearch,
    searchResults: !!searchResults,
    useLocation: !!useLocation
});

// Station options map (populated by fetch or fallback)
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
    { code: "NCON", name: "North Concord/Martinez" },
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
    { code: "70092", name: "San Mateo" },
    { code: "70102", name: "Hayward Park" },
    { code: "70112", name: "Hillsdale" },
    { code: "70122", name: "Belmont" },
    { code: "70132", name: "San Carlos" },
    { code: "70142", name: "Redwood City" },
    { code: "70152", name: "Atherton" },
    { code: "70162", name: "Menlo Park" },
    { code: "70172", name: "Palo Alto" },
    { code: "70182", name: "California Avenue" },
    { code: "70192", name: "San Antonio" },
    { code: "70202", name: "Mountain View" },
    { code: "70212", name: "Sunnyvale" },
    { code: "70222", name: "Lawrence" },
    { code: "70232", name: "Santa Clara" },
    { code: "70242", name: "College Park" },
    { code: "70252", name: "San Jose Diridon" }
  ],
  vta: [
    { code: "70211", name: "Rail - Mountain View" },
    { code: "70221", name: "Rail - San Antonio" },
    { code: "70231", name: "Rail - Lawrence" },
    { code: "70241", name: "Rail - Sunnyvale" },
    { code: "70251", name: "Rail - Santa Clara" },
    { code: "70261", name: "Rail - College Park" },
    { code: "70271", name: "Rail - San Jose Diridon" },
    { code: "70281", name: "Rail - Tamien" },
    { code: "70291", name: "Bus - Capitol" },
    { code: "70301", name: "Bus - Blossom Hill" },
    { code: "70311", name: "Bus - Morgan Hill" },
    { code: "70321", name: "Bus - Gilroy" }
  ],
  actransit: [
    { code: "50001", name: "Downtown Berkeley" },
    { code: "50002", name: "MacArthur" },
    { code: "50003", name: "Ashby" },
    { code: "50004", name: "Rockridge" },
    { code: "50005", name: "Temescal" },
    { code: "50006", name: "Piedmont" },
    { code: "50007", name: "Lakeshore" },
    { code: "50008", name: "Grand" },
    { code: "50009", name: "Broadway" },
    { code: "50010", name: "West Oakland" }
  ]
};

// Search debounce timer
let searchTimer = null;

// API base URLs
const API_BASE_URLS = {
  bart: "https://api.bart.gov/api/stn.aspx?cmd=stns&key=MW9S-E7SL-26DU-VV8V&json=y",
  muni: "https://api.511.org/transit/stops?api_key=MW9S-E7SL-26DU-VV8V&agency=SF&format=json",
  vta: "https://api.511.org/transit/stops?api_key=MW9S-E7SL-26DU-VV8V&agency=SC&format=json",
  caltrain: "https://api.511.org/transit/stops?api_key=MW9S-E7SL-26DU-VV8V&agency=CT&format=json",
  actransit: "https://api.511.org/transit/stops?api_key=MW9S-E7SL-26DU-VV8V&agency=AC&format=json"
};

// Search for stops using API
async function searchStops(query) {
  if (!query || query.length < 3) {
    searchResults.classList.add("hidden");
    return;
  }

  const provider = providerSelect.value;
  const apiUrl = API_BASE_URLS[provider];

  if (!apiUrl) {
    console.warn("No API configured for provider:", provider);
    return;
  }

  try {
    const response = await fetch(apiUrl);
    const data = await response.json();

    let stops = [];
    
    if (provider === "bart") {
      // Parse BART API response
      if (data?.root?.stations?.station) {
        const stations = Array.isArray(data.root.stations.station) 
          ? data.root.stations.station 
          : [data.root.stations.station];
        
        stops = stations
          .filter(s => s.name.toLowerCase().includes(query.toLowerCase()))
          .map(s => ({ code: s.abbr, name: s.name }));
      }
    } else {
      // Parse 511.org API response (Muni, VTA, Caltrain, AC Transit)
      const stopsData = data?.ServiceDelivery?.StopDelivery?.Stop;
      if (stopsData) {
        const stopsArray = Array.isArray(stopsData) ? stopsData : [stopsData];
        stops = stopsArray
          .filter(s => s.StopName && s.StopName.toLowerCase().includes(query.toLowerCase()))
          .map(s => ({ code: s.StopCode || s.id, name: s.StopName }));
      }
    }

    // Show results
    if (stops.length > 0) {
      showSearchResults(stops);
    } else {
      searchResults.classList.add("hidden");
      status.textContent = "No stops found for that search.";
    }
  } catch (error) {
    console.error("Search error:", error);
    searchResults.classList.add("hidden");
    status.textContent = "Error searching for stops. Using fallback list.";
    
    // Fallback to static list if API fails
    const fallbackStops = stationOptions[provider] || [];
    const filtered = fallbackStops.filter(s => 
      s.name.toLowerCase().includes(query.toLowerCase())
    );
    if (filtered.length > 0) showSearchResults(filtered);
  }
}

// Show search results in dropdown
function showSearchResults(stops) {
  searchResults.innerHTML = "";
  searchResults.classList.remove("hidden");
  
  stops.forEach(stop => {
    const div = document.createElement("div");
    div.className = "search-result";
    div.textContent = `${stop.name} (${stop.code})`;
    div.dataset.code = stop.code;
    div.dataset.name = stop.name;
    div.addEventListener("click", () => {
      stationCode.value = stop.code;
      status.textContent = `Selected: ${stop.name}`;
      searchResults.classList.add("hidden");
    });
    searchResults.appendChild(div);
  });
}

// Use geolocation to find nearby stops
async function findNearbyStops() {
  if (!navigator.geolocation) {
    status.textContent = "Geolocation not supported in this browser.";
    return;
  }

  status.textContent = "Getting your location...";
  
  try {
    const position = await new Promise((resolve, reject) => {
      navigator.geolocation.getCurrentPosition(resolve, reject, { timeout: 10000 });
    });

    const { latitude, longitude } = position.coords;
    status.textContent = `Location: ${latitude.toFixed(4)}, ${longitude.toFixed(4)}`;
    
    // For now, just show a message. In a full implementation, we'd query the API
    // with lat/long to find nearby stops. This would require a backend proxy
    // due to CORS restrictions with 511.org API.
    status.textContent = "Geolocation successful! For nearby stops, use the search above.";
    
  } catch (error) {
    console.error("Geolocation error:", error);
    status.textContent = "Could not get location. Please enable location services.";
  }
}

document.addEventListener('DOMContentLoaded', () => {
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
    const slot = parseInt(stationSlot.value);
    const code = stationCode.value.trim();
    
    if (slot >= 1 && slot <= 10 && code) {
      sendBLECommand(`STATION_${slot}=${code}`);
      configuredStations[slot - 1] = code;
      updateStationListUI();
      status.textContent = `Saved ${code} to slot ${slot}`;
    } else {
      status.textContent = "Select a valid slot and station.";
    }
  });

  document.getElementById("saveTiming").addEventListener("click", () => {
    if (switchInterval.value) sendBLECommand(`SWITCH_INT=${switchInterval.value}`);
    if (apiInterval.value) setTimeout(() => sendBLECommand(`API_INT=${apiInterval.value}`), 200);
  });

  document.getElementById("fetchStations").addEventListener("click", () => {
    sendBLECommand("GET_STATIONS");
  });

  // Search functionality
  stationSearch.addEventListener("input", () => {
    clearTimeout(searchTimer);
    searchTimer = setTimeout(() => searchStops(stationSearch.value), 500);
  });

  useLocation.addEventListener("click", findNearbyStops);

  // Fetch stations.json
  fetch('data/stations.json')
    .then(response => response.json())
    .then(data => {
      stationOptions = data;
      console.log("Stations loaded successfully");
      providerSelect.dispatchEvent(new Event('change'));
    })
    .catch(error => {
      console.warn("Could not load stations.json, using fallback:", error);
      providerSelect.dispatchEvent(new Event('change'));
    });
});

providerSelect.addEventListener("change", () => {
  if (providerSelect.value === "custom") {
    apiUrl.classList.remove("hidden");
  } else {
    apiUrl.classList.add("hidden");
  }
  
  stationCode.innerHTML = '<option value="">Select a Station</option>';
  const options = stationOptions[providerSelect.value] || [];
  options.forEach(opt => {
    const el = document.createElement("option");
    el.value = opt.code;
    el.textContent = opt.name;
    stationCode.appendChild(el);
  });
});

function updateStationListUI() {
  const listDiv = document.getElementById("stationList");
  listDiv.innerHTML = "";
  
  const activeStations = configuredStations.map((code, index) => ({slot: index + 1, code})).filter(s => s.code);
  
  if (activeStations.length === 0) {
    listDiv.innerHTML = "<p>No stations configured yet.</p>";
  } else {
    activeStations.forEach(s => {
      listDiv.insertAdjacentHTML('beforeend', `<p>Slot ${s.slot}: ${s.code}</p>`);
    });
  }
}

async function connect() {
  console.log("Connect button clicked");
  if (!navigator.bluetooth) {
    console.error("Web Bluetooth is not supported");
    status.textContent = "Web Bluetooth is not supported on this browser.";
    return;
  }
  console.log("Navigator bluetooth is supported");

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