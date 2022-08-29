#include <BleMouse.h>
#include <M5Unified.h>

BleMouse bleMouse;

namespace {

// rotary encoder unit
std::pair<int, bool> getEncoderValue() {
  int value = 0;
  bool btn = false;

  uint8_t buf[2];
  if (M5.Ex_I2C.readRegister(0x40, 0x10, buf, 2, 400000)) {
    value = (int16_t)(buf[1] << 8 | buf[0]);
  }

  if (M5.Ex_I2C.readRegister(0x40, 0x20, buf, 1, 400000)) {
    btn = buf[0] == 0;
  }
  return {value, btn};
}

struct Cursor {
  int stopCount = 0;
  float x = -1.0f;
  float y = -1.0f;
  float baseX = 0;
  float baseY = 0;
  float moveX = 0;
  float moveY = 0;
  float speed = 1.7f;
  float distX = 0.0f;
  float distY = 0.0f;
  float dist = 0.0f;
  int prevWheel = 0;
  int wheel = 0;
  bool initPos = true;
  bool wheelBtn = false;
  bool onTouch = false;
  bool onMove = false;
  bool touchClick = false;

  //
  void clear() {
    if (touchClick) {
      touchClick = false;
    } else if (onTouch) {
      if (onMove == false && stopCount < 10) {
        touchClick = true;
      }
    }
    stopCount = 0;
    initPos = true;
    moveX = 0.0f;
    moveY = 0.0f;
    x = -1.0f;
    y = -1.0f;
    onTouch = false;
    onMove = false;
  }

  //
  bool update(int X, int Y) {
    onTouch = true;

    float newX = (float)X * speed;
    float newY = (float)Y * speed;
    if (x == newX && y == newY) {
      moveX = 0.0f;
      moveY = 0.0f;
      if (++stopCount > 10) {
        initPos = true;
      }
      return false;
    }

    stopCount = 0;
    if (initPos) {
      baseX = newX;
      baseY = newY;
      x = newX;
      y = newY;
      initPos = false;
    } else {
      onMove = true;
    }
    moveX = newX - x;
    moveY = newY - y;
    distX = newX - baseX;
    distY = newY - baseY;
    float absDX = fabsf(distX);
    float absDY = fabsf(distY);
    if (absDX > 3.0f || absDY > 3.0f) {
      dist = sqrtf(distX * distX + distY + distY);
    } else {
      dist = 0.0f;
    }
    float offsetSpeed = std::min(1.0f, dist / 300.0f) + 1.0f;
    moveX *= offsetSpeed;
    moveY *= offsetSpeed;
    x = newX;
    y = newY;

    return true;
  }
};

Cursor cursor;

//
// cursor mode
//
void CursorMode() {
  auto [val, btn] = getEncoderValue();
  cursor.wheel = val - cursor.prevWheel;
  cursor.prevWheel = val;
  cursor.wheelBtn = btn;

  int tCount = M5.Touch.getCount();
  if (tCount == 0) {
    cursor.clear();
    return;
  }

  auto t = M5.Touch.getDetail(0);
  if (cursor.update(t.x, t.y)) {
  }
}

}  // namespace

//
//
//
void setup() {
  M5.begin();
  bleMouse.begin();
  M5.Ex_I2C.begin();

  M5.Display.setFont(&fonts::lgfxJapanGothic_24);

  auto [val, btn] = getEncoderValue();
  cursor.prevWheel = val;
}

//
//
//
void loop() {
  M5.update();

  CursorMode();

  bool bleOn = bleMouse.isConnected();
  if (bleOn) {
    if (cursor.touchClick) {
      bleMouse.click(MOUSE_LEFT);
    }
    if (M5.BtnC.wasClicked()) {
      bleMouse.click(MOUSE_RIGHT);
    }
    if (cursor.moveX != 0 || cursor.moveY != 0 || cursor.wheel) {
      bleMouse.move(cursor.moveX, cursor.moveY, cursor.wheel);
    }
  }

  auto &gfx = M5.Display;
  gfx.startWrite();
  gfx.setTextColor(TFT_WHITE);

  static bool oldBleOn = false;
  if (oldBleOn != bleOn) {
    gfx.fillRect(10, 2, 12 * 5, 24, TFT_BLACK);
    gfx.drawString(bleOn ? "BLE ON" : "---", 10, 2);
    oldBleOn = bleOn;
  }

  // battery
  static int battLv = -1;
  int newBattLv = M5.Power.getBatteryLevel();
  if (newBattLv != battLv) {
    battLv = newBattLv;
    int bL = M5.Display.width() - 120;
    gfx.drawRoundRect(bL, 2, 106, 16, 4, TFT_WHITE);
    auto col = TFT_GREEN;
    if (!M5.Power.isCharging()) {
      col = battLv < 20 ? TFT_RED : TFT_WHITE;
    }
    gfx.fillRect(bL + 3, 5, battLv, 10, col);
  }

  gfx.endWrite();
  gfx.display();
  delay(5);
}
