#include <M5Core2.h>  // Use M5Core2 directly without M5Unified

// ====== TUNABLE PARAMETERS ======
float takeoffThreshold = 1.2;
float landingThresholdLow = 0.9;
float landingThresholdHigh = 1.1;
float gravityFactor = 1.0;
float accelSmoothing = 0.2;
float maxJumpHeight = 0.1;
float minFlightTime = 0.1;
int jumpStartDebounce = 1000;       // NEW: debounce time in ms
int minAccelDuration = 50;          // NEW: min accel threshold duration in ms
int landingHoldDuration = 50;       // NEW: duration to confirm landing (ms)
// ================================

int menuIndex = 0;
bool inMenu = true;
bool inAir = false;
unsigned long takeoffTime = 0;
unsigned long landingTime = 0;
unsigned long currentTime = 0;
unsigned long lastJumpTime = 0;
unsigned long accelOverThresholdStart = 0;

// Graph Data
#define MAX_JUMPS 5
float jumpHeights[MAX_JUMPS] = {0};
float airTimes[MAX_JUMPS] = {0};

// Menu Interface
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

// Draw Menu
void drawMenu() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(0, 0);

  const char* labels[] = {
    "TakeoffThresh", "Land Thresh Low", "Land Thresh High",
    "Gravity Factor", "Accel Smoothing", "Max Jump Height", "Min Flight Time",
    "Jump Debounce", "Accel Hold(ms)", "Landing Hold(ms)", "Exit"
  };
  float* values[] = {
    &takeoffThreshold, &landingThresholdLow, &landingThresholdHigh,
    &gravityFactor, &accelSmoothing, &maxJumpHeight, &minFlightTime
  };

  for (int i = 0; i < 11; i++) {
    if (i == menuIndex) M5.Lcd.print("> "); else M5.Lcd.print("  ");
    if (i < 7) M5.Lcd.printf("%s: %.2f\n", labels[i], *values[i]);
    else if (i == 7) M5.Lcd.printf("%s: %d\n", labels[i], jumpStartDebounce);
    else if (i == 8) M5.Lcd.printf("%s: %d\n", labels[i], minAccelDuration);
    else if (i == 9) M5.Lcd.printf("%s: %d\n", labels[i], landingHoldDuration);
    else M5.Lcd.println("Exit");
  }

  drawButtons();
}

// Graph Update Function with height in cm
void drawJumpGraph() {
  M5.Lcd.fillRect(0, 50, 320, 200, BLACK);
  int startX = 20, baseY = 200, barWidth = 40, maxHeight = 120;

  float maxJump = 0.1; // maximum height to scale the bars
  for (int i = 0; i < MAX_JUMPS; i++) {
    if (jumpHeights[i] > maxJump) maxJump = jumpHeights[i];
  }

  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(YELLOW, BLACK);

  for (int i = 0; i < MAX_JUMPS; i++) {
    int x = startX + i * (barWidth + 20);
    int h = (int)(min(jumpHeights[i] / maxJump, 1.0f) * maxHeight);
    int y = baseY - h;

    M5.Lcd.fillRect(x, y, barWidth, h, GREEN);

    // Display height in centimeters below the bars
    char hbuf[6];
    sprintf(hbuf, "%.0f cm", jumpHeights[i]);
    int htw = M5.Lcd.textWidth(hbuf);
    M5.Lcd.setCursor(x + (barWidth - htw) / 2, baseY + 5); // Position the height text below the bars
    M5.Lcd.printf("%s", hbuf);

    // Display flight time in seconds above the bars
    char tbuf[6];
    sprintf(tbuf, "%.2fs", airTimes[i]);
    int ttw = M5.Lcd.textWidth(tbuf);
    M5.Lcd.setCursor(x + (barWidth - ttw) / 2, baseY - h - 20);
    M5.Lcd.printf("%s", tbuf);
  }

  // Reset Button at the bottom
  M5.Lcd.fillRect(100, 260, 120, 50, RED);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(120, 275);
  M5.Lcd.print("RESET");
}

// Function to reset the jump data, graph, and timer
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

// Function to calculate jump height based on flight time
float calculateJumpHeight(float flightTime) {
  return 0.5 * 9.81 * pow(flightTime / 2.0, 2);  // Height formula
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
      if (p.x > 5 && p.x < 75) { menuIndex = (menuIndex - 1 + 11) % 11; drawMenu(); delay(200); }
      else if (p.x > 85 && p.x < 155) { menuIndex = (menuIndex + 1) % 11; drawMenu(); delay(200); }
      else if (p.x > 165 && p.x < 235) {
        if (menuIndex == 0) takeoffThreshold = min(takeoffThreshold + 0.1, 2.0);
        else if (menuIndex == 1) landingThresholdLow = min(landingThresholdLow + 0.1, 1.5);
        else if (menuIndex == 2) landingThresholdHigh = min(landingThresholdHigh + 0.1, 1.5);
        else if (menuIndex == 3) gravityFactor = min(gravityFactor + 0.1, 1.5);
        else if (menuIndex == 4) accelSmoothing = min(accelSmoothing + 0.05, 1.0);
        else if (menuIndex == 5) maxJumpHeight = min(maxJumpHeight + 0.05, 2.0);
        else if (menuIndex == 6) minFlightTime = min(minFlightTime + 0.1, 1.0);
        else if (menuIndex == 7) jumpStartDebounce += 50;
        else if (menuIndex == 8) minAccelDuration += 10;
        else if (menuIndex == 9) landingHoldDuration += 10;
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
        else if (menuIndex == 7) jumpStartDebounce = max(jumpStartDebounce - 50, 100);
        else if (menuIndex == 8) minAccelDuration = max(minAccelDuration - 10, 10);
        else if (menuIndex == 9) landingHoldDuration = max(landingHoldDuration - 10, 10);
        drawMenu(); delay(200);
      }
    }

    if (menuIndex == 10) {
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

  // Detect sustained takeoff
  if (!inAir && totalAccel > takeoffThreshold) {
    if (accelOverThresholdStart == 0) accelOverThresholdStart = now;
    if (now - accelOverThresholdStart > minAccelDuration && (now - lastJumpTime > jumpStartDebounce)) {
      takeoffTime = now;
      inAir = true;
      lastJumpTime = now;
    }
  } else {
    accelOverThresholdStart = 0;
  }

  if (inAir) {
    currentTime = now - takeoffTime;
    M5.Lcd.fillRect(0, 30, 320, 30, BLACK);
    M5.Lcd.setCursor(10, 30);
    M5.Lcd.printf("Time: %.2fs", currentTime / 1000.0);
  }

  // Check for landing
  static unsigned long landingAccelStart = 0;

  if (inAir && totalAccel > landingThresholdLow && totalAccel < landingThresholdHigh) {
    landingTime = now;
    float flightTime = (landingTime - takeoffTime) / 1000.0;  // Flight time in seconds

    if (flightTime > minFlightTime) {
        inAir = false;

        // Jump height calculation (in meters)
        float heightMeters = 0.5 * 9.81 * pow(flightTime / 2.0, 2); // Half the flight time for ascent
        float heightCm = heightMeters * 100; // Convert to centimeters

        // Save the jump height in the array (keep in cm)
        for (int i = 0; i < MAX_JUMPS - 1; i++) {
            jumpHeights[i] = jumpHeights[i + 1];
            airTimes[i] = airTimes[i + 1];
        }

        jumpHeights[MAX_JUMPS - 1] = heightCm; // Store the height in cm
        airTimes[MAX_JUMPS - 1] = flightTime;

        drawJumpGraph();
        delay(200);
    }
  }

  // Handle RESET button press
  if (p.x > 100 && p.x < 220 && p.y > 260 && p.y < 310) {
    resetJumps(); // Reset the jumps when button is pressed
    delay(500);   // Prevent multiple presses from being detected
  }
}
