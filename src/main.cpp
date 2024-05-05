#include "main.h"

#include <time.h>

// define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 2

// Firebase data object
FirebaseData Fdo;
FirebaseAuth Fauth;
FirebaseConfig Fcfg;

// Variable to save user UID
String uid;

// Database main path (to be updated in setup with the user UID
String dbPath;
// Database child nodes
String dummySensorPath = "/dummy";
String timePath = "/timestamp";

// Parent node (to be updated in every loop)
String parentPath;

int timestamp;
FirebaseJson json;

const char* ntpServer = "pool.ntp.org";

// Timer variables (send new reading every 3 minutes)
unsigned long sendDataPrevMillis = 0ul;
// unsigned long timerDelay = 3 * 60 * 1000;
unsigned long timerDelay = 30 * 1000;

int count = 0;
bool signUpOK = false;

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

    Firebase.reconnectWiFi(true);
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

void logToFirebase() {
    if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)) {
        sendDataPrevMillis = millis();

        timestamp = getTime();
        Serial.print("time: ");
        Serial.println(timestamp);

        parentPath = dbPath;
        parentPath.concat("/");
        parentPath.concat(String(timestamp));

        json.set(dummySensorPath.c_str(), String(count));
        json.set(timePath.c_str(), String(timestamp));

        bool ok = Firebase.RTDB.setJSON(&Fdo, parentPath.c_str(), &json);
        if (!ok) {
            Serial.printf("Set json error: %s\n", Fdo.errorReason().c_str());
            return;
        }
        Serial.println("===============");
        Serial.println("Set json: OK");
        Serial.printf("Dummy: %s\nTimestamp:%s\n", String(count), String(timestamp));
        Serial.println("===============");

        count++;
    }
}

void setup() {
    // initialize Serial Monitor
    Serial.flush();
    Serial.begin(115200);
    Serial.println("Initializing...");
    delay(3000);
    configTime(0, 0, ntpServer);

    Serial.println("================= LoRa Receiver =================");

    // setup LoRa transceiver module
    LoRa.setPins(ss, rst, dio0);

    // replace the LoRa.begin(---E-) argument with your location's frequency
    // 433E6 for Asia
    // 866E6 for Europe
    // 915E6 for North America
    while (!LoRa.begin(915E6)) {
        Serial.println(".");
        delay(500);
    }
    // Change sync word (0xF3) to match the receiver
    // The sync word assures you don't get LoRa messages from other LoRa transceivers
    // ranges from 0-0xFF
    LoRa.setSyncWord(0xF3);
    Serial.println("LoRa Initializing OK!");

    Serial.println("================= Wifi =================");
    connectWifi();

    Serial.println("================= Firebase =================");
    initFirebase();
}

void loop() {
    // try to parse packet
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        // received a packet
        Serial.print("Received packet '");

        // read packet
        while (LoRa.available()) {
            String LoRaData = LoRa.readString();
            Serial.print(LoRaData);
        }

        // print RSSI of packet
        Serial.print("' with RSSI ");
        Serial.println(LoRa.packetRssi());
    }

    logToFirebase();
}
