#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#ifdef ESP32
// Include the ESP32 servo library and use the right pins
#include <ESP32Servo.h>
// 17,18,21,38
#define SERVO_PIN_ROTATION 17
#define SERVO_PIN_SHOULDER 18
#define SERVO_PIN_WRIST 21
#define SERVO_PIN_CLAW 38
#else
// Otherwise include the regular Arduino servo library, use those pins
#include <Servo.h>
// 8,9,10,11
#define SERVO_PIN_ROTATION 8
#define SERVO_PIN_SHOULDER 9
#define SERVO_PIN_WRIST 10
#define SERVO_PIN_CLAW 11
#endif

// Servo objects for each axis
Servo servo_rotation;
Servo servo_shoulder;
Servo servo_wrist;
Servo servo_claw;

// Intended position for each servo
float servo_pos_rotation_target = 90;
float servo_pos_shoulder_target = 90;
float servo_pos_wrist_target = 90;
float servo_pos_claw_target = 90;

// "Actual" (filtered) position for each servo
float servo_pos_rotation_actual = 90;
float servo_pos_shoulder_actual = 90;
float servo_pos_wrist_actual = 90;
float servo_pos_claw_actual = 90;

// Timestamp (milliseconds) of last output event
uint last_servo_update;
uint last_mdns_update;

// The webserver that handles all the network connections
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Set up the WiFi connection
void setup_wifi()
{
  int nwifi;
  // Put the WiFi into client mode
  WiFi.mode(WIFI_STA);
  // Scan for WiFi networks, return total number available
  Serial.println("Scanning for networks...");
  nwifi = WiFi.scanNetworks();
  // Loop through all available networks looking for one we know
  for (int i = 0; i < nwifi; i++)
  {
    // If the school network is available, connect to it
    if (WiFi.SSID(i) == "CCSdrama")
    {
      Serial.print("Connecting to CCSdrama");
      WiFi.begin("CCSdrama", "2025frozen");
      break;
      // or if home network, use that
    }
    else if (WiFi.SSID(i) == "omegacs.net")
    {
      Serial.print("Connecting to omegacs.net");
      WiFi.begin("omegacs.net", "b5a0897f7a");
      break;
    }
  }
  // Wait for WiFi to connect
  while (WiFi.status() != WL_CONNECTED)
  {
    // Pause for half a second
    delay(500);
    Serial.print(".");
  }
  // Output the IP address acquired from the network
  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


void handle_websocket_data(void *arg, uint8_t *data, size_t len) {
  //Serial.printf("Got websocket: %.*s\n", len, data);
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
  
    //const uint8_t size = JSON_OBJECT_SIZE(1);
    JsonDocument json;
    DeserializationError err = deserializeJson(json, data);
    if (err) {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(err.c_str());
      return;
    }

    if (json.containsKey("rotation")) {
      servo_pos_rotation_target = json["rotation"];
      Serial.printf("ws new rotation is %f\n", servo_pos_rotation_target);
    }
    if (json.containsKey("shoulder")) {
      servo_pos_shoulder_target = json["shoulder"];
      Serial.printf("ws new shoulder is %f\n", servo_pos_shoulder_target);
    }
    if (json.containsKey("claw")) {
      servo_pos_claw_target = json["claw"];
      Serial.printf("ws new claw is %f\n", servo_pos_claw_target);
    }
  }
}

void on_websocket_event(AsyncWebSocket *server,
                        AsyncWebSocketClient *client,
                        AwsEventType type,
                        void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("Websocket connect client #%u\n", client->id());
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("Websocket disconnect client #%u\n", client->id());
    break;
  case WS_EVT_DATA:
    handle_websocket_data(arg, data, len);
    break;
  }
}

void setup(void)
{
  // Set up the serial port for debugging
  Serial.begin(115200);

  // Start up the WiFi
  setup_wifi();

  // Advertise as "robotarm.local" to the network
  MDNS.begin("robotarm");

  // Set up the flash filesystem to serve webpages and JavaScript from
  if (!LittleFS.begin(true))
  {
    //    while (1) {
    //      Serial.println("Failed to start littlefs");
    //      delay(500);
    //    }
  }

  // Set up default headers to resolve CORS access issues
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

  // Listen for /servo for commands to the arm
  server.on("/servo", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              String ratestr;
              // Return a success "page"
              request->send(200, "text/plain", "Set");
              // If the incoming request has a "rotation" parameter
              if (request->hasParam("rotation"))
              {
                // Extract the string
                ratestr = request->getParam("rotation")->value();
                // Convert the string to an integer
                servo_pos_rotation_target = ratestr.toInt();
                // Output some debugging
              }
              // Handle the "shoulder" parameter the same way
              if (request->hasParam("shoulder"))
              {
                ratestr = request->getParam("shoulder")->value();
                servo_pos_shoulder_target = ratestr.toInt();
              }
              // Serial.print("New target rotation: ");
              // Serial.print(servo_pos_rotation_target);
              // Serial.print("  shoulder: ");
              // Serial.println(servo_pos_shoulder_target);
            });

  // Set up to serve static files from SPIFFS, with a default index.html
  // server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Set up a handler for unknown files
  server.onNotFound([](AsyncWebServerRequest *request)
                    {
    // If it's an OPTION request, answer it (with the headers above)
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    // Otherwise return a 404 Not Found
    } else {
      request->send(404,"Not found");
    } });

  ws.onEvent(on_websocket_event);
  server.addHandler(&ws);

  // Set up the servo objects with the appropriate pins
  servo_rotation.attach(SERVO_PIN_ROTATION);
  servo_shoulder.attach(SERVO_PIN_SHOULDER);
  servo_wrist.attach(SERVO_PIN_WRIST);
  servo_claw.attach(SERVO_PIN_CLAW);

  // Start the webserver
  server.begin();

  // Prime the action timer with the current time
  last_servo_update = millis();
  last_mdns_update = millis();
}

#define AVERAGE 0.95

void loop(void)
{
  // If 10 milliseconds has elapsed since the last action, do the action
  if (millis() >= (last_servo_update + 10))
  {
    // Calculate rolling average for the servo positions
    servo_pos_rotation_actual = (servo_pos_rotation_actual * AVERAGE) + (servo_pos_rotation_target * (1 - AVERAGE));
    servo_pos_shoulder_actual = (servo_pos_shoulder_actual * AVERAGE) + (servo_pos_shoulder_target * (1 - AVERAGE));
    servo_pos_claw_actual = (servo_pos_claw_actual * AVERAGE) + (servo_pos_claw_target * (1 - AVERAGE));

    // Serial.print("New actual rotation: ");
    // Serial.print(servo_pos_rotation_actual);
    // Serial.print("  shoulder: ");
    // Serial.println(servo_pos_shoulder_actual);

    // Write the actual servo positions to the hardware
    servo_rotation.write(servo_pos_rotation_actual);
    servo_shoulder.write(servo_pos_shoulder_actual);
    servo_claw.write(servo_pos_claw_actual);
    

    // Update the last action timer to the current time
    last_servo_update = millis();
  }
  if (millis() >= (last_mdns_update + 5000))
  {
    // This doesn't actually seem to exist, so ....?
    // MDNS.update()
  }
}