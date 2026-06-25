#include <Arduino.h>
#include <WiFi.h>
#include <math.h>

// ================= 參數設定 =================
const int SCAN_INTERVAL = 1500;  // 掃描間隔 (ms)
const int HISTORY_SIZE = 15;     // 歷史記錄數量 (用於計算標準差)
const float THRESHOLD = 4.5;     // 障礙物偵測閾值 (標準差 dBm)，需依環境微調
// ============================================

int rssiHistory[HISTORY_SIZE];
int historyIndex = 0;
bool isCalibrating = true;
int calibrateCount = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== ESP32-S3 WiFi Obstacle Detection ===");
    
    // 初始化 WiFi 為 Station 模式 (不連接特定 AP，僅用於掃描)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // 初始化歷史記錄
    for (int i = 0; i < HISTORY_SIZE; i++) {
        rssiHistory[i] = -100; 
    }
    
    Serial.println("System Initialized. Calibrating environment...");
}

void loop() {
    // 執行 WiFi 掃描
    int n = WiFi.scanNetworks();

    if (n > 0) {
        // 找出當前環境中最強的 WiFi 訊號作為基準
        int maxRssi = -100;
        for (int i = 0; i < n; ++i) {
            if (WiFi.RSSI(i) > maxRssi) {
                maxRssi = WiFi.RSSI(i);
            }
        }

        // 更新歷史紀錄 (Ring Buffer)
        rssiHistory[historyIndex] = maxRssi;
        historyIndex = (historyIndex + 1) % HISTORY_SIZE;

        // 計算平均 RSSI
        float avgRssi = 0;
        int validCount = 0;
        for (int i = 0; i < HISTORY_SIZE; i++) {
            if (rssiHistory[i] > -100) {
                avgRssi += rssiHistory[i];
                validCount++;
            }
        }

        if (validCount > 0) {
            avgRssi /= validCount;

            // 計算標準差 (Standard Deviation)
            float sumDiffSq = 0;
            for (int i = 0; i < HISTORY_SIZE; i++) {
                if (rssiHistory[i] > -100) {
                    float diff = rssiHistory[i] - avgRssi;
                    sumDiffSq += diff * diff;
                }
            }
            float stdDev = sqrt(sumDiffSq / validCount);

            // 輸出日誌
            Serial.printf("Max RSSI: %3d dBm | Avg: %6.2f | StdDev: %5.2f", maxRssi, avgRssi, stdDev);

            // 校準階段 (前 10 次不觸發警報，讓環境穩定)
            if (isCalibrating) {
                calibrateCount++;
                Serial.println(" [Calibrating...]");
                if (calibrateCount >= 10) {
                    isCalibrating = false;
                    Serial.println(">>> Calibration Done. Monitoring started! <<<");
                }
            } 
            // 偵測階段
            else {
                if (stdDev > THRESHOLD) {
                    Serial.println(" | >>> ALERT: Obstacle/Motion Detected! <<<");
                } else {
                    Serial.println(" | Normal");
                }
            }
        }
    } else {
        Serial.println("No WiFi networks found in range.");
    }

    delay(SCAN_INTERVAL);
}