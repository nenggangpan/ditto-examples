/*
 * Copyright (c) 2017 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0
 *
 * SPDX-License-Identifier: EPL-2.0
 */
#include <Wire.h>
#include "ESP8266WiFi.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <Adafruit_NeoPixel.h>
#include <PubSubClient.h>
#include <ESP8266Ping.h>
#include <ArduinoJson.h>

#define PIN 13 //NeoPixl
#define BAUD_RATE 115200
#define SEALEVELPRESSURE_HPA (1013.25)
#define MSG_LENGTH 1024

// TODO: set valid WiFi Credentials and SSID
const char* ssid = ;
const char* pass = ;

// MQTT Broker
const char* mqtt_server = "test.mosquitto.org";

// Topics
// inTopic: Channel + thingId
const char* inTopic = "ditto-tutorial/my.test:octopus";
const char* outTopic = "ditto-tutorial/my.test:octopus";
const char* thingId = "octopus";

// 37.187.106.16 -> IP of test.mosquitto.org
const IPAddress remote_ip(37, 187, 106, 16);

WiFiClient wifiClient;
PubSubClient client;
Adafruit_BME680 bme680;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, PIN, NEO_GRB + NEO_KHZ800);
StaticJsonBuffer<MSG_LENGTH> jsonBuffer;

// Function Prototypes
void initSensors();
void readSensors();
void wifiConnect();
void mqttConnect();
void setLED(const char* powerState);

/**
 * subStr
 *  Create a substring from a arbitrary input_string.
 */
char* subStr (const char* input_string, char* separator, int segment_number) {
    char *act, *sub, *ptr;
    static char copy[100];
    int i;

    strcpy(copy, input_string);
    for (i = 1, act = copy; i <= segment_number; i++, act = NULL)
    {
        sub = strtok_r(act, separator, &ptr);
        if (sub == NULL) break;
    }
    return sub;
}

/**
 * Callback for MQTT incoming message
 */
void messageReceived(char* topic, byte* payload, unsigned int length) {

    JsonObject& root = jsonBuffer.parseObject(payload);

    if (root.size() > 0 && root.containsKey("path") && root.containsKey("value")){
        // Get feature to handle and it's value
        const char* path = root["path"];
        const char* payload = root["value"];

        char* command = subStr(path, "/", 3);

        if (strcmp(command, "LED") == 0){
            setLED(payload);
        }
    } else if(!root.containsKey("temp")) {
        Serial.println("[error] - Invalid JSON Object.");
    }
    jsonBuffer.clear();
}

/**
 * Setup the octopus: initialize the sensors and connect to Wifi and MQTT.
 */
void setup() {
    Serial.begin(BAUD_RATE);

    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    Serial.println("Initializing board ...");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // Intitialize Adafruit NeoPixel
    strip.begin();
    // Initialize it as off
    strip.setPixelColor(0, 0, 0, 0);
    strip.show();

    initSensors();
    wifiConnect();
    delay(100);

    Serial.println("[info] - Pinging Host.");
    if (Ping.ping(remote_ip)){
        Serial.println("[info] - Ping successfull.");
    } else {
        Serial.print("[error] - Host not reachable - Wifi Status: ");
        Serial.print(WiFi.status());
        Serial.println();
    }
    delay(100);

    client.setServer(mqtt_server, 1883);
    delay(100);
    client.setClient(wifiClient);
    delay(100);
    client.setCallback(messageReceived);
    delay(100);

    Serial.println("[info] - Setup done. Starting MQTT Connectionloop.");

}

/**
 * Main loop.
 */
void loop() {
    if (!client.connected()) {
        mqttConnect();
    }
    client.loop();
    readSensors();
}

/**
 * wifiConnect
 *  Connect to Wifi.
 */
void wifiConnect(){
    delay(10);
    WiFi.begin(ssid, pass);
    Serial.print("[info] - Connecting to Wifi: ");
    Serial.print(ssid);
    while(WiFi.status() != WL_CONNECTED){
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("[info] - Connected, IP address: ");
    Serial.println(WiFi.localIP());
}

/**
 * mqttConnect
 *  Connect to the mqtt Broker.
 */
void mqttConnect(){
    // Loop until we're reconnected
    while (!client.connected()) {
        Serial.print("[info] - Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect(thingId)) {
            Serial.println("connected!");
            Serial.print("[info] - Subscribed on: ");
            Serial.print(inTopic);
            Serial.println();
            // Once connected, publish an announcement...
            // client.publish(outTopic, "Octopus is online, Baby!");
            // ... and resubscribe
            client.subscribe(inTopic);
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 2 seconds");
            delay(2000);
        }
    }
}

/**
 * initSensors
 *  Initialize the sensors on Octopus board.
 */
void initSensors(){
    if (!bme680.begin(118)){
        Serial.println("Could not find a valid BME680 sensor.");
        while(1);
    }
}

/**
 * setLED
 *  Set's the LED on or off depending on the given char ptr.
 *  @param powerState: "on" | "off"
 */
void setLED(const char* powerState){
    if (strcmp(powerState, "on") == 0){
        // Set LED to red
        strip.setPixelColor(0, 255, 0, 0);
    } else {
        // Set LED to no light -> #off
        strip.setPixelColor(0, 0, 0, 0);
    }
    strip.show();
}

/**
 * readSensors
 *  Read sensors of octopus board and push them to the globally initialized mqtt topic.
 */
void readSensors(){
    // Readable sensors -> reduced on temp and altitude
    JsonObject& root = jsonBuffer.createObject();
    root["temp"] = bme680.readTemperature();
    root["alt"] = bme680.readAltitude(SEALEVELPRESSURE_HPA);
    root["thingId"] = thingId;

    // Transform JSON Object to const char*
    char jsonChar[100];
    root.printTo((char*)jsonChar, root.measureLength() + 1);

    client.publish(outTopic, jsonChar);

    // Clear JSON buffer for further use
    jsonBuffer.clear();
}
