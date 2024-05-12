#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

// =========== LoRa ===============
#include <LoRa.h>
#include <SPI.h>

#define LORA_CS_PIN 5
#define LORA_RST_PIN 14
#define LORA_IRQ_PIN 2
const long LORA_FREQ = 915E6;

// =========== Firebase ===============
#include <Firebase_ESP_Client.h>

#include "addons/RTDBHelper.h"
#include "addons/TokenHelper.h"

#define XSTR(x) STR(x)
#define STR(x) #x

#define WIFI_SSID XSTR(ENV_WIFI_SSID)
#define WIFI_PWD XSTR(ENV_WIFI_PWD)
#define FIREBASE_API_KEY XSTR(ENV_FIREBASE_API_KEY)
#define FIREBASE_DB_ENDPOINT XSTR(ENV_FIREBASE_DB_ENDPOINT)
#define FIREBASE_USER_EMAIL XSTR(ENV_FIREBASE_USER_EMAIL)
#define FIREBASE_USER_PWD XSTR(ENV_FIREBASE_USER_PWD)

// function declaration

/**
 * @brief `runEvery` Check if a specified interval of time has elapsed since the last call.
 *
 * @param[in] interval The time interval in milliseconds to check for.
 * @return True if the specified interval has elapsed since the last call,
 *         otherwise false.
 */
bool runEvery(unsigned long interval);

/**
 * Establishes a connection to a Wi-Fi network using the provided credentials.
 *
 * @brief `connectWifi()` initiates the Wi-Fi connection process by attempting to connect
 * to the specified Wi-Fi network using the provided SSID and password. It then
 * waits for the connection to be established.
 *
 * @note This function relies on the global variables WIFI_SSID and WIFI_PWD, which
 *       should be defined in `.env` file to be passed as a macro defined at compile time
 *
 * @warning This function blocks execution until a Wi-Fi connection is successfully
 *          established. If the connection process fails or takes an extended
 *          duration, it may lead to program unresponsiveness.
 */
void connectWifi();

/**
 * Retrieves the current epoch time in seconds.
 *
 * @brief `getTime()` retrieves the current epoch time, representing the number of seconds
 * elapsed since the Unix epoch. It achieves this by obtaining the
 * current local time and converting it into epoch time.
 *
 * @return The current epoch time in seconds. If the current time cannot be obtained
 *         or converted successfully, the function returns 0.
 *
 * @note The function internally uses the getLocalTime function to obtain the current
 *       local time information.
 *
 * @warning This function may not be suitable for high-precision timing requirements
 *          due to potential delays in obtaining the current time and converting it
 *          to epoch time. For precise timing applications, consider using
 *          alternative methods that offer higher accuracy.
 */
unsigned long getTime();

/**
 * Initializes the Firebase connection and authentication parameters.
 *
 * @brief `initFirebase()` sets up the Firebase configuration, including the API key,
 * database URL, user authentication credentials, and other settings required
 * for Firebase integration. It establishes the connection to the Firebase
 * service and performs user authentication to obtain the user's unique
 * identifier (UID).
 *
 * @note The function sets the Firebase API key and database URL based on
 *       predefined constants (FIREBASE_API_KEY and FIREBASE_DB_ENDPOINT) in `.env` file
 *
 * @note User authentication credentials (email and password) are provided
 *       through predefined constants (FIREBASE_USER_EMAIL and FIREBASE_USER_PWD) in `.env` file.
 *
 * @note After successful authentication, the function retrieves the user's
 *       unique identifier (UID) and constructs the database path for subsequent
 *       data operations.
 *
 * @warning This function may block execution until the authentication process
 *          completes and the UID is obtained. If the authentication process
 *          takes an extended duration or encounters errors, it may lead to
 *          program unresponsiveness.
 */
void initFirebase();

/**
 * Logs data to the Firebase Realtime Database under the appropriate node and timestamp.
 *
 * @brief `logToFirebase` logs data received from a specific node to the Firebase Realtime Database.
 * It constructs the database path based on the node identifier and current timestamp,
 * then sets the JSON data at that path in the database.
 *
 * @param[in] fromNode The identifier of the node from which the data originates.
 * @param[in] jsonData A pointer to the FirebaseJson object containing the data to be logged.
 *
 *
 * @note The constructed database path consists of the parent path (`dbPath`), the node
 *       identifier (`fromNode`), and the current timestamp. This ensures that each set
 *       of data is logged under a unique path based on its origin node and time.
 *
 * @warning This function assumes that the Firebase Realtime Database connection has
 *          been initialized and authenticated prior to calling this function.
 */
void logToFirebase(int fromNode, FirebaseJson* jsonData);

/**
 * Initializes the LoRa transceiver module for receiving messages.
 *
 * @brief `initLoRa` sets up the LoRa transceiver module for message reception. It configures
 * the necessary pins, frequency, and synchronization parameters, and then initializes
 * the LoRa receiver. It continuously attempts to initialize the receiver until success
 * or a maximum number of attempts is reached.
 *
 * @note The LoRa receiver is initialized with the specified frequency (LORA_FREQ) using
 *       the LoRa.begin method. Initialization attempts are made in a loop until successful
 *       or until the maximum number of attempts (15) is reached.
 *
 * @note The sync word is set to 0xF3 to ensure that only messages from compatible LoRa
 *       transceivers are received. This helps avoid interference from other LoRa devices
 *       operating in the same frequency range.
 *
 * @warning This function may block execution until the LoRa receiver is successfully
 *          initialized. If initialization fails repeatedly, it may lead to program
 *          unresponsiveness.
 */
void initLoRa();

/**
 * Handles the reception and processing of LoRa packets.
 *
 * @brief `receiveLoRa` is responsible for receiving and processing LoRa packets. Upon receiving
 * a packet, extracts the data payload from the packet, parses it as JSON,
 * and logs the parsed data to the Firebase Realtime Database.
 *
 * @param[in] packet_size The size of the received packet. If non-zero, indicates the presence
 *                    of a packet to be processed.
 *
 * @note The function extracts the data payload from the received packet and parses it
 *       as a JSON string using the FirebaseJson library.
 *
 * @note Each key-value pair in the JSON data is retrieved and logged to the
 *       Firebase Realtime Database using the logToFirebase function.
 *
 * @warning This function may block execution while processing a received packet. If the
 *          packet processing takes an extended duration or encounters errors, it may lead
 *          to program unresponsiveness.
 */
void receiveLoRa(int packet_size);

/**
 * Checks the type of JSON value and prints the corresponding key-value pair.
 *
 * @brief `checkAndPrintJSONKeyValuePair` checks the type of a JSON value and prints the corresponding key-value pair
 * to the serial monitor. It handles various JSON value types such as string, integer, float,
 * double, boolean, object, array, and null.
 *
 * @param[in] key The key of the JSON key-value pair to be printed.
 * @param[in] result A pointer to the FirebaseJsonData object containing the JSON value.
 *
 * @note The function prints the key followed by the corresponding value to the serial monitor.
 *
 * @warning This function assumes that the provided FirebaseJsonData object (result) contains
 *          valid JSON data. If the provided data is not valid JSON or if the conversion fails,
 *          an error message is printed to the serial monitor.
 */
inline void checkAndPrintJSONKeyValuePair(String key, FirebaseJsonData* result);
