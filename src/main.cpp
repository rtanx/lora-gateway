#include "main.h"

#include <ArduinoJson.h>
#include <time.h>

FirebaseData Fdo;     // Firebase data object
FirebaseAuth Fauth;   // Firebase authentication object
FirebaseConfig Fcfg;  // Firebase configuration object

String uid;  // Variable to save user UID

String dbPath;  // Database main path (to be updated in setup with the user UID

String parentPath;  // Parent node (to be updated in every loop)

const char* ntpServer = "pool.ntp.org";  //  time synchronizzation NTP server address

bool runEvery(unsigned long interval) {
    static unsigned long previousMillis = 0;  // Stores the timestamp of the last call
    unsigned long currentMillis = millis();   // Retrieves the current time

    // Checks if the difference between the current time and the previous time
    // is greater than or equal to the specified interval
    // then, updates the previous time to the current time if true
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
    String db_url = String("https://" FIREBASE_DB_ENDPOINT "/");  // construct firebase db endpoint to full url form
    Fcfg.database_url = db_url;

    Fauth.user.email = FIREBASE_USER_EMAIL;
    Fauth.user.password = FIREBASE_USER_PWD;

    Firebase.reconnectNetwork(true);  // let firebase reconnect to the network if network fail occured
    Fdo.setResponseSize(4096);        // sets the response size for Firebase operations to 4096 bytes,  meaning that the buffer will be able to hold up to 4096 bytes of data when receiving responses from Firebase

    Fcfg.token_status_callback = tokenStatusCallback;  // Assign the callback function for the long running token generation task

    Fcfg.max_token_generation_retry = 5;  // Set maximum retry count for token generation

    // Begin Firebase connection and authentication
    Firebase.begin(&Fcfg, &Fauth);

    // Wait for user authentication and obtain UID
    Serial.println("Getting User UID");
    while ((Fauth.token.uid) == "") {
        Serial.print(".");
        delay(1000);
    }
    uid = Fauth.token.uid.c_str();
    Serial.print("User UID: ");
    Serial.println(uid);

    // Construct database path for user data
    dbPath = "/UsersData/";
    dbPath.concat(uid);
    dbPath.concat("/readings");
}

void logToFirebase(int fromNode, FirebaseJson* jsonData) {
    int timestamp;  // hold current timestamp
    String nodePath = "/node-";
    nodePath.concat(String(fromNode));  // Construct node path
    timestamp = getTime();              // Get current timestamp
    Serial.print("time: ");
    Serial.println(timestamp);

    // Construct parent path
    parentPath = dbPath;
    parentPath.concat(nodePath);
    parentPath.concat("/");
    parentPath.concat(String(timestamp));

    // Set JSON data to Firebase Realtime Database
    bool ok = Firebase.RTDB.setJSON(&Fdo, parentPath.c_str(), jsonData);
    if (!ok) {
        // Print error message if setting JSON data fails
        Serial.printf("Set json error: %s\n", Fdo.errorReason().c_str());
        return;
    }
    Serial.println("===============");
    Serial.println("Set json: OK");
    Serial.println("===============");
}

void initLoRa() {
    // Setup LoRa transceiver module pin configurations
    LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_IRQ_PIN);

    int conn_try = 1;
    int lora_begin_ok = 0;
    Serial.println("Initializing LoRa Receiver");
    while (!lora_begin_ok) {
        lora_begin_ok = LoRa.begin(LORA_FREQ);  // Set frequency to 915E6 Hz and initialize LoRa receiver
        Serial.print(".");
        // Check if maximum attempts reached
        if (conn_try >= 15) {
            Serial.println();
            Serial.println("LoRa init failed. Check your connections.");
            while (true);  // Infinite loop to halt program execution if failed to establish LoRa conenction
        }
        conn_try++;
        delay(500);  // Delay before next initialization attempt
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
        // Print the Received Signal Strength Indicator (RSSI) of the received packet
        Serial.print("Received packet with RSSI ");
        Serial.println(LoRa.packetRssi());

        String message = "";
        // Read packet data and append to the message string
        while (LoRa.available()) {
            message += (char)LoRa.read();
        }

        // Parse the received message as JSON using the FirebaseJson library
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

        // Extract and print each key-value pair from the JSON data
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

        Fjson.remove("node_id");  // Remove node_id from the JSON object, not logged to RTDB, cause already defined as a data path

        logToFirebase(node_id, &Fjson);  // Log the parsed data to the Firebase Realtime Database
        Fjson.clear();                   // Clear the JSON object to free memory
    }
}

inline void checkAndPrintJSONKeyValuePair(String key, FirebaseJsonData* result) {
    if (result->success) {   // Check if JSON value conversion was successful
        Serial.print(key);   // Print the key of the key-value pair
        Serial.print(": ");  // Print separator between key and value

        // Check the type of JSON value and print the corresponding value
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
    Serial.flush();  // Flush any existing data in the serial buffer
    Serial.begin(115200);
    Serial.println("Initializing...");
    delay(3000);  // Additional delay for stability

    // Configure time synchronization using NTP
    configTime(0, 0, ntpServer);

    Serial.println("================= Wifi =================");
    connectWifi();  // Establish Wi-Fi connection

    Serial.println("================= Firebase =================");
    initFirebase();  // Initialize Firebase integration

    Serial.println("================= LoRa Receiver =================");
    initLoRa();  // Set up LoRa receiver
}

void loop() {
    int packet_size = LoRa.parsePacket();  // Check for incoming LoRa packet
    receiveLoRa(packet_size);              // Process the received LoRa packet (if any)
}
