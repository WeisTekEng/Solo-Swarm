// ESP32-S3 SuperMini Bare-Bones Bitcoin Miner
// Maximum performance - minimal overhead
// Reports to Core2 dashboard via UDP

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "mbedtls/md.h"

// ============ CONFIGURATION ============
#define WIFI_SSID "your_ssid"
#define WIFI_PASSWORD "your_password"
#define POOL_URL "solo.ckpool.org"
#define POOL_PORT 3333
#define ADDRESS "your_bitcoin_address"
#define MINER_ID 1  // Change for each board: 1, 2, 3, 4, 5
#define CORE2_IP "192.168.1.100"  // IP of your Core2
#define CORE2_PORT 8888
#define MAX_NONCE 0xFFFFFFFF

// Stats reporting interval
#define REPORT_INTERVAL 2000  // Report to Core2 every 2 seconds

// Global stats
volatile unsigned long hashes = 0;
volatile int shares = 0;
volatile int valids = 0;
volatile float temperature = 0;

WiFiUDP udp;
unsigned long lastReport = 0;

// Optimized hex conversion
inline uint8_t hex(char ch) {
    return (ch > 57) ? (ch - 55) : (ch - 48);
}

// Fast byte array conversion
int to_byte_array(const char *in, size_t in_size, uint8_t *out) {
    int count = 0;
    while (*in && count < in_size / 2) {
        *out++ = (hex(*in++) << 4) | hex(*in++);
        count++;
    }
    return count;
}

// Inline hash validation
inline bool checkShare(unsigned char* hash) {
    return (*(uint32_t*)(hash + 28) == 0);
}

inline bool checkValid(unsigned char* hash, unsigned char* target) {
    uint32_t* h = (uint32_t*)hash;
    uint32_t* t = (uint32_t*)target;
    for(int8_t i = 7; i >= 0; i--) {
        if(h[i] > t[i]) return false;
        if(h[i] < t[i]) return true;
    }
    return true;
}

// Send stats to Core2 via UDP
void reportStats() {
    unsigned long now = millis();
    if (now - lastReport < REPORT_INTERVAL) return;
    lastReport = now;
    
    // Format: "ID,HASHRATE,SHARES,VALIDS,TEMP"
    char buffer[64];
    float hashrate = (float)hashes / (now / 1000.0);
    snprintf(buffer, sizeof(buffer), "%d,%.2f,%d,%d,%.1f", 
             MINER_ID, hashrate, shares, valids, temperature);
    
    udp.beginPacket(CORE2_IP, CORE2_PORT);
    udp.write((uint8_t*)buffer, strlen(buffer));
    udp.endPacket();
}

void setup() {
    Serial.begin(115200);
    
    // Set CPU to max frequency
    setCpuFrequencyMhz(240);
    
    // Disable both watchdogs
    disableCore0WDT();
    disableCore1WDT();
    
    Serial.printf("\nESP32-S3 Miner #%d Starting...\n", MINER_ID);
    Serial.printf("CPU Frequency: %d MHz\n", getCpuFrequencyMhz());
    
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
    
    udp.begin(CORE2_PORT + MINER_ID);
    
    delay(1000);
    Serial.println("Starting mining...\n");
}

void loop() {
    WiFiClient client;
    client.setTimeout(10000);
    client.setNoDelay(true);
    
    // Connect to pool
    if (!client.connect(POOL_URL, POOL_PORT)) {
        Serial.println("Pool connection failed");
        delay(5000);
        return;
    }
    
    // Subscribe
    String payload = "{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[]}\n";
    client.print(payload);
    String line = client.readStringUntil('\n');
    
    // Parse subscription response - simplified
    int extranonce1_start = line.indexOf("\",\"", line.indexOf("result")) + 3;
    int extranonce1_end = line.indexOf("\",", extranonce1_start);
    String extranonce1 = line.substring(extranonce1_start, extranonce1_end);
    
    if (extranonce1.length() == 0) {
        Serial.println("Failed to parse extranonce1");
        client.stop();
        return;
    }
    
    line = client.readStringUntil('\n'); // Difficulty
    
    // Authorize
    payload = "{\"params\":[\"" + String(ADDRESS) + "\",\"password\"],\"id\":2,\"method\":\"mining.authorize\"}\n";
    client.print(payload);
    line = client.readStringUntil('\n');
    
    // Parse job - simplified extraction
    int job_start = line.indexOf("\"params\":[\"") + 11;
    int job_end = line.indexOf("\",\"", job_start);
    String job_id = line.substring(job_start, job_end);
    
    int prev_start = job_end + 3;
    int prev_end = line.indexOf("\",\"", prev_start);
    String prevhash = line.substring(prev_start, prev_end);
    
    int cb1_start = prev_end + 3;
    int cb1_end = line.indexOf("\",\"", cb1_start);
    String coinb1 = line.substring(cb1_start, cb1_end);
    
    int cb2_start = cb1_end + 3;
    int cb2_end = line.indexOf("\",", cb2_start);
    String coinb2 = line.substring(cb2_start, cb2_end);
    
    // Skip merkle parsing for now - assume empty or handle separately
    int ver_start = line.indexOf("\"20000000\"");
    String version = "20000000";
    
    int nbits_start = line.indexOf("\",\"170", ver_start) + 3;
    int nbits_end = line.indexOf("\",\"", nbits_start);
    String nbits = line.substring(nbits_start, nbits_end);
    
    int ntime_start = nbits_end + 3;
    int ntime_end = line.indexOf("\"", ntime_start);
    String ntime = line.substring(ntime_start, ntime_end);
    
    // Read remaining lines
    client.readStringUntil('\n');
    client.readStringUntil('\n');
    
    // Calculate target
    String target = nbits.substring(2);
    int zeros = (int)strtol(nbits.substring(0, 2).c_str(), 0, 16) - 3;
    for (int k = 0; k < zeros; k++) target += "00";
    while (target.length() < 64) target = "0" + target;
    
    uint8_t bytearray_target[32] __attribute__((aligned(4)));
    to_byte_array(target.c_str(), 64, bytearray_target);
    
    for (size_t j = 0; j < 16; j++) {
        uint8_t tmp = bytearray_target[j];
        bytearray_target[j] = bytearray_target[31 - j];
        bytearray_target[31 - j] = tmp;
    }
    
    // Generate extranonce2
    uint32_t extranonce2_a = esp_random();
    uint32_t extranonce2_b = esp_random();
    char extranonce2[17];
    snprintf(extranonce2, sizeof(extranonce2), "%08x%08x", extranonce2_a, extranonce2_b);
    
    // Build coinbase
    String coinbase = coinb1 + extranonce1 + String(extranonce2) + coinb2;
    size_t str_len = coinbase.length() / 2;
    uint8_t* bytearray_coinbase = (uint8_t*)malloc(str_len);
    to_byte_array(coinbase.c_str(), str_len * 2, bytearray_coinbase);
    
    // SHA256 context
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    
    byte interResult[32] __attribute__((aligned(4)));
    byte shaResult[32] __attribute__((aligned(4)));
    
    // Double SHA coinbase
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, bytearray_coinbase, str_len);
    mbedtls_md_finish(&ctx, interResult);
    
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, interResult, 32);
    mbedtls_md_finish(&ctx, shaResult);
    
    free(bytearray_coinbase);
    
    // For simplicity, use coinbase hash as merkle root (no merkle branches for now)
    char merkle_root[65];
    for (int i = 0; i < 32; i++) {
        snprintf(merkle_root + (i * 2), 3, "%02x", shaResult[i]);
    }
    
    // Build block header
    String blockheader = version + prevhash + String(merkle_root) + nbits + ntime + "00000000";
    uint8_t bytearray_blockheader[80] __attribute__((aligned(4)));
    to_byte_array(blockheader.c_str(), 160, bytearray_blockheader);
    
    // Reverse version
    for (size_t j = 0; j < 2; j++) {
        uint8_t tmp = bytearray_blockheader[j];
        bytearray_blockheader[j] = bytearray_blockheader[3 - j];
        bytearray_blockheader[3 - j] = tmp;
    }
    
    // Reverse merkle
    for (size_t j = 0; j < 16; j++) {
        uint8_t tmp = bytearray_blockheader[36 + j];
        bytearray_blockheader[36 + j] = bytearray_blockheader[67 - j];
        bytearray_blockheader[67 - j] = tmp;
    }
    
    // Reverse difficulty
    for (size_t j = 0; j < 2; j++) {
        uint8_t tmp = bytearray_blockheader[72 + j];
        bytearray_blockheader[72 + j] = bytearray_blockheader[75 - j];
        bytearray_blockheader[75 - j] = tmp;
    }
    
    // MINING LOOP - Maximum speed
    uint32_t nonce = 0;
    uint32_t local_hashes = 0;
    
    Serial.printf("Mining job %s...\n", job_id.c_str());
    
    while (nonce < MAX_NONCE) {
        // Update nonce
        bytearray_blockheader[76] = nonce & 0xFF;
        bytearray_blockheader[77] = (nonce >> 8) & 0xFF;
        bytearray_blockheader[78] = (nonce >> 16) & 0xFF;
        bytearray_blockheader[79] = (nonce >> 24) & 0xFF;
        
        // Double SHA256
        mbedtls_md_starts(&ctx);
        mbedtls_md_update(&ctx, bytearray_blockheader, 80);
        mbedtls_md_finish(&ctx, interResult);
        
        mbedtls_md_starts(&ctx);
        mbedtls_md_update(&ctx, interResult, 32);
        mbedtls_md_finish(&ctx, shaResult);
        
        local_hashes++;
        nonce++;
        
        // Check share (32-bit zeros)
        if (checkShare(shaResult)) {
            shares++;
            Serial.printf("Share found! Nonce: %u\n", nonce - 1);
            
            // Check if valid block
            if (checkValid(shaResult, bytearray_target)) {
                valids++;
                Serial.println("========================================");
                Serial.println("    VALID BLOCK FOUND!!!");
                Serial.println("========================================");
                
                char nonceHex[9];
                snprintf(nonceHex, 9, "%08x", nonce - 1);
                payload = "{\"params\":[\"" + String(ADDRESS) + "\",\"" + job_id + 
                          "\",\"" + String(extranonce2) + "\",\"" + ntime + 
                          "\",\"" + String(nonceHex) + "\"],\"id\":1,\"method\":\"mining.submit\"}\n";
                client.print(payload);
                line = client.readStringUntil('\n');
                Serial.println(line);
                break;
            }
        }
        
        // Update global counter and report periodically
        if (local_hashes >= 1000) {
            hashes += local_hashes;
            local_hashes = 0;
            temperature = temperatureRead();
            reportStats();
        }
    }
    
    // Final updates
    hashes += local_hashes;
    reportStats();
    
    mbedtls_md_free(&ctx);
    client.stop();
    
    Serial.println("Job complete, getting new work...");
}