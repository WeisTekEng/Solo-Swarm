// ESP32-S3 SuperMini Bare-Bones Bitcoin Miner
// Maximum performance - minimal overhead
// Reports to Core2 dashboard via UDP

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "configs.h"
#include "BitcoinMiner.h"

// Stats reporting interval
#define REPORT_INTERVAL 2000  // Report to Core2 every 2 seconds

WiFiUDP udp;
unsigned long lastReport = 0;
TaskHandle_t statsTaskHandle = nullptr;

// Stats reporting task - runs independently
void statsReportTask(void* param) {
vTaskDelay(20 / portTICK_PERIOD_MS);
unsigned long start = millis();
    while (true) {
        unsigned long now = millis();
        
        
        // Get stats from BitcoinMiner using the mutex
        unsigned long currentHashes = 0;
        int currentShares = 0;
        int currentValids = 0;
        float currentTemp = temperatureRead();
        
        unsigned long elapsed = now - start;
        
        xSemaphoreTake(statsMutex, portMAX_DELAY);
        if (hashes == 65536000) {
            // Prevent overflow

            hashes = 0;
        }
        if (templates == 65536000) {
            // Prevent overflow

            templates = 0;
        }
        if (shares == 65536000) {
            // Prevent overflow

            shares = 0;
        }
        if (valids == 65536000) {
            // Prevent overflow

            valids = 0;
        }
        currentHashes = hashes;
        currentShares = shares;
        currentValids = valids;
        xSemaphoreGive(statsMutex);
        
        // Calculate hashrate
        float hashrate = 0;
        if (elapsed > 0) {
            hashrate = (float)currentHashes / (elapsed / 1000.0f) / 1000.0f;
        }
        
        // Format: "ID,HASHRATE,SHARES,VALIDS,TEMP"
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%d,%.2f,%d,%d,%.1f", 
                 MINER_ID, hashrate, currentShares, currentValids, currentTemp);
        
        // Send UDP packet
        udp.beginPacket(CORE2_IP, CORE2_PORT);
        udp.write((uint8_t*)buffer, strlen(buffer));
        udp.endPacket();
        
        // Also print locally for debugging
        Serial.printf("[Miner #%d] Hash: %.2f kH/s | Shares: %d | Valid: %d | Temp: %.1f°C\n",
                      MINER_ID, hashrate, currentShares, currentValids, currentTemp);
        
        // Wait for next report interval
        vTaskDelay(REPORT_INTERVAL / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);
    
    // Set CPU to max frequency
    setCpuFrequencyMhz(240);
    
    // Disable both watchdogs
    disableCore0WDT();
    disableCore1WDT();
    
    Serial.printf("\n=================================\n");
    Serial.printf("ESP32-S3 Miner #%d Starting...\n", MINER_ID);
    Serial.printf("=================================\n");
    Serial.printf("CPU Frequency: %d MHz\n", getCpuFrequencyMhz());
    Serial.printf("Threads: %d\n", THREADS);
    
    // Connect WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    Serial.print("Connecting WiFi");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts++ < 20) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nWiFi FAILED! Rebooting...");
        delay(3000);
        ESP.restart();
    }
    
    Serial.println("\nWiFi Connected!");
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Reporting to Core2: %s:%d\n", CORE2_IP, CORE2_PORT);
    Serial.printf("=================================\n\n");
    
    // Start UDP
    udp.begin(CORE2_PORT + MINER_ID);
    
    // Initialize global stats mutex (if not already done in BitcoinMiner)
    if (statsMutex == nullptr) {
        statsMutex = xSemaphoreCreateMutex();
    }
    
    // Start stats reporting task on Core 1 (low priority, won't interfere with mining)
    xTaskCreatePinnedToCore(
        statsReportTask,
        "StatsReport",
        4096,
        nullptr,
        2,  // Low priority
        &statsTaskHandle,
        1   // Core 1
    );
    
    // Mining tasks - highest priority for maximum hashrate
    if (THREADS == 2) {
        static BitcoinMiner miner1("M1", 0);
        static BitcoinMiner miner2("M2", 1);
        xTaskCreatePinnedToCore([](void*){ miner1.start(); }, "M1", 35000, nullptr, 2, nullptr, 0);
        xTaskCreatePinnedToCore([](void*){ miner2.start(); }, "M2", 35000, nullptr, 2, nullptr, 1);
    } else if (THREADS == 4) {
        static BitcoinMiner miner1("M1", 0);
        static BitcoinMiner miner2("M2", 1);
        static BitcoinMiner miner3("M3", 0);
        static BitcoinMiner miner4("M4", 1);
        xTaskCreatePinnedToCore([](void*){ miner1.start(); }, "M1", 35000, nullptr, 2, nullptr, 0);
        xTaskCreatePinnedToCore([](void*){ miner2.start(); }, "M2", 35000, nullptr, 2, nullptr, 1);
        xTaskCreatePinnedToCore([](void*){ miner3.start(); }, "M3", 35000, nullptr, 2, nullptr, 0);
        xTaskCreatePinnedToCore([](void*){ miner4.start(); }, "M4", 35000, nullptr, 2, nullptr, 1);
    }
    
    delay(1000);
    Serial.println("Mining started! Stats reporting every 2 seconds...\n");
}

void loop() {
    // Keep WiFi alive
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected! Reconnecting...");
        WiFi.reconnect();
        delay(5000);
    }
    
    // Main loop does nothing - all work is in tasks
    vTaskDelay(10000 / portTICK_PERIOD_MS);
}