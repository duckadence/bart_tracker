const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

let characteristic = null;

const status = document.getElementById("status");
const connectButton = document.getElementById("connect");
const onButton = document.getElementById("on");
const offButton = document.getElementById("off");

connectButton.addEventListener("click", connect);
onButton.addEventListener("click", () => setLED(true));
offButton.addEventListener("click", () => setLED(false));

async function connect() {
  if (!navigator.bluetooth) {
    status.textContent = "Web Bluetooth is not supported by this browser.";
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

    onButton.disabled = false;
    offButton.disabled = false;

    device.addEventListener("gattserverdisconnected", () => {
      characteristic = null;
      onButton.disabled = true;
      offButton.disabled = true;
      status.textContent = "Disconnected";
    });

  } catch (error) {
    console.error(error);
    status.textContent = "Connection cancelled or failed.";
  }
}

async function setLED(on) {
  if (!characteristic) return;

  try {
    const value = new TextEncoder().encode(on ? "1" : "0");
    await characteristic.writeValue(value);
    status.textContent = on ? "LED is ON" : "LED is OFF";
  } catch (error) {
    console.error(error);
    status.textContent = "Failed to control LED.";
  }
}
