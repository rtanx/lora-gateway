#include "main.h"

#include <ArduinoJson.h>
#include <time.h>

// Firebase data object
FirebaseData Fdo;
FirebaseAuth Fauth;
FirebaseConfig Fcfg;

// Variable to save user UID
String uid;

// Database main path (to be updated in setup with the user UID
String dbPath;
// Database child nodes
String timePath = "/timestamp";

// Parent node (to be updated in every loop)
String parentPath;

int timestamp;

const char* ntpServer = "pool.ntp.org";

// Timer variables (send new reading every 3 minutes)
unsigned long sendDataPrevMillis = 0ul;
// unsigned long timerDelay = 3 * 60 * 1000;
unsigned long timerDelay = 30 * 1000;

bool signUpOK = false;

bool runEvery(unsigned long interval) {
    static unsigned long previousMillis = 0;
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        return true;
    }
    return false;
}

void connectWifi() {
    WiFi.begin(WIFI_SSID, WIFI_PWD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
}

// Function to gets current epoch time
unsigned long getTime() {
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return 0;
    }
    time(&now);
    return now;
}

void initFirebase() {
    Fcfg.api_key = FIREBASE_API_KEY;
    String db_url = String("https://" FIREBASE_DB_ENDPOINT "/");
    Fcfg.database_url = db_url;

    Fauth.user.email = FIREBASE_USER_EMAIL;
    Fauth.user.password = FIREBASE_USER_PWD;

    Firebase.reconnectNetwork(true);
    Fdo.setResponseSize(4096);

    // Assign the callback function for the long running token generation task
    Fcfg.token_status_callback = tokenStatusCallback;

    // Assign the maximum retry of token generation
    Fcfg.max_token_generation_retry = 5;

    Firebase.begin(&Fcfg, &Fauth);

    Serial.println("Getting User UID");
    while ((Fauth.token.uid) == "") {
        Serial.print(".");
        delay(1000);
    }
    uid = Fauth.token.uid.c_str();
    Serial.print("User UID: ");
    Serial.println(uid);

    dbPath = "/UsersData/";
    dbPath.concat(uid);
    dbPath.concat("/readings");
}

void logToFirebase(int fromNode, FirebaseJson* jsonData) {
    // if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)) {
    // sendDataPrevMillis = millis();
    String nodePath = "/node-";
    nodePath.concat(String(fromNode));
    timestamp = getTime();
    Serial.print("time: ");
    Serial.println(timestamp);

    parentPath = dbPath;
    parentPath.concat(nodePath);
    parentPath.concat("/");
    parentPath.concat(String(timestamp));

    bool ok = Firebase.RTDB.setJSON(&Fdo, parentPath.c_str(), jsonData);
    if (!ok) {
        Serial.printf("Set json error: %s\n", Fdo.errorReason().c_str());
        return;
    }
    Serial.println("===============");
    Serial.println("Set json: OK");
    Serial.println("===============");

    // }
}

void initLoRa() {
    // setup LoRa transceiver module
    LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_IRQ_PIN);

    // using 915E6 freq
    int conn_try = 1;
    int lora_begin_ok = 0;
    Serial.println("Initializing LoRa Receiver");
    while (!lora_begin_ok) {
        lora_begin_ok = LoRa.begin(LORA_FREQ);
        Serial.print(".");
        if (conn_try >= 15) {
            Serial.println();
            Serial.println("LoRa init failed. Check your connections.");
            while (true);
        }
        conn_try++;
        delay(500);
    }
    Serial.println();
    Serial.println("LoRa Receiver Initializing OK!");
    Serial.println("Only receive messages from nodes");
    Serial.println();
    // Change sync word (0xF3) to match the receiver
    // The sync word assures you don't get LoRa messages from other LoRa transceivers
    // ranges from 0-0xFF
    LoRa.setSyncWord(0xF3);
}

void receiveLoRa(int packet_size) {
    if (packet_size) {
        Serial.println("--------------------");
        // print RSSI of packet
        Serial.print("Received packet with RSSI ");
        Serial.println(LoRa.packetRssi());

        // received a packet
        String message = "";
        // read packet
        while (LoRa.available()) {
            message += (char)LoRa.read();
        }

        FirebaseJson Fjson;
        bool is_parsed = Fjson.setJsonData<String>(message);
        if (!is_parsed) {
            Serial.print("JSON deserialization failed: ");
            Serial.print("Position Error: ");
            Serial.println(Fjson.errorPosition());
            return;
        }
        FirebaseJsonData desJson;
        int node_id;
        Serial.println("--------------------");
        Fjson.get(desJson, "node_id");
        checkAndPrintJSONKeyValuePair("node_id", &desJson);
        node_id = desJson.to<int>();
        desJson.clear();
        Fjson.get(desJson, "humidity");
        checkAndPrintJSONKeyValuePair("humidity", &desJson);
        desJson.clear();
        Fjson.get(desJson, "temperature");
        checkAndPrintJSONKeyValuePair("temperature", &desJson);
        desJson.clear();
        Fjson.get(desJson, "wind_speed");
        checkAndPrintJSONKeyValuePair("wind_speed", &desJson);
        desJson.clear();
        Fjson.get(desJson, "water_level");
        checkAndPrintJSONKeyValuePair("water_level", &desJson);
        desJson.clear();
        Serial.println("--------------------");

        // remove node_id from json object
        Fjson.remove("node_id");

        logToFirebase(node_id, &Fjson);
        Fjson.clear();
    }
}

inline void checkAndPrintJSONKeyValuePair(String key, FirebaseJsonData* result) {
    if (result->success) {
        Serial.print(key);
        Serial.print(": ");
        if (result->typeNum == FirebaseJson::JSON_STRING)
            Serial.println(result->to<String>().c_str());
        else if (result->typeNum == FirebaseJson::JSON_INT)
            Serial.println(result->to<int>());
        else if (result->typeNum == FirebaseJson::JSON_FLOAT)
            Serial.println(result->to<float>());
        else if (result->typeNum == FirebaseJson::JSON_DOUBLE)
            Serial.println(result->to<double>());
        else if (result->typeNum == FirebaseJson::JSON_BOOL)
            Serial.println(result->to<bool>());
        else if (result->typeNum == FirebaseJson::JSON_OBJECT)
            Serial.println(result->to<String>().c_str());
        else if (result->typeNum == FirebaseJson::JSON_ARRAY)
            Serial.println(result->to<String>().c_str());
        else if (result->typeNum == FirebaseJson::JSON_NULL)
            Serial.println(result->to<String>().c_str());

        return;
    }
    Serial.println("JSON value to primitive conversion error");
}

void setup() {
    // initialize Serial Monitor
    delay(3000);
    Serial.flush();
    Serial.begin(115200);
    Serial.println("Initializing...");
    delay(3000);
    configTime(0, 0, ntpServer);

    Serial.println("================= Wifi =================");
    connectWifi();

    Serial.println("================= Firebase =================");
    initFirebase();

    Serial.println("================= LoRa Receiver =================");
    initLoRa();
}

void loop() {
    int packet_size = LoRa.parsePacket();
    receiveLoRa(packet_size);
}
