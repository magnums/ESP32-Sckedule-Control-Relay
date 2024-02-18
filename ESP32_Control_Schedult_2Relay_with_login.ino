#include <WiFiManager.h> // For WiFi setup without hardcoding credentials
#include <WebServer.h>
#include <EEPROM.h>
#include <time.h>

// NTP server to use
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200; // UTC+7: 7 hours * 3600 seconds/hour
const int daylightOffset_sec = 0; // Adjust if daylight saving time is applicable in your timezone

bool is_authenticated = false; // Global variable to track authentication status

const int relayPin = 26;

WebServer server(80);

// Structure to store schedule times
struct Schedule {
  int startHour[3];
  int startMinute[3];
  int endHour[3];
  int endMinute[3];
} schedule;

// Function prototypes
void setupTime();
void loadSchedule();
void saveSchedule();
void handleRoot();
void handleNotFound();
void handleSetSchedule();
void checkScheduleAndControlRelay();
int getCurrentHour();
int getCurrentMinute();


String login_html = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Login Page</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background-color: #f7f7f7;
      margin: 0;
      padding: 0;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
    }
    .login-container {
      width: 300px;
      padding: 20px;
      background-color: #fff;
      border-radius: 8px;
      box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
    }
    h2 {
      margin-top: 0;
      text-align: center;
      color: #333;
    }
    form {
      display: flex;
      flex-direction: column;
    }
    label {
      margin-bottom: 5px;
      color: #555;
    }
    input[type="text"],
    input[type="password"] {
      padding: 10px;
      margin-bottom: 10px;
      border: 1px solid #ccc;
      border-radius: 5px;
    }
    button[type="submit"] {
      padding: 10px;
      background-color: #007bff;
      color: #fff;
      border: none;
      border-radius: 5px;
      cursor: pointer;
    }
    button[type="submit"]:hover {
      background-color: #0056b3;
    }
  </style>
</head>
<body>
  <div class="login-container">
    <h2>Login</h2>
    <form action='/login' method='post'>
      <label for='username'>Username:</label>
      <input type='text' id='username' name='username'>
      <label for='password'>Password:</label>
      <input type='password' id='password' name='password'>
      <button type='submit'>Login</button>
    </form>
  </div>
</body>
</html>
)html";


void setup() {
  Serial.begin(115200);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  // Initialize EEPROM with size enough for the schedule
  EEPROM.begin(sizeof(schedule));

// WiFiManager for dynamic WiFi configuration
  WiFiManager wifiManager;
  // Attempts to connect to last known settings
  if(!wifiManager.autoConnect("AutoConnectAP")) {
    Serial.println("Failed to connect and hit timeout");
    // Reset and try again, or maybe put it to deep sleep
    ESP.restart();
  }

  Serial.println("Connected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  setupTime(); // Set up time synchronization
  loadSchedule(); // Load schedule from EEPROM

  // Define routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setSchedule", HTTP_POST, handleSetSchedule);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/logout", HTTP_GET, handleLogout); // GET request for logout
  server.on("/logout", HTTP_POST, handleLogout); // POST request for logout


server.on("/login", HTTP_GET,handleLogout, []() {
    if (!is_authenticated) {
        server.send(200, "text/html", login_html); // Serve the login page
    } else {
        // If already authenticated, redirect to the root or another page
        server.sendHeader("Location", "/", true);
        server.send(303);
    }
});



  server.on("/logout", HTTP_POST, handleLogout); // Add route for logout
  server.onNotFound(handleNotFound);

  server.begin();
}

void loop() {
  server.handleClient();
  checkScheduleAndControlRelay();
}

void setupTime() {
  // Initialize and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Wait for time to be set
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Time set");
}

void loadSchedule() {
  // Read the schedule from EEPROM
  EEPROM.get(0, schedule);
}

void saveSchedule() {
  // Write the schedule to EEPROM
  EEPROM.put(0, schedule);
  EEPROM.commit(); // Make sure to commit after writing to EEPROM
}




void handleRoot() {
  if (!is_authenticated) {
    server.send(401, "text/html", login_html);
    return;
  }
  
  if (server.hasArg("logout")) { // Check if logout parameter is present in the request
    handleLogout(); // Call handleLogout() if logout parameter is present
    return;
  }
  
  // Render the main page
  loadSchedule();
  

String html = "<!DOCTYPE html>"
              "<html lang=\"en\">"
              "<head>"
              "<meta charset=\"UTF-8\">"
              "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
              "<title>Main Page</title>"
              "<style>"
              "body {"
              "font-family: Arial, sans-serif;"
              "background-color: #f4f4f4;"
              "margin: 0;"
              "padding: 0;"
              "}"
              ".container {"
              "max-width: 800px;"
              "margin: 20px auto;"
              "padding: 20px;"
              "background-color: #fff;"
              "border-radius: 8px;"
              "box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);"
              "}"
              "h1 {"
              "color: #333;"
              "text-align: center;"
              "}"
              ".content {"
              "text-align: center;"
              "}"
              ".button {"
              "display: inline-block;"
              "padding: 10px 20px;"
              "background-color: #007bff;"
              "color: #fff;"
              "text-decoration: none;"
              "border-radius: 4px;"
              "transition: background-color 0.3s;"
              "}"
              ".button:hover {"
              "background-color: #0056b3;"
              "}"
              ".btn-danger {"
              "background-color: red;"
              "}"
              "</style>"
              "</head>"
              "<body>"
              "<div class=\"container\">"
              "<h1>Welcome to Your IoT Dashboard</h1>"
              "<div class=\"content\">"
              "<h3>View and Update Relay Schedule</h3>"
              "</div>";
             

html += "<div align='center'><table>"
        "<tr>"
        "<th>Start Time</th>"
        "<th></th>" // Empty header for space
        "<th>End Time</th>"
        "</tr>";
for (int i = 0; i < 3; i++) {
    html += "<tr>"
            "<td><label>Start Time " + String(i + 1) + ":</label> " + (schedule.startHour[i] < 10 ? "0" : "") + String(schedule.startHour[i]) + ":" + (schedule.startMinute[i] < 10 ? "0" : "") + String(schedule.startMinute[i]) + "</td>"
            "<td></td>" // Empty cell for spacing
            "<td><label>End Time " + String(i + 1) + ":</label> " + (schedule.endHour[i] < 10 ? "0" : "") + String(schedule.endHour[i]) + ":" + (schedule.endMinute[i] < 10 ? "0" : "") + String(schedule.endMinute[i]) + "</td>"
            "</tr>";
}
html += "</table></div>";

html += "<p id='currentDateTime'>Current Date and Time (UTC+7): <br> <span id='dateTime'></span></p>"
        "<form action=\"/setSchedule\" method=\"POST\">";
html += "<p>You can manage your device settings and schedules here.</p>";

for (int i = 1; i <= 3; i++) { // Start from 1 to avoid repeating Start Time 1 and End Time 1
    html += "<div class='time-input-container'>" // Start of time input container
            "<div class='time-input-group'>"
            "<label for='start-time" + String(i) + "'>Start" + String(i) + ":</label>"
            "<input type='time' id='start-time" + String(i) + "' name='start-time" + String(i) + "' value='" + (schedule.startHour[i - 1] < 10 ? "0" : "") + String(schedule.startHour[i - 1]) + ":" + (schedule.startMinute[i - 1] < 10 ? "0" : "") + String(schedule.startMinute[i - 1]) + "'>"
            "<label for='end-time" + String(i) + "'>End" + String(i) + ":</label>"
            "<input type='time' id='end-time" + String(i) + "' name='end-time" + String(i) + "' value='" + (schedule.endHour[i - 1] < 10 ? "0" : "") + String(schedule.endHour[i - 1]) + ":" + (schedule.endMinute[i - 1] < 10 ? "0" : "") + String(schedule.endMinute[i - 1]) + "'>"
            "</div>" // End of time-input-group
            "</div>"; // End of time input container
}

// Save Schedule Button
html += "<br><div align='center' class='time-input-container text-center'>" // Start of Save Schedule button container
        "<div class='time-input-group'>"
        "<button type='submit' class='button'>Save Schedule</button>"
        "</div>" // End of time-input-group
        "</div>"; // End of Save Schedule button container

html += "</form> <br><hr>"; // End of Save Schedule form

// Logout Button
html += "<div align='right' class='time-input-container text-center'>" // Start of Logout button container
        "<div class='time-input-group'>"
        "<form action=\"/logout\" method=\"POST\">" // Form for logout
        "<button type='submit' class='button btn-danger'>Logout</button>"
        "</form>" // End of logout form
        "</div>" // End of time-input-group
        "</div>"; // End of Logout button container

html +=
        "<script>"
        "function updateTime() {"
        "  var now = new Date();"
        "  var options = { weekday: 'long', year: 'numeric', month: 'long', day: 'numeric', hour: 'numeric', minute: 'numeric', second: 'numeric', hour12: false, timeZone: 'Asia/Bangkok' };"
        "  var dateTime = now.toLocaleString('en-US', options);"
        "  document.getElementById('dateTime').textContent = dateTime;"
        "}"
        "setInterval(updateTime, 1000);"
        "</script>"

        "</body>"
        "</html>";








  server.send(200, "text/html", html);
}



void handleLogout() {
  is_authenticated = false;
  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleLogin() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  String username = server.arg("username"); // Get the entered username

  // Check if the username is either "admin" or "Admin" (case-insensitive comparison)
  if (username.equalsIgnoreCase("admin") && server.arg("password") == "1234") {
    is_authenticated = true;
    server.sendHeader("Location", "/", true);
    server.send(303);
  } else {
    server.send(401, "text/plain", "Unauthorized");
  }
}






void handleSetSchedule() {
  // Ensure EEPROM size is adequate for the schedule structure and initialize
  EEPROM.begin(sizeof(schedule));
  
  for (int i = 0; i < 3; i++) {
    // Construct the argument names based on the input fields
    String startArg = "start-time" + String(i + 1); // start-time1, start-time2, start-time3
    String endArg = "end-time" + String(i + 1);     // end-time1, end-time2, end-time3

    // Parse start times
    String startTime = server.arg(startArg);
    schedule.startHour[i] = startTime.substring(0, 2).toInt();
    schedule.startMinute[i] = startTime.substring(3, 5).toInt();

    // Parse end times
    String endTime = server.arg(endArg);
    schedule.endHour[i] = endTime.substring(0, 2).toInt();
    schedule.endMinute[i] = endTime.substring(3, 5).toInt();
  }

  // Save the updated schedule to EEPROM
  saveSchedule();

  // Redirect the client to the root page after updating the schedule
  server.sendHeader("Location", "/", true);
  server.send(303); // HTTP status code for redirection after a POST request
}

void handleNotFound(){
  server.send(404, "text/plain", "404: Not found");
}

void checkScheduleAndControlRelay() {
  int currentHour = getCurrentHour();
  int currentMinute = getCurrentMinute();
  int currentSecond = getCurrentSecond(); // Add function to retrieve current second
 
  bool relayShouldBeOn = false;

  for (int i = 0; i < 3; i++) {
    if ((currentHour > schedule.startHour[i] || (currentHour == schedule.startHour[i] && currentMinute >= schedule.startMinute[i])) &&
        (currentHour < schedule.endHour[i] || (currentHour == schedule.endHour[i] && currentMinute < schedule.endMinute[i]))) {
      relayShouldBeOn = true;
      break; // Exit loop as we found the relay should be on
    }
  }

  // Debugging: Print current time and relay status
  Serial.print("Current Time: ");
  Serial.print(currentHour);
  Serial.print(":");
  Serial.print(currentMinute);
  Serial.print(":");
  Serial.print(currentSecond); // Print seconds
  Serial.print(", Relay Should Be On: ");
  Serial.println(relayShouldBeOn);
// Introduce a small delay before activating the relay
  delay(1000); // Adjust this delay as needed to align with your requirements


  digitalWrite(relayPin, relayShouldBeOn ? HIGH : LOW);
}


int getCurrentHour() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return 0;
  }
  return timeinfo.tm_hour;
}

int getCurrentMinute() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return 0;
  }
  return timeinfo.tm_min;
}

int getCurrentSecond() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return 0;
  }
  return timeinfo.tm_sec;
}
