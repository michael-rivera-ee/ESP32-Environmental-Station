#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// WIFI
// =====================================================

const char* ssid = "YourActualWiFi";
const char* password = "YourActualPassword";

WiFiServer server(80);

// =====================================================
// OLED
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// =====================================================
// SENSORS
// =====================================================

Adafruit_BME280 bme;
BH1750 lightMeter;

#define RAIN_SENSOR_PIN 34

// Your rain sensor:
// Dry = approximately 4095
// Wet = approximately 400-600

#define RAIN_THRESHOLD 2000

// =====================================================
// SENSOR VARIABLES
// =====================================================

float temperature = 0;
float humidity = 0;
float pressure = 0;
float lightLevel = 0;

int rainValue = 4095;
bool raining = false;

// =====================================================
// TEMPERATURE HISTORY
// =====================================================

#define MAX_READINGS 30

float temperatureHistory[MAX_READINGS];
unsigned long timeHistory[MAX_READINGS];

int historyCount = 0;

// =====================================================
// DATA LOG
// =====================================================

#define MAX_LOGS 30

struct DataLog {
  float temperature;
  float humidity;
  float pressure;
  float light;
  int rainValue;
  bool raining;
  unsigned long time;
};

DataLog logs[MAX_LOGS];

int logCount = 0;

// =====================================================
// TIMERS
// =====================================================

unsigned long lastSensorRead = 0;
unsigned long lastLog = 0;
unsigned long lastOLEDUpdate = 0;

const unsigned long SENSOR_INTERVAL = 1000;
const unsigned long LOG_INTERVAL = 5000;
const unsigned long OLED_INTERVAL = 1000;

// =====================================================
// READ SENSORS
// =====================================================

void readSensors() {

  temperature = bme.readTemperature();

  humidity = bme.readHumidity();

  pressure = bme.readPressure() / 100.0F;

  lightLevel = lightMeter.readLightLevel();

  rainValue = analogRead(RAIN_SENSOR_PIN);

  // Lower value = wetter
  // Higher value = dryer

  raining = rainValue < RAIN_THRESHOLD;
}

// =====================================================
// ADD TEMPERATURE READING
// =====================================================

void addTemperatureReading() {

  if (historyCount < MAX_READINGS) {

    temperatureHistory[historyCount] = temperature;
    timeHistory[historyCount] = millis() / 1000;

    historyCount++;

  } else {

    for (int i = 0; i < MAX_READINGS - 1; i++) {

      temperatureHistory[i] =
        temperatureHistory[i + 1];

      timeHistory[i] =
        timeHistory[i + 1];
    }

    temperatureHistory[MAX_READINGS - 1] =
      temperature;

    timeHistory[MAX_READINGS - 1] =
      millis() / 1000;
  }
}

// =====================================================
// ADD DATA LOG
// =====================================================

void addLog() {

  if (logCount < MAX_LOGS) {

    logs[logCount].temperature = temperature;
    logs[logCount].humidity = humidity;
    logs[logCount].pressure = pressure;
    logs[logCount].light = lightLevel;
    logs[logCount].rainValue = rainValue;
    logs[logCount].raining = raining;
    logs[logCount].time = millis() / 1000;

    logCount++;

  } else {

    for (int i = 0; i < MAX_LOGS - 1; i++) {

      logs[i] = logs[i + 1];
    }

    logs[MAX_LOGS - 1].temperature = temperature;
    logs[MAX_LOGS - 1].humidity = humidity;
    logs[MAX_LOGS - 1].pressure = pressure;
    logs[MAX_LOGS - 1].light = lightLevel;
    logs[MAX_LOGS - 1].rainValue = rainValue;
    logs[MAX_LOGS - 1].raining = raining;
    logs[MAX_LOGS - 1].time = millis() / 1000;
  }
}

// =====================================================
// OLED DISPLAY
// =====================================================

void updateOLED() {

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 0);

  display.println("ENVIRONMENT STATION");

  display.drawLine(
    0,
    9,
    127,
    9,
    SSD1306_WHITE
  );

  // Temperature

  display.setCursor(0, 14);

  display.print("Temp: ");

  display.print(
    temperature,
    1
  );

  display.println(" C");

  // Humidity

  display.setCursor(0, 25);

  display.print("Hum:  ");

  display.print(
    humidity,
    1
  );

  display.println(" %");

  // Pressure

  display.setCursor(0, 36);

  display.print("Pres: ");

  display.print(
    pressure,
    0
  );

  display.println(" hPa");

  // Light

  display.setCursor(0, 47);

  display.print("Light: ");

  display.print(
    lightLevel,
    0
  );

  display.println(" lx");

  // Rain

  display.setCursor(82, 14);

  if (raining) {

    display.println("RAIN");

  } else {

    display.println("DRY");
  }

  display.display();
}

// =====================================================
// SEND JSON DATA
// =====================================================

void sendJSON(WiFiClient& client) {

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  client.print("{");

  client.print("\"temperature\":");
  client.print(temperature, 2);

  client.print(",\"humidity\":");
  client.print(humidity, 2);

  client.print(",\"pressure\":");
  client.print(pressure, 2);

  client.print(",\"light\":");
  client.print(lightLevel, 2);

  client.print(",\"rainValue\":");
  client.print(rainValue);

  client.print(",\"raining\":");
  client.print(
    raining ? "true" : "false"
  );

  // Temperature history

  client.print(",\"history\":[");

  for (int i = 0; i < historyCount; i++) {

    if (i > 0) {
      client.print(",");
    }

    client.print(
      temperatureHistory[i],
      2
    );
  }

  client.print("]");

  // Time history

  client.print(",\"times\":[");

  for (int i = 0; i < historyCount; i++) {

    if (i > 0) {
      client.print(",");
    }

    client.print(
      timeHistory[i]
    );
  }

  client.print("]");

  // Data logs

  client.print(",\"logs\":[");

  for (int i = 0; i < logCount; i++) {

    if (i > 0) {
      client.print(",");
    }

    client.print("{");

    client.print("\"temperature\":");
    client.print(
      logs[i].temperature,
      2
    );

    client.print(",\"humidity\":");
    client.print(
      logs[i].humidity,
      2
    );

    client.print(",\"pressure\":");
    client.print(
      logs[i].pressure,
      2
    );

    client.print(",\"light\":");
    client.print(
      logs[i].light,
      2
    );

    client.print(",\"rainValue\":");
    client.print(
      logs[i].rainValue
    );

    client.print(",\"raining\":");
    client.print(
      logs[i].raining
      ? "true"
      : "false"
    );

    client.print(",\"time\":");
    client.print(
      logs[i].time
    );

    client.print("}");
  }

  client.print("]");

  client.print("}");
}

// =====================================================
// WEB PAGE
// =====================================================

void sendWebPage(WiFiClient& client) {

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();

  client.println(R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
      content="width=device-width, initial-scale=1">

<title>
ESP32 Environmental Station
</title>

<style>

/* =====================================================
   GENERAL
   ===================================================== */

* {
  box-sizing: border-box;
}

body {

  margin: 0;

  font-family:
    Arial,
    Helvetica,
    sans-serif;

  background:
    linear-gradient(
      135deg,
      #0f172a,
      #172554,
      #0f172a
    );

  color: white;

  min-height: 100vh;
}

.container {

  width: 95%;

  max-width: 1200px;

  margin: auto;

  padding: 30px 0 50px;
}

/* =====================================================
   HEADER
   ===================================================== */

header {

  margin-bottom: 30px;
}

.title {

  font-size: 32px;

  font-weight: bold;

  margin-bottom: 8px;
}

.subtitle {

  color: #94a3b8;

  font-size: 15px;
}

.status {

  display: inline-flex;

  align-items: center;

  gap: 8px;

  margin-top: 15px;

  padding: 8px 14px;

  border-radius: 20px;

  background:
    rgba(255,255,255,0.08);

  color: #cbd5e1;

  font-size: 13px;
}

.status-dot {

  width: 9px;

  height: 9px;

  border-radius: 50%;

  background: #22c55e;
}

/* =====================================================
   SENSOR CARDS
   ===================================================== */

.cards {

  display: grid;

  grid-template-columns:
    repeat(
      auto-fit,
      minmax(210px, 1fr)
    );

  gap: 18px;
}

.card {

  background:
    rgba(255,255,255,0.08);

  border:
    1px solid
    rgba(255,255,255,0.1);

  border-radius: 18px;

  padding: 22px;

  backdrop-filter: blur(10px);

  box-shadow:
    0 10px 30px
    rgba(0,0,0,0.2);
}

.card-title {

  color: #94a3b8;

  font-size: 14px;

  margin-bottom: 15px;
}

.value {

  font-size: 32px;

  font-weight: bold;
}

.unit {

  font-size: 15px;

  color: #94a3b8;

  margin-left: 5px;
}

/* =====================================================
   RAIN
   ===================================================== */

.rain-card {

  margin-top: 20px;
}

.rain-status {

  font-size: 24px;

  font-weight: bold;

  margin-bottom: 8px;
}

.rain-description {

  color: #94a3b8;

  font-size: 14px;
}

/* =====================================================
   SECTIONS
   ===================================================== */

.section {

  margin-top: 25px;
}

.section-title {

  font-size: 20px;

  font-weight: bold;

  margin-bottom: 15px;
}

/* =====================================================
   GRAPH
   ===================================================== */

.graph-card {

  padding: 20px;
}

canvas {

  width: 100%;

  height: 300px;
}

/* =====================================================
   ALERTS
   ===================================================== */

.alert {

  padding: 15px;

  border-radius: 12px;

  margin-bottom: 10px;

  background:
    rgba(255,255,255,0.07);

  color: #cbd5e1;
}

.alert.warning {

  border-left:
    4px solid #f59e0b;
}

.alert.good {

  border-left:
    4px solid #22c55e;
}

/* =====================================================
   TABLE
   ===================================================== */

.table-container {

  overflow-x: auto;
}

table {

  width: 100%;

  border-collapse: collapse;

  min-width: 700px;
}

th,
td {

  text-align: left;

  padding: 13px;

  border-bottom:
    1px solid
    rgba(255,255,255,0.08);
}

th {

  color: #94a3b8;

  font-size: 13px;
}

td {

  font-size: 14px;
}

.rain-yes {

  color: #60a5fa;

  font-weight: bold;
}

.rain-no {

  color: #94a3b8;
}

/* =====================================================
   FOOTER
   ===================================================== */

.footer {

  margin-top: 35px;

  text-align: center;

  color: #64748b;

  font-size: 13px;
}

</style>

</head>

<body>

<div class="container">

<!-- ===================================================
     HEADER
     =================================================== -->

<header>

<div class="title">

ESP32 Environmental Station

</div>

<div class="subtitle">

Real-time environmental monitoring dashboard

</div>

<div class="status">

<div class="status-dot"></div>

ESP32 Online

</div>

</header>


<!-- ===================================================
     SENSOR CARDS
     =================================================== -->

<div class="cards">


<!-- TEMPERATURE -->

<div class="card">

<div class="card-title">

TEMPERATURE

</div>

<div>

<span
  class="value"
  id="temperature">
--
</span>

<span class="unit">
&deg;C
</span>

</div>

</div>


<!-- HUMIDITY -->

<div class="card">

<div class="card-title">

HUMIDITY

</div>

<div>

<span
  class="value"
  id="humidity">
--
</span>

<span class="unit">
%
</span>

</div>

</div>


<!-- PRESSURE -->

<div class="card">

<div class="card-title">

PRESSURE

</div>

<div>

<span
  class="value"
  id="pressure">
--
</span>

<span class="unit">
hPa
</span>

</div>

</div>


<!-- LIGHT -->

<div class="card">

<div class="card-title">

LIGHT LEVEL

</div>

<div>

<span
  class="value"
  id="light">
--
</span>

<span class="unit">
lux
</span>

</div>

</div>

</div>


<!-- ===================================================
     RAIN
     =================================================== -->

<div class="section">

<div class="card rain-card">

<div class="card-title">

RAIN SENSOR

</div>

<div
  class="rain-status"
  id="rainStatus">

Checking...

</div>

<div class="rain-description">

Sensor reading:

<span id="rainValue">
--
</span>

</div>

</div>

</div>


<!-- ===================================================
     TEMPERATURE GRAPH
     =================================================== -->

<div class="section">

<div class="section-title">

Temperature History

</div>

<div class="card graph-card">

<canvas
  id="temperatureChart">
</canvas>

</div>

</div>


<!-- ===================================================
     ALERTS
     =================================================== -->

<div class="section">

<div class="section-title">

Environmental Alerts

</div>

<div id="alerts">

<div class="alert good">

All environmental conditions are currently normal.

</div>

</div>

</div>


<!-- ===================================================
     DATA LOG
     =================================================== -->

<div class="section">

<div class="section-title">

Recent Data

</div>

<div class="card table-container">

<table>

<thead>

<tr>

<th>Time</th>

<th>Temperature</th>

<th>Humidity</th>

<th>Pressure</th>

<th>Light</th>

<th>Rain</th>

</tr>

</thead>

<tbody id="logTable">

</tbody>

</table>

</div>

</div>


<!-- ===================================================
     FOOTER
     =================================================== -->

<div class="footer">

ESP32 Environmental Monitoring Station

</div>

</div>


<script>

/* =====================================================
   FORMAT TIME
   ===================================================== */

function formatTime(seconds) {

  let hours =
    Math.floor(
      seconds / 3600
    );

  let minutes =
    Math.floor(
      (seconds % 3600) / 60
    );

  let secs =
    Math.floor(
      seconds % 60
    );

  return String(hours).padStart(2,'0')
    + ":"
    + String(minutes).padStart(2,'0')
    + ":"
    + String(secs).padStart(2,'0');
}


/* =====================================================
   UPDATE DASHBOARD
   ===================================================== */

async function updateDashboard() {

  try {

    const response =
      await fetch('/data');

    const data =
      await response.json();


    // Temperature

    document.getElementById(
      "temperature"
    ).textContent =
      data.temperature.toFixed(1);


    // Humidity

    document.getElementById(
      "humidity"
    ).textContent =
      data.humidity.toFixed(1);


    // Pressure

    document.getElementById(
      "pressure"
    ).textContent =
      data.pressure.toFixed(0);


    // Light

    document.getElementById(
      "light"
    ).textContent =
      data.light.toFixed(0);


    // Rain value

    document.getElementById(
      "rainValue"
    ).textContent =
      data.rainValue;


    // Rain status

    const rainStatus =
      document.getElementById(
        "rainStatus"
      );


    if (data.raining) {

      rainStatus.textContent =
        "Rain Detected";

    } else {

      rainStatus.textContent =
        "No Rain Detected";
    }


    // =================================================
    // ALERTS
    // =================================================

    let alerts = [];


    if (data.temperature > 30) {

      alerts.push(
        "Warning: High temperature detected: "
        + data.temperature.toFixed(1)
        + " &deg;C"
      );
    }


    if (data.temperature < 10) {

      alerts.push(
        "Warning: Low temperature detected: "
        + data.temperature.toFixed(1)
        + " &deg;C"
      );
    }


    if (data.humidity > 70) {

      alerts.push(
        "Warning: High humidity detected: "
        + data.humidity.toFixed(1)
        + "%"
      );
    }


    if (data.humidity < 30) {

      alerts.push(
        "Warning: Low humidity detected: "
        + data.humidity.toFixed(1)
        + "%"
      );
    }


    const alertContainer =
      document.getElementById(
        "alerts"
      );


    if (alerts.length === 0) {

      alertContainer.innerHTML =
        '<div class="alert good">'
        + 'All environmental conditions are currently normal.'
        + '</div>';

    } else {

      alertContainer.innerHTML =
        alerts.map(function(alert) {

          return '<div class="alert warning">'
            + alert
            + '</div>';

        }).join("");
    }


    // =================================================
    // GRAPH
    // =================================================

    drawGraph(
      data.history
    );


    // =================================================
    // DATA TABLE
    // =================================================

    const table =
      document.getElementById(
        "logTable"
      );

    table.innerHTML = "";


    for (
      let i = data.logs.length - 1;
      i >= 0;
      i--
    ) {

      const log =
        data.logs[i];


      const row =
        document.createElement(
          "tr"
        );


      row.innerHTML =

        "<td>"
        + formatTime(log.time)
        + "</td>"

        +

        "<td>"
        + log.temperature.toFixed(1)
        + " &deg;C"
        + "</td>"

        +

        "<td>"
        + log.humidity.toFixed(1)
        + " %"
        + "</td>"

        +

        "<td>"
        + log.pressure.toFixed(0)
        + " hPa"
        + "</td>"

        +

        "<td>"
        + log.light.toFixed(0)
        + " lux"
        + "</td>"

        +

        "<td class='"
        + (
            log.raining
            ? "rain-yes"
            : "rain-no"
          )
        + "'>"

        +

        (
          log.raining
          ? "Rain"
          : "Dry"
        )

        +

        "</td>";


      table.appendChild(
        row
      );
    }

  }

  catch (error) {

    console.log(
      "Connection error:",
      error
    );
  }
}


/* =====================================================
   DRAW TEMPERATURE GRAPH
   ===================================================== */

function drawGraph(values) {

  const canvas =
    document.getElementById(
      "temperatureChart"
    );

  const ctx =
    canvas.getContext("2d");


  const width =
    canvas.clientWidth;

  const height =
    canvas.clientHeight;


  canvas.width =
    width *
    window.devicePixelRatio;

  canvas.height =
    height *
    window.devicePixelRatio;


  ctx.scale(
    window.devicePixelRatio,
    window.devicePixelRatio
  );


  ctx.clearRect(
    0,
    0,
    width,
    height
  );


  if (values.length < 2) {

    ctx.fillStyle =
      "#94a3b8";

    ctx.font =
      "14px Arial";

    ctx.fillText(
      "Collecting temperature data...",
      20,
      40
    );

    return;
  }


  let min =
    Math.min(...values);

  let max =
    Math.max(...values);


  if (max - min < 2) {

    min -= 1;

    max += 1;
  }


  const padding = 35;


  // ===================================================
  // GRID
  // ===================================================

  ctx.strokeStyle =
    "rgba(255,255,255,0.08)";

  ctx.lineWidth = 1;


  for (
    let i = 0;
    i < 5;
    i++
  ) {

    const y =
      padding +
      (
        height -
        padding * 2
      )
      * i / 4;


    ctx.beginPath();

    ctx.moveTo(
      padding,
      y
    );

    ctx.lineTo(
      width - padding,
      y
    );

    ctx.stroke();


    const value =
      max -
      (
        max - min
      )
      * i / 4;


    ctx.fillStyle =
      "#94a3b8";

    ctx.font =
      "11px Arial";


    ctx.fillText(
      value.toFixed(1),
      5,
      y + 4
    );
  }


  // ===================================================
  // GRAPH LINE
  // ===================================================

  ctx.beginPath();


  for (
    let i = 0;
    i < values.length;
    i++
  ) {

    const x =
      padding +
      (
        width -
        padding * 2
      )
      * i /
      (
        values.length - 1
      );


    const y =
      height -
      padding -
      (
        values[i] - min
      )
      /
      (
        max - min
      )
      *
      (
        height -
        padding * 2
      );


    if (i === 0) {

      ctx.moveTo(
        x,
        y
      );

    } else {

      ctx.lineTo(
        x,
        y
      );
    }
  }


  ctx.strokeStyle =
    "#60a5fa";

  ctx.lineWidth = 3;

  ctx.stroke();


  // ===================================================
  // DATA POINTS
  // ===================================================

  for (
    let i = 0;
    i < values.length;
    i++
  ) {

    const x =
      padding +
      (
        width -
        padding * 2
      )
      * i /
      (
        values.length - 1
      );


    const y =
      height -
      padding -
      (
        values[i] - min
      )
      /
      (
        max - min
      )
      *
      (
        height -
        padding * 2
      );


    ctx.beginPath();


    ctx.arc(
      x,
      y,
      3,
      0,
      Math.PI * 2
    );


    ctx.fillStyle =
      "#ffffff";

    ctx.fill();
  }
}


/* =====================================================
   AUTOMATIC REFRESH
   ===================================================== */

updateDashboard();

setInterval(
  updateDashboard,
  2000
);

</script>

</body>

</html>

)rawliteral");
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32 ENVIRONMENTAL STATION");
  Serial.println("==============================");


  // ===================================================
  // I2C
  // ===================================================

  Wire.begin(
    21,
    22
  );


  // ===================================================
  // OLED
  // ===================================================

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C
      )) {

    Serial.println(
      "OLED not found!"
    );

  } else {

    display.clearDisplay();

    display.setTextColor(
      SSD1306_WHITE
    );

    display.setTextSize(1);

    display.setCursor(
      0,
      0
    );

    display.println(
      "Environmental"
    );

    display.println(
      "Monitoring Station"
    );

    display.println();

    display.println(
      "Starting..."
    );

    display.display();
  }


  // ===================================================
  // BME280
  // ===================================================

  bool bmeFound =
    bme.begin(0x76);


  if (!bmeFound) {

    bmeFound =
      bme.begin(0x77);
  }


  if (!bmeFound) {

    Serial.println(
      "BME280 not found!"
    );

  } else {

    Serial.println(
      "BME280 connected."
    );
  }


  // ===================================================
  // BH1750
  // ===================================================

  if (
    lightMeter.begin(
      BH1750::CONTINUOUS_HIGH_RES_MODE
    )
  ) {

    Serial.println(
      "BH1750 connected."
    );

  } else {

    Serial.println(
      "BH1750 not found!"
    );
  }


  // ===================================================
  // RAIN SENSOR
  // ===================================================

  pinMode(
    RAIN_SENSOR_PIN,
    INPUT
  );

  Serial.println(
    "Rain sensor connected."
  );


  // ===================================================
  // WIFI
  // ===================================================

  Serial.print(
    "Connecting to WiFi"
  );


  WiFi.begin(
    ssid,
    password
  );


  while (
    WiFi.status() != WL_CONNECTED
  ) {

    delay(500);

    Serial.print(".");
  }


  Serial.println();

  Serial.println(
    "WiFi connected!"
  );


  Serial.print(
    "IP Address: "
  );

  Serial.println(
    WiFi.localIP()
  );


  server.begin();


  Serial.println(
    "Web server started."
  );

  Serial.println();

  Serial.println(
    "Open the IP address above"
  );

  Serial.println(
    "in your web browser."
  );
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  unsigned long currentTime =
    millis();


  // ===================================================
  // READ SENSORS
  // ===================================================

  if (
    currentTime -
    lastSensorRead >=
    SENSOR_INTERVAL
  ) {

    lastSensorRead =
      currentTime;


    readSensors();

    addTemperatureReading();


    // Serial monitor

    Serial.print(
      "Temperature: "
    );

    Serial.print(
      temperature,
      1
    );

    Serial.print(
      " C | Humidity: "
    );

    Serial.print(
      humidity,
      1
    );

    Serial.print(
      " % | Pressure: "
    );

    Serial.print(
      pressure,
      1
    );

    Serial.print(
      " hPa | Light: "
    );

    Serial.print(
      lightLevel,
      0
    );

    Serial.print(
      " lux | Rain: "
    );

    Serial.println(
      raining
      ? "YES"
      : "NO"
    );
  }


  // ===================================================
  // DATA LOG
  // ===================================================

  if (
    currentTime -
    lastLog >=
    LOG_INTERVAL
  ) {

    lastLog =
      currentTime;

    addLog();
  }


  // ===================================================
  // OLED
  // ===================================================

  if (
    currentTime -
    lastOLEDUpdate >=
    OLED_INTERVAL
  ) {

    lastOLEDUpdate =
      currentTime;

    updateOLED();
  }


  // ===================================================
  // WEB SERVER
  // ===================================================

  WiFiClient client =
    server.available();


  if (client) {

    String request =
      client.readStringUntil('\r');

    client.flush();


    if (
      request.indexOf(
        "GET /data"
      ) >= 0
    ) {

      sendJSON(client);

    } else {

      sendWebPage(client);
    }


    delay(1);

    client.stop();
  }
}
