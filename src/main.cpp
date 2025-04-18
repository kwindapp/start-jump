#include <M5Core2.h>
#include <math.h>

float takeoffThreshold = 1.2;            // Acceleration threshold to detect takeoff
float landingThresholdLow = 0.9;         // Lower bound of acceleration for landing
float landingThresholdHigh = 1.3;        // Upper bound of acceleration for landing
float gravityFactor = 1.0;               // Adjust gravity for height calculation
float accelSmoothing = 0.2;              // Amount of smoothing on acceleration data
float maxJumpHeight = 40.0;              // Maximum height for jump detection (in cm)
float minFlightTime = 0.1;               // Minimum valid jump flight time
int jumpStartDebounce = 1000;            // Debounce time to avoid multiple jump detections
int minAccelDuration = 50;               // Minimum time above threshold before jump is valid
int landingHoldDuration = 50;            // Duration to hold the landing threshold for validation
float airtimeTimeout = 5000;             // Maximum time before jump is considered over
float impactThreshold = 3.0;             // G-force threshold for detecting landing impact
int minLandingDetectTime = 200;          // Minimum time after takeoff before detecting landing


int menuIndex = 0;
bool inMenu = true;
bool inAir = false;
unsigned long takeoffTime = 0;
unsigned long landingTime = 0;
unsigned long currentTime = 0;
unsigned long lastJumpTime = 0;
unsigned long accelOverThresholdStart = 0;

#define MAX_JUMPS 5
float jumpHeights[MAX_JUMPS] = {0};
float airTimes[MAX_JUMPS] = {0};

unsigned long lastPrintTime = 0;
const unsigned long printInterval = 1000;

void drawButtons() {
  M5.Lcd.fillRect(0, 200, 320, 40, DARKGREY);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(BLACK);

  M5.Lcd.fillRect(5, 205, 70, 30, WHITE);
  M5.Lcd.setCursor(20, 210); M5.Lcd.print("UP");

  M5.Lcd.fillRect(85, 205, 70, 30, WHITE);
  M5.Lcd.setCursor(95, 210); M5.Lcd.print("DOWN");

  M5.Lcd.fillRect(165, 205, 70, 30, WHITE);
  M5.Lcd.setCursor(190, 210); M5.Lcd.print("+");

  M5.Lcd.fillRect(245, 205, 70, 30, WHITE);
  M5.Lcd.setCursor(270, 210); M5.Lcd.print("-");
}

void drawMenu() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(0, 0);

  const char* labels[] = {
    "TakeoffThresh", "Land Thresh Low", "Land Thresh High",
    "Gravity Factor", "Accel Smoothing", "Max Jump Height", "Min Flight Time",
    "Jump Debounce", "Accel Hold(ms)", "Landing Hold(ms)", "Max Airtime(ms)",
    "Impact G", "LandDelay(ms)", "Exit"
  };

  float* values[] = {
    &takeoffThreshold, &landingThresholdLow, &landingThresholdHigh,
    &gravityFactor, &accelSmoothing, &maxJumpHeight, &minFlightTime, &impactThreshold
  };

  for (int i = 0; i < 14; i++) {
    if (i == menuIndex) M5.Lcd.print("> "); else M5.Lcd.print("  ");
    if (i < 8) M5.Lcd.printf("%s: %.2f\n", labels[i], *values[i]);
    else if (i == 8) M5.Lcd.printf("%s: %d\n", labels[i], jumpStartDebounce);
    else if (i == 9) M5.Lcd.printf("%s: %d\n", labels[i], minAccelDuration);
    else if (i == 10) M5.Lcd.printf("%s: %d\n", labels[i], landingHoldDuration);
    else if (i == 11) M5.Lcd.printf("%s: %d\n", labels[i], (int)airtimeTimeout);
    else if (i == 12) M5.Lcd.printf("%s: %d\n", labels[i], minLandingDetectTime);
    else M5.Lcd.println("Exit");
  }

  drawButtons();
}

void drawJumpGraph() {
  M5.Lcd.fillRect(0, 50, 320, 200, BLACK);
  int startX = 20, baseY = 200, barWidth = 40, maxHeight = 120;
  float maxJump = 0.1;
  for (int i = 0; i < MAX_JUMPS; i++) if (jumpHeights[i] > maxJump) maxJump = jumpHeights[i];

  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(YELLOW, BLACK);

  for (int i = 0; i < MAX_JUMPS; i++) {
    int x = startX + i * (barWidth + 20);
    int h = (int)(min(jumpHeights[i] / maxJump, 1.0f) * maxHeight);
    int y = baseY - h;
    M5.Lcd.setTextSize(1);
    M5.Lcd.fillRect(x, y, barWidth, h, (inAir ? GREEN : TFT_CYAN));
    M5.Lcd.setTextSize(2);

    char timeBuffer[8];
    sprintf(timeBuffer, "%.2fs", airTimes[i]);
    int timeTextWidth = M5.Lcd.textWidth(timeBuffer);
    M5.Lcd.setCursor(x + (barWidth - timeTextWidth) / 2, y - 20);
    M5.Lcd.print(timeBuffer);

    char heightBuffer[8];
    sprintf(heightBuffer, "%.0f", jumpHeights[i]);
    int heightTextWidth = M5.Lcd.textWidth(heightBuffer);
    M5.Lcd.setCursor(x + (barWidth - heightTextWidth) / 2, baseY + 5);
    M5.Lcd.print(heightBuffer);
  }

  M5.Lcd.fillRect(100, 260, 120, 50, RED);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(120, 275);
  M5.Lcd.print("RESET");
}

void resetJumps() {
  for (int i = 0; i < MAX_JUMPS; i++) {
    jumpHeights[i] = 0;
    airTimes[i] = 0;
  }
  takeoffTime = landingTime = currentTime = lastJumpTime = 0;
  inAir = false;

  M5.Lcd.fillRect(0, 30, 320, 30, BLACK);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(10, 30);
  M5.Lcd.print("Time: 0.00s");
  M5.Lcd.setCursor(15, 0);
  M5.Lcd.print("Jump(cm)");
  drawJumpGraph();
}

float calculateJumpHeight(float flightTime) {
  float h = 0.5 * 9.81 * pow(flightTime / 2.0, 2);
  return min(h, maxJumpHeight);
}

void setup() {
  M5.begin();
  M5.IMU.Init();
  Serial.begin(115200);
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("Ready to Adjust Parameters!");
  drawMenu();
}

void loop() {
  M5.update();
  TouchPoint_t p = M5.Touch.getPressPoint();

  if (inMenu) {
    if (p.y > 200 && p.y < 240) {
      if (p.x > 5 && p.x < 75) { menuIndex = (menuIndex - 1 + 14) % 14; drawMenu(); delay(200); }
      else if (p.x > 85 && p.x < 155) { menuIndex = (menuIndex + 1) % 14; drawMenu(); delay(200); }
      else if (p.x > 165 && p.x < 235) {
        if (menuIndex == 0) takeoffThreshold = min(takeoffThreshold + 0.1, 2.0);
        else if (menuIndex == 1) landingThresholdLow = min(landingThresholdLow + 0.1, 1.5);
        else if (menuIndex == 2) landingThresholdHigh = min(landingThresholdHigh + 0.1, 1.5);
        else if (menuIndex == 3) gravityFactor = min(gravityFactor + 0.1, 1.5);
        else if (menuIndex == 4) accelSmoothing = min(accelSmoothing + 0.05, 1.0);
        else if (menuIndex == 5) maxJumpHeight = min(maxJumpHeight + 0.05, 2.0);
        else if (menuIndex == 6) minFlightTime = min(minFlightTime + 0.1, 1.0);
        else if (menuIndex == 7) impactThreshold = min(impactThreshold + 0.1, 5.0);
        else if (menuIndex == 8) jumpStartDebounce += 50;
        else if (menuIndex == 9) minAccelDuration += 10;
        else if (menuIndex == 10) landingHoldDuration += 10;
        else if (menuIndex == 11) airtimeTimeout += 500;
        else if (menuIndex == 12) minLandingDetectTime += 10;
        drawMenu(); delay(200);
      }
      else if (p.x > 245 && p.x < 315) {
        if (menuIndex == 0) takeoffThreshold = max(takeoffThreshold - 0.1, 0.1);
        else if (menuIndex == 1) landingThresholdLow = max(landingThresholdLow - 0.1, 0.1);
        else if (menuIndex == 2) landingThresholdHigh = max(landingThresholdHigh - 0.1, 0.1);
        else if (menuIndex == 3) gravityFactor = max(gravityFactor - 0.1, 0.1);
        else if (menuIndex == 4) accelSmoothing = max(accelSmoothing - 0.05, 0.0);
        else if (menuIndex == 5) maxJumpHeight = max(maxJumpHeight - 0.05, 0.1);
        else if (menuIndex == 6) minFlightTime = max(minFlightTime - 0.1, 0.0);
        else if (menuIndex == 7) impactThreshold = max(impactThreshold - 0.1, 0.5);
        else if (menuIndex == 8) jumpStartDebounce = max(jumpStartDebounce - 50, 100);
        else if (menuIndex == 9) minAccelDuration = max(minAccelDuration - 10, 10);
        else if (menuIndex == 10) landingHoldDuration = max(landingHoldDuration - 10, 10);
        else if (menuIndex == 11) airtimeTimeout = max(airtimeTimeout - 500.0f, 1000.0f);
        else if (menuIndex == 12) minLandingDetectTime = max(minLandingDetectTime - 10, 0);
        drawMenu(); delay(200);
      }
    }

    if (menuIndex == 13) {
      inMenu = false;
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setTextSize(2);
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.println("Ready to Jump!");
      resetJumps();
      delay(500);
    }
    return;
  }

  float ax, ay, az;
  M5.IMU.getAccelData(&ax, &ay, &az);
  float totalAccel = sqrt(ax * ax + ay * ay + az * az);
  unsigned long now = millis();

  if (now - lastPrintTime >= printInterval) {
    Serial.printf("Accel: %.2f, inAir: %d\n", totalAccel, inAir);
    lastPrintTime = now;
  }

  static unsigned long landingAccelStart = 0;

  if (!inAir && totalAccel > takeoffThreshold) {
    if (accelOverThresholdStart == 0) accelOverThresholdStart = now;
    if (now - accelOverThresholdStart > minAccelDuration && (now - lastJumpTime > jumpStartDebounce)) {
      takeoffTime = now;
      inAir = true;
      lastJumpTime = now;
      Serial.println("Jump detected!");
    }
  } else {
    accelOverThresholdStart = 0;
  }

  if (inAir) {
    currentTime = now - takeoffTime;
    M5.Lcd.fillRect(0, 30, 320, 30, BLACK);
    M5.Lcd.setCursor(10, 30);
    M5.Lcd.printf("Time: %.2fs", currentTime / 1000.0);

    if (totalAccel > impactThreshold) {
      landingTime = now;
      float flightTime = (landingTime - takeoffTime) / 1000.0;
      if (flightTime > minFlightTime) {
        inAir = false;
        float heightCm = calculateJumpHeight(flightTime) * 100;
        Serial.printf("Impact detected! G-force: %.2fG\n", totalAccel);
        for (int i = 0; i < MAX_JUMPS - 1; i++) {
          jumpHeights[i] = jumpHeights[i + 1];
          airTimes[i] = airTimes[i + 1];
        }
        jumpHeights[MAX_JUMPS - 1] = heightCm;
        airTimes[MAX_JUMPS - 1] = flightTime;
        drawJumpGraph();
        delay(200);
        return;
      }
    }

    if (currentTime > airtimeTimeout) {
      landingTime = now;
      float flightTime = (landingTime - takeoffTime) / 1000.0;
      if (flightTime > minFlightTime) {
        inAir = false;
        float heightCm = calculateJumpHeight(flightTime) * 100;
        Serial.printf("Airtime timeout! Jump landed after %.2fs, Height: %.2f cm\n", flightTime, heightCm);
        for (int i = 0; i < MAX_JUMPS - 1; i++) {
          jumpHeights[i] = jumpHeights[i + 1];
          airTimes[i] = airTimes[i + 1];
        }
        jumpHeights[MAX_JUMPS - 1] = heightCm;
        airTimes[MAX_JUMPS - 1] = flightTime;
        drawJumpGraph();
        delay(200);
      }
    }

    if ((now - takeoffTime) > minLandingDetectTime &&
        totalAccel > landingThresholdLow &&
        totalAccel < landingThresholdHigh) {
      if (landingAccelStart == 0) landingAccelStart = now;

      if ((now - landingAccelStart) >= landingHoldDuration) {
        landingTime = now;
        float flightTime = (landingTime - takeoffTime) / 1000.0;
        if (flightTime > minFlightTime) {
          inAir = false;
          float heightCm = calculateJumpHeight(flightTime) * 100;
          Serial.printf("Jump landed! Time: %.2fs, Height: %.2f cm\n", flightTime, heightCm);
          Serial.printf("Landing after %d ms delay allowed\n", minLandingDetectTime);
          for (int i = 0; i < MAX_JUMPS - 1; i++) {
            jumpHeights[i] = jumpHeights[i + 1];
            airTimes[i] = airTimes[i + 1];
          }
          jumpHeights[MAX_JUMPS - 1] = heightCm;
          airTimes[MAX_JUMPS - 1] = flightTime;
          drawJumpGraph();
          delay(200);
        }
      }
    } else {
      landingAccelStart = 0;
    }
   
  }
    // Handle the RESET button press in jump graph
    if (M5.Touch.ispressed()) {
      TouchPoint_t touch = M5.Touch.getPressPoint();
      if (touch.x > 100 && touch.x < 220 && touch.y > 260 && touch.y < 310) {
        resetJumps();  // Reset jumps and menu
        delay(500);  // Debounce delay
}
}
}
