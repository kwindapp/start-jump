#include <M5Core2.h>  // Use M5Core2 directly without M5Unified

MPU6886 imu;

// ====== TUNABLE PARAMETERS ======
float takeoffThreshold = 0.5;
float landingThresholdLow = 0.9;
float landingThresholdHigh = 1.1;
float gravity = 9.81;  // Standard gravity
float gravityFactor = 1.0; // Factor to scale gravity
float accelSmoothing = 0.2;
float minFlightTime = 0.1;
float distanceDamping = 0.2;
float maxJumpHeight = 0.1;  // Maximum height for jump graph
// ================================

bool inAir = false;
unsigned long takeoffTime = 0;
unsigned long landingTime = 0;

float axFiltered = 0, ayFiltered = 0, azFiltered = 0;
float vx = 0, vy = 0;
float distanceX = 0, distanceY = 0;

#define MAX_JUMPS 5
float jumpHeights[MAX_JUMPS] = {0};

// ====== MENU ======
int menuIndex = 0;
bool inMenu = true;
bool exitConfirm = false;

void drawButtons() {
  M5.Lcd.fillRect(0, 200, 320, 40, DARKGREY);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(BLACK);

  M5.Lcd.fillRect(5, 205, 70, 30, WHITE);
  M5.Lcd.setCursor(20, 210);
  M5.Lcd.print("UP");

  M5.Lcd.fillRect(85, 205, 70, 30, WHITE);
  M5.Lcd.setCursor(95, 210);
  M5.Lcd.print("DOWN");

  M5.Lcd.fillRect(165, 205, 70, 30, WHITE);
  M5.Lcd.setCursor(190, 210);
  M5.Lcd.print("+");

  M5.Lcd.fillRect(245, 205, 70, 30, WHITE);
  M5.Lcd.setCursor(270, 210);
  M5.Lcd.print("-");
}

void drawMenu() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(0, 0);

  if (menuIndex == 0) M5.Lcd.print("> "); else M5.Lcd.print("  ");
  M5.Lcd.printf("TakeoffThresh: %.2f\n", takeoffThreshold);

  if (menuIndex == 1) M5.Lcd.print("> "); else M5.Lcd.print("  ");
  M5.Lcd.printf("Land Thresh Low: %.2f\n", landingThresholdLow);

  if (menuIndex == 2) M5.Lcd.print("> "); else M5.Lcd.print("  ");
  M5.Lcd.printf("Land Thresh High: %.2f\n", landingThresholdHigh);

  if (menuIndex == 3) M5.Lcd.print("> "); else M5.Lcd.print("  ");
  M5.Lcd.printf("Gravity Factor: %.2f\n", gravityFactor);

  if (menuIndex == 4) M5.Lcd.print("> "); else M5.Lcd.print("  ");
  M5.Lcd.printf("Accel Smoothing: %.2f\n", accelSmoothing);

  if (menuIndex == 5) M5.Lcd.print("> "); else M5.Lcd.print("  ");
  M5.Lcd.printf("Max Jump Height: %.2f\n", maxJumpHeight);

  if (menuIndex == 6) M5.Lcd.print("> "); else M5.Lcd.print("  ");
  M5.Lcd.printf("Min Flight Time: %.2f\n", minFlightTime);

  if (menuIndex == 7) {
    M5.Lcd.print("> ");
    if (exitConfirm) {
      M5.Lcd.println("Exit (Confirm Again)");
    } else {
      M5.Lcd.println("Exit");
    }
  } else {
    M5.Lcd.print("  Exit\n");
  }

  drawButtons();
}

void drawJumpGraph() {
  int startX = 20;
  int baseY = 200;
  int barWidth = 40;
  int maxHeight = 120;
  
  // Dynamically calculate maxJumpHeight based on the heights in jumpHeights array
  float maxJumpHeight = 0.1;  // Start with a minimum value to avoid divide by zero
  for (int i = 0; i < MAX_JUMPS; i++) {
    if (jumpHeights[i] > maxJumpHeight) {
      maxJumpHeight = jumpHeights[i];
    }
  }

  // Clear graph area (title + bars)
  M5.Lcd.fillRect(0, baseY - maxHeight - 40, 320, maxHeight + 70, BLACK);

  // Title
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(YELLOW, BLACK);
  M5.Lcd.setCursor(10, baseY - maxHeight - 30 + 10);  // Move title 20px down
  M5.Lcd.print("Last 5 Jump Heights (cm)");

  // Draw bars
  for (int i = 0; i < MAX_JUMPS; i++) {
    int x = startX + i * (barWidth + 20);
    int barHeight = (int)(min(jumpHeights[i] / maxJumpHeight, 1.0f) * maxHeight);
    int y = baseY - barHeight;

    M5.Lcd.fillRect(x, y, barWidth, barHeight, GREEN);

    // Center the value under the bar (convert from meters to cm)
    char buf[6];
    sprintf(buf, "%.0f", jumpHeights[i] * 100);  // Convert height to cm
    int textWidth = M5.Lcd.textWidth(buf);
    M5.Lcd.setCursor(x + (barWidth - textWidth) / 2, baseY + 5);
    M5.Lcd.printf("%s", buf);
  }
}

void setup() {
  M5.begin(); // Using M5.begin() despite the deprecation warning
  M5.IMU.Init();

  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.println("Ready to Jump!");
  drawMenu();
}

void loop() {
  M5.update();

  // ===== MENU HANDLING =====
  if (inMenu) {
    TouchPoint_t p = M5.Touch.getPressPoint();

    if (p.y > 200 && p.y < 240) {
      if (p.x > 5 && p.x < 75) { // UP
        menuIndex = (menuIndex - 1 + 8) % 8;
        exitConfirm = false;
        drawMenu();
        delay(200);
      } else if (p.x > 85 && p.x < 155) { // DOWN
        menuIndex = (menuIndex + 1) % 8;
        exitConfirm = false;
        drawMenu();
        delay(200);
      } else if (p.x > 165 && p.x < 235) { // +
        if (menuIndex == 0) takeoffThreshold = min(takeoffThreshold + 0.1, 2.0);
        else if (menuIndex == 1) landingThresholdLow = min(landingThresholdLow + 0.1, 1.5);
        else if (menuIndex == 2) landingThresholdHigh = min(landingThresholdHigh + 0.1, 1.5);
        else if (menuIndex == 3) gravityFactor = min(gravityFactor + 0.1, 1.5);
        else if (menuIndex == 4) accelSmoothing = min(accelSmoothing + 0.05, 1.0);
        else if (menuIndex == 5) maxJumpHeight = min(maxJumpHeight + 0.05, 2.0);
        else if (menuIndex == 6) minFlightTime = min(minFlightTime + 0.1, 1.0);
        else if (menuIndex == 7) {
          if (exitConfirm) {
            inMenu = false;
            M5.Lcd.fillScreen(BLACK);
            M5.Lcd.setCursor(10, 50);
            M5.Lcd.setTextColor(GREEN);
            M5.Lcd.setTextSize(2);
            M5.Lcd.println("Jump Mode Active!");
            delay(1000);
          } else {
            exitConfirm = true;
          }
        }
        drawMenu();
        delay(200);
      } else if (p.x > 245 && p.x < 315) { // -
        if (menuIndex == 0) takeoffThreshold = max(takeoffThreshold - 0.1, 0.1);
        else if (menuIndex == 1) landingThresholdLow = max(landingThresholdLow - 0.1, 0.1);
        else if (menuIndex == 2) landingThresholdHigh = max(landingThresholdHigh - 0.1, 0.1);
        else if (menuIndex == 3) gravityFactor = max(gravityFactor - 0.1, 0.1);
        else if (menuIndex == 4) accelSmoothing = max(accelSmoothing - 0.05, 0.0);
        else if (menuIndex == 5) maxJumpHeight = max(maxJumpHeight - 0.05, 0.1);
        else if (menuIndex == 6) minFlightTime = max(minFlightTime - 0.1, 0.0);
        exitConfirm = false;
        drawMenu();
        delay(200);
      }
    }
    return;
  }

  // ===== JUMP LOGIC =====
  float rawAx, rawAy, rawAz;
  M5.IMU.getAccelData(&rawAx, &rawAy, &rawAz);

  axFiltered = accelSmoothing * rawAx + (1 - accelSmoothing) * axFiltered;
  ayFiltered = accelSmoothing * rawAy + (1 - accelSmoothing) * ayFiltered;
  azFiltered = accelSmoothing * rawAz + (1 - accelSmoothing) * azFiltered;

  float totalAccel = sqrt(axFiltered * axFiltered +
                          ayFiltered * ayFiltered +
                          azFiltered * azFiltered);

  static unsigned long lastTime = millis();

  // Takeoff
  if (!inAir && totalAccel < takeoffThreshold) {
    takeoffTime = millis();
    inAir = true;
    vx = vy = 0;
    distanceX = distanceY = 0;

    M5.Lcd.fillRect(0, 0, 320, 20, BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println("Takeoff!");
  }

  if (inAir) {
    unsigned long now = millis();
    float dt = (now - lastTime) / 1000.0;
    lastTime = now;

    float az_corrected = azFiltered - gravity * gravityFactor;  // Adjust gravity
    vx += axFiltered * az_corrected * dt;
    vy += ayFiltered * az_corrected * dt;
    distanceX += vx * dt * distanceDamping;
    distanceY += vy * dt * distanceDamping;
  } else {
    lastTime = millis();
  }

  // Landing
  if (inAir && totalAccel > landingThresholdLow && totalAccel < landingThresholdHigh) {
    landingTime = millis();
    float flightTime = (landingTime - takeoffTime) / 1000.0;

    if (flightTime > minFlightTime) {
      inAir = false;

      float height = 0.5 * gravity * pow(flightTime / 2.0, 2);
      float totalDistance = sqrt(distanceX * distanceX + distanceY * distanceY);
      float impactG = totalAccel;

      for (int i = 0; i < MAX_JUMPS - 1; i++) {
        jumpHeights[i] = jumpHeights[i + 1];
      }
      jumpHeights[MAX_JUMPS - 1] = height;

      M5.Lcd.fillRect(0, 0, 320, 200, BLACK);
      M5.Lcd.setTextSize(2);
      M5.Lcd.setTextColor(WHITE, BLACK);
      M5.Lcd.setCursor(10, 0);
      M5.Lcd.printf("Flight:%.2fs", flightTime);

      M5.Lcd.setCursor(160, 0);
      M5.Lcd.printf("Height:%.0fcm", height * 100);

      M5.Lcd.setCursor(10, 20);
      M5.Lcd.printf("Dist:%.2fm", totalDistance);

      M5.Lcd.setCursor(160, 20);
      M5.Lcd.printf("Impact:%.2fg", impactG);

      drawJumpGraph();
    }
  }

  delay(10);
} 