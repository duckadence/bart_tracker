const SERVICE_UUID =
  "4fafc201-1fb5-459e-8fcc-c5c9c331914b";

const CHARACTERISTIC_UUID =
  "beb5483e-36e1-4688-b7f5-ea07361b26a8";

let characteristic = null;
let device = null;
let stationCount = 0;


// ==================================================
// BART STATIONS
// ==================================================

const stations = [
  ["12TH", "12th St/Oakland City Center"],
  ["16TH", "16th St Mission"],
  ["19TH", "19th St Oakland"],
  ["24TH", "24th St Mission"],
  ["ANTC", "Antioch"],
  ["ASHB", "Ashby"],
  ["BALB", "Balboa Park"],
  ["BAYF", "Bay Fair"],
  ["BERY", "Berryessa/North San Jose"],
  ["CAST", "Castro Valley"],
  ["CIVC", "Civic Center/UN Plaza"],
  ["COLM", "Colma"],
  ["COLS", "Coliseum"],
  ["CONC", "Concord"],
  ["DALY", "Daly City"],
  ["DBRK", "Downtown Berkeley"],
  ["DUBL", "Dublin/Pleasanton"],
  ["DELN", "El Cerrito del Norte"],
  ["PLZA", "El Cerrito Plaza"],
  ["EMBR", "Embarcadero"],
  ["FRMT", "Fremont"],
  ["FTVL", "Fruitvale"],
  ["GLEN", "Glen Park"],
  ["HAYW", "Hayward"],
  ["LAFY", "Lafayette"],
  ["LAKE", "Lake Merritt"],
  ["MCAR", "MacArthur"],
  ["MLBR", "Millbrae"],
  ["MLPT", "Milpitas"],
  ["MONT", "Montgomery St"],
  ["NBRK", "North Berkeley"],
  ["NCON", "North Concord/Martinez"],
  ["OAKL", "Oakland International Airport"],
  ["ORIN", "Orinda"],
  ["PCTR", "Pittsburg Center"],
  ["PITT", "Pittsburg/Bay Point"],
  ["PHIL", "Pleasant Hill/Contra Costa Centre"],
  ["POWL", "Powell St"],
  ["RICH", "Richmond"],
  ["ROCK", "Rockridge"],
  ["SANL", "San Leandro"],
  ["SBRN", "San Bruno"],
  ["SFIA", "San Francisco International Airport"],
  ["SHAY", "South Hayward"],
  ["SSAN", "South San Francisco"],
  ["UCTY", "Union City"],
  ["WARM", "Warm Springs/South Fremont"],
  ["WCRK", "Walnut Creek"],
  ["WDUB", "West Dublin/Pleasanton"],
  ["WOAK", "West Oakland"]
];


// ==================================================
// ELEMENTS
// ==================================================

const status =
  document.getElementById("status");

const connectButton =
  document.getElementById("connect");

const saveButton =
  document.getElementById("save");

const addStationButton =
  document.getElementById("add-station");

const stationsContainer =
  document.getElementById("stations");


// ==================================================
// BUTTONS
// ==================================================

connectButton.addEventListener(
  "click",
  connect
);

saveButton.addEventListener(
  "click",
  saveConfiguration
);

addStationButton.addEventListener(
  "click",
  addStation
);


// ==================================================
// CREATE STATION SELECTOR
// ==================================================

function addStation() {

  stationCount++;

  const section =
    document.createElement("div");

  section.className = "station-row";
  section.id = `station-row-${stationCount}`;

  const label =
    document.createElement("label");

  label.textContent =
    `Station ${stationCount}`;

  label.htmlFor =
    `station-${stationCount}`;

  const select =
    document.createElement("select");

  select.id =
    `station-${stationCount}`;

  select.dataset.stationNumber =
    stationCount;

  // None option
  const none =
    document.createElement("option");

  none.value = "";
  none.textContent = "None";

  select.appendChild(none);

  // BART stations
  for (const [code, name] of stations) {

    const option =
      document.createElement("option");

    option.value = code;
    option.textContent = name;

    select.appendChild(option);
  }

  const removeButton =
    document.createElement("button");

  removeButton.type = "button";
  removeButton.textContent = "Remove";
  removeButton.className = "remove-station";

  removeButton.addEventListener(
    "click",
    () => section.remove()
  );

  section.appendChild(label);
  section.appendChild(select);
  section.appendChild(removeButton);

  stationsContainer.appendChild(section);
}


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

    status.textContent =
      "Searching for ESP32...";

    device =
      await navigator.bluetooth.requestDevice({
        filters: [
          {
            services: [SERVICE_UUID]
          }
        ]
      });

    status.textContent =
      "Connecting...";

    const server =
      await device.gatt.connect();

    const service =
      await server.getPrimaryService(
        SERVICE_UUID
      );

    characteristic =
      await service.getCharacteristic(
        CHARACTERISTIC_UUID
      );

    status.textContent =
      "Connected to " +
      (device.name || "ESP32");

    connectButton.textContent =
      "Connected";

    connectButton.disabled =
      true;

    saveButton.disabled =
      false;

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

  connectButton.disabled =
    false;

  connectButton.textContent =
    "Connect ESP32";

  saveButton.disabled =
    true;

  status.textContent =
    "Disconnected";
}


// ==================================================
// SEND BLE COMMAND
// ==================================================

async function sendCommand(command) {

  if (!characteristic) {

    status.textContent =
      "ESP32 is not connected.";

    return false;
  }

  try {

    console.log(
      "BLE → ESP32:",
      command
    );

    const data =
      new TextEncoder().encode(command);

    await characteristic.writeValue(data);

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
    document
      .getElementById("wifi-ssid")
      .value
      .trim();

  const password =
    document
      .getElementById("wifi-password")
      .value;

  if (!ssid) {

    status.textContent =
      "Enter a Wi-Fi network.";

    return;
  }

  // Get all station selectors
  const stationSelects =
    stationsContainer.querySelectorAll(
      "select"
    );

  const selectedStations = [];

  stationSelects.forEach(select => {

    if (select.value) {
      selectedStations.push(
        select.value
      );
    }

  });


  try {

    status.textContent =
      "Saving configuration...";


    // ------------------------------
    // WI-FI
    // ------------------------------

    if (!await sendCommand(
      "WIFI_SSID=" + ssid
    )) return;

    await delay(100);

    if (!await sendCommand(
      "WIFI_PASS=" + password
    )) return;

    await delay(100);


    // ------------------------------
    // STATIONS
    // ------------------------------

    for (
      let i = 0;
      i < selectedStations.length;
      i++
    ) {

      const command =
        `STATION_${i + 1}=` +
        selectedStations[i];

      if (!await sendCommand(command))
        return;

      await delay(100);
    }


    // ------------------------------
    // CONNECT WI-FI
    // ------------------------------

    if (!await sendCommand(
      "WIFI_CONNECT"
    )) return;

    status.textContent =
      "Configuration sent. " +
      "ESP32 is connecting to Wi-Fi.";

  } catch (error) {

    console.error(error);

    status.textContent =
      "Failed to save configuration.";
  }
}


// ==================================================
// HELPER
// ==================================================

function delay(ms) {

  return new Promise(
    resolve => setTimeout(resolve, ms)
  );
}


// ==================================================
// INITIAL STATIONS
// ==================================================

// Start with three stations
addStation();
addStation();
addStation();
