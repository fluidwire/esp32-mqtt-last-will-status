/*
 * ESP32 MQTT Last-Will Online/Offline Status (JSON presence)
 * ----------------------------------------------------------
 * Device presence tracking with a retained JSON status topic:
 *   - Birth message : {"state":"online", ...ip, rssi, fw, boots} on connect
 *   - Last will     : {"state":"offline"} published by the broker if the
 *                     device vanishes (power cut, WiFi drop, crash)
 *   - Clean shutdown: hold the BOOT button (GPIO0) for 2 s and the sketch
 *                     publishes "offline" itself before disconnecting,
 *                     because a clean disconnect does NOT fire the will.
 *
 * Required library (Library Manager):
 *   - "PubSubClient" by Nick O'Leary
 *
 * Setup:
 *   1. Copy config.h.example to config.h and fill in WiFi + broker.
 *   2. Flash, open Serial Monitor at 115200.
 *   3. mosquitto_sub -h broker.hivemq.com -t 'fluidwire/demo/presence/#' -v
 *
 * (c) 2026 Fluidwire. Released under the MIT License.
 * https://github.com/fluidwire
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include "config.h"

#define FW_VERSION          "1.0.0"
#define SHUTDOWN_BUTTON_PIN 0          // BOOT button, active LOW
#define SHUTDOWN_HOLD_MS    2000UL
#define MQTT_KEEPALIVE_S    10
#define HEARTBEAT_MS        30000UL

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
Preferences prefs;

String statusTopic = String(MQTT_BASE_TOPIC) + "/status";
uint32_t bootCount = 0;
uint32_t lastHeartbeat = 0;
uint32_t buttonDownAt = 0;

void connectWiFi() {
  Serial.print(F("[wifi] connecting to "));
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  Serial.print(F("[wifi] IP: "));
  Serial.println(WiFi.localIP());
}

void publishOnline() {
  char buf[200];
  snprintf(buf, sizeof(buf),
           "{\"state\":\"online\",\"ip\":\"%s\",\"rssi\":%d,\"fw\":\"%s\",\"boots\":%lu,\"uptime_s\":%lu}",
           WiFi.localIP().toString().c_str(), WiFi.RSSI(), FW_VERSION,
           (unsigned long)bootCount, (unsigned long)(millis() / 1000UL));
  mqtt.publish(statusTopic.c_str(), buf, true);   // retained
  Serial.print(F("[mqtt] status -> "));
  Serial.println(buf);
}

void connectMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(MQTT_KEEPALIVE_S);

  while (!mqtt.connected()) {
    Serial.print(F("[mqtt] connecting with last will... "));
    // connect(id, user, pass, willTopic, willQos, willRetain, willMessage)
    bool ok = mqtt.connect(
      MQTT_CLIENT_ID,
      strlen(MQTT_USER) ? MQTT_USER : nullptr,
      strlen(MQTT_PASSWORD) ? MQTT_PASSWORD : nullptr,
      statusTopic.c_str(), 1, true, "{\"state\":\"offline\"}");
    if (ok) {
      Serial.println(F("ok"));
      publishOnline();   // birth message overwrites the retained will
    } else {
      Serial.print(F("failed, state="));
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

void cleanShutdown() {
  Serial.println(F("[power] clean shutdown requested"));
  // A clean DISCONNECT skips the will, so announce offline ourselves.
  mqtt.publish(statusTopic.c_str(), "{\"state\":\"offline\",\"reason\":\"clean\"}", true);
  mqtt.loop();
  delay(100);
  mqtt.disconnect();
  WiFi.disconnect(true);
  Serial.println(F("[power] going to deep sleep (press EN to restart)"));
  Serial.flush();
  esp_deep_sleep_start();
}

void checkButton() {
  if (digitalRead(SHUTDOWN_BUTTON_PIN) == LOW) {
    if (buttonDownAt == 0) buttonDownAt = millis();
    else if (millis() - buttonDownAt >= SHUTDOWN_HOLD_MS) cleanShutdown();
  } else {
    buttonDownAt = 0;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(SHUTDOWN_BUTTON_PIN, INPUT_PULLUP);

  prefs.begin("presence", false);
  bootCount = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", bootCount);
  prefs.end();
  Serial.printf("[boot] #%lu, firmware %s\n", (unsigned long)bootCount, FW_VERSION);

  connectWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();
  checkButton();

  if (millis() - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = millis();
    publishOnline();   // refresh rssi/uptime in the retained status
  }
  delay(20);
}
