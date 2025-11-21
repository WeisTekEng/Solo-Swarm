
#ifndef CONFIGS_H
#define CONFIGS_H
static const int  MINER_ID = 1;  // Change for each board: 1, 2, 3, 4, 5

// Core2
static const char* CORE2_IP = "192.168.1.100";  // IP of your Core2
static const int CORE2_PORT = 8888;

// Wifi
static const char* WIFI_SSID = "YOUR_WIFI_SSID"; // 2.5Ghz
static const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD!";

// Mining
static const int THREADS = 4;
static const unsigned long MAX_NONCE = 0xFFFFFFFFUL;
static const char* ADDRESS = "bc1qpe8gjgfs5hh0aw7veusxqppycyz0ea0nvjxr3k";

// Pool
static const char* POOL_URL = "solo.ckpool.org";
static const int POOL_PORT = 3333;
static const bool DEBUG = true;
#endif // CONFIGS_H
