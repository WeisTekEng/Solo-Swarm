# Solo-Swarm
A Lottery ticket swarm miner for the ESP32 S3
Each ESP32 S3 SuperMini is a part of the swarm.

Use the Solo Swarm Dashboard to monitor the swarm ( cluster )
https://github.com/WeisTekEng/Solo-Swarm-Dashboard

```
**For each board:**
1. Change `#define MINER_ID` to 1, 2, 3, 4, 5
2. Set WiFi credentials
3. Flash the firmware
4. Label the board physically (S3-1, S3-2, etc.)

### **2. Configure Core2**
1. Find Core2's IP (shows on boot)
2. Update all S3 boards with `#define CORE2_IP "192.168.1.XXX"`
3. Reflash S3 boards

### **3. Power Up**

1. Connect all 5 S3s to USB hub
2. Power on Core2
3. Wait ~30 seconds for connection
4. Watch cluster view populate!

## **Expected Performance:**

S3-1:  ~30+ KH/s
S3-2:  ~30+ KH/s  
S3-3:  ~30+ KH/s
S3-4:  ~30+ KH/s
S3-5:  ~30+ KH/s
Core2: ~30+ KH/s
───────────────
Total: ~210 KH/s
```
