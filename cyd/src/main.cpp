#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "station_config.h"

#define BL 21
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define MAX_QSOS 300
#define PAGE_SIZE 5

TFT_eSPI tft;
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

struct QSO {
  String call, band, mode, time;
  String freq, grid, rst, date, rcvd;
};

QSO logs[MAX_QSOS];
int total = 0;
int page = 0;
int selected = -1;
int brightnessIndex = 0;
const int brightnessLevels[] = {255, 102, 26};
const char* brightnessNames[] = {"100%", "40%", "10%"};

String rx = "";
bool receiving = false;

const uint16_t BG = 0x1082;
const uint16_t PANEL = 0x2126;
const uint16_t GREEN = 0x07E0;
const uint16_t GREY = 0xB596;
const uint16_t WHITE = 0xFFFF;

void printAt(int x, int y, String s,
             uint16_t color = WHITE, int size = 1) {
  tft.setTextColor(color, BG);
  tft.setTextSize(size);
  tft.setCursor(x, y);
  tft.print(s);
}

void draw() {
  tft.fillScreen(BG);

  printAt(8, 7, STATION_CALLSIGN, GREEN, 2);
  printAt(100, 12, "X6100 QSO LOG");

  printAt(264, 5, "DIM", GREEN);
  printAt(265, 19, brightnessNames[brightnessIndex], GREY);

  tft.drawFastHLine(0, 34, 320, GREEN);


  if (selected >= 0 && selected < total) {
    QSO &q = logs[selected];

    // Callsign
    printAt(10, 40, q.call, GREEN, 2);
    tft.drawFastHLine(5, 62, 310, GREEN);

    // Row 1: UTC date and time
    printAt(10, 69, "UTC DATE", GREY);
    printAt(171, 69, "UTC TIME", GREY);
    printAt(10, 82, q.date, WHITE, 2);
    printAt(171, 82, q.time, WHITE, 2);

    // Row 2: Band and mode
    printAt(10, 106, "BAND", GREY);
    printAt(171, 106, "MODE", GREY);
    printAt(10, 119, q.band, WHITE, 2);
    printAt(171, 119, q.mode, WHITE, 2);

    // Row 3: Frequency and grid
    printAt(10, 143, "FREQUENCY MHz", GREY);
    printAt(171, 143, "GRID", GREY);
    printAt(10, 156, q.freq, WHITE, 2);
    printAt(171, 156, q.grid, WHITE, 2);

    // Row 4: Both signal reports
    printAt(10, 180, "RST SENT", GREY);
    printAt(171, 180, "RST RECEIVED", GREY);
    printAt(10, 193,
            q.rst.length() ? q.rst : "--", WHITE, 2);
    printAt(171, 193,
            q.rcvd.length() ? q.rcvd : "--", WHITE, 2);

    // Full-width BACK button
    tft.fillRoundRect(8, 212, 304, 26, 5, PANEL);
    printAt(135, 217, "BACK", GREEN, 2);
    return;
  }

  // Contact counter and navigation controls
  printAt(8, 45, "CONTACTS: " + String(total), GREEN, 2);

  tft.fillRoundRect(171, 38, 69, 29, 5, PANEL);
  tft.fillRoundRect(245, 38, 70, 29, 5, PANEL);

  printAt(182, 45, "PREV", GREEN, 2);
  printAt(256, 45, "NEXT", GREEN, 2);

  // Five contacts per page
  for (int i = 0; i < PAGE_SIZE; i++) {
    int idx = page * PAGE_SIZE + i;
    if (idx >= total) break;

    int y = 71 + i * 27;

    tft.fillRoundRect(5, y, 310, 25, 4, PANEL);
    tft.setTextColor(WHITE, PANEL);
    tft.setTextSize(1);

    tft.setCursor(11, y + 3);
    tft.print(logs[idx].call.substring(0, 13));

    tft.setCursor(160, y + 3);
    tft.print(logs[idx].band);

    tft.setCursor(235, y + 3);
    tft.print(logs[idx].mode);

    tft.setTextColor(GREY, PANEL);
    tft.setCursor(11, y + 14);
    tft.print(logs[idx].date + " " + logs[idx].time + " UTC");
  }

  tft.drawFastHLine(0, 211, 320, GREEN);

  int pages = max(1, (total + PAGE_SIZE - 1) / PAGE_SIZE);

  printAt(82, 220,
          "Page " + String(page + 1) +
          " of " + String(pages) +
          "  |  " + String(total) + " QSOs", GREY);
}

String field(String line, int index) {
  int start = 0;

  for (int i = 0; i < index; i++) {
    start = line.indexOf('|', start);
    if (start < 0) return "";
    start++;
  }

  int end = line.indexOf('|', start);
  if (end < 0) end = line.length();

  return line.substring(start, end);
}

void processLine(String line) {
  if (line == "BEGIN") {
    receiving = true;
    total = 0;
    page = 0;
    selected = -1;
    return;
  }

  if (line == "END") {
    receiving = false;
    draw();
    return;
  }

  if (line.startsWith("Q|") && total < MAX_QSOS) {
    QSO &q = logs[total++];

    q.call = field(line, 1);
    q.band = field(line, 2);
    q.mode = field(line, 3);
    q.time = field(line, 4);
    q.freq = field(line, 5);
    q.grid = field(line, 6);
    q.rst = field(line, 7);
    q.date = field(line, 8);
    q.rcvd = field(line, 9);

    if (!receiving) draw();
  }
}

void dimScreen() {
  brightnessIndex = (brightnessIndex + 1) % 3;
  ledcWrite(0, brightnessLevels[brightnessIndex]);
  draw();
}

void tapAt(int x, int y) {
  // DIM button
  if (y < 35 && x >= 250) {
    dimScreen();
    return;
  }

  // Contact details BACK button
  if (selected >= 0) {
    if (y >= 208) {
      selected = -1;
      draw();
    }
    return;
  }

  // Top navigation buttons
  if (y >= 36 && y <= 69) {
    if (x >= 171 && x <= 240 && page > 0) {
      page--;
      draw();
    } else if (x >= 245 &&
               (page + 1) * PAGE_SIZE < total) {
      page++;
      draw();
    }
    return;
  }

  // QSO rows
  if (y >= 71 && y < 206) {
    int row = (y - 71) / 27;
    int idx = page * PAGE_SIZE + row;

    if (idx < total) {
      selected = idx;
      draw();
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BL, OUTPUT);
  ledcSetup(0, 1000, 8);
  ledcAttachPin(BL, 0);
  ledcWrite(0, brightnessLevels[brightnessIndex]);

  tft.init();
  tft.setRotation(3);

  touchSPI.begin(25, 39, 32, 33);
  touch.begin(touchSPI);
  touch.setRotation(3);

  draw();
  Serial.println("CYD QSO Dashboard V2 Ready");
}

void loop() {
  // Receive contact records from X6100
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n') {
      processLine(rx);
      rx = "";
    } else if (c != '\r' && rx.length() < 200) {
      rx += c;
    }
  }

  static bool pressed = false;
  static int startX = 0, startY = 0;
  static int lastX = 0, lastY = 0;

  bool touching = touch.touched();

  if (touching) {
    TS_Point p = touch.getPoint();

    // Touchscreen calibration values for the tested CYD board
    int x = constrain(
      map(p.x, 410, 3855, 0, 319), 0, 319);
    int y = constrain(
      map(p.y, 465, 3680, 0, 239), 0, 239);

    if (!pressed) {
      startX = x;
      startY = y;
      pressed = true;
    }

    lastX = x;
    lastY = y;
  } else if (pressed) {
    int delta = lastY - startY;

    // Swipe changes pages
    if (selected < 0 && abs(delta) > 40) {
      if (delta < 0 &&
          (page + 1) * PAGE_SIZE < total) {
        page++;
      } else if (delta > 0 && page > 0) {
        page--;
      }
      draw();
    } else {
      tapAt(lastX, lastY);
    }

    pressed = false;
  }

  delay(20);
}
