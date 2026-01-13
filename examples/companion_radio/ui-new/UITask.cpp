#include "UITask.h"
#include <helpers/TxtDataHelpers.h>
#include "../MyMesh.h"
#include "target.h"
#ifdef M5STACK_CARDPUTER_ADV
#include <M5Cardputer.h>
#endif

#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS     15000   // 15 seconds
#endif
#define BOOT_SCREEN_MILLIS   3000   // 3 seconds

#ifdef PIN_STATUS_LED
#define LED_ON_MILLIS     20
#define LED_ON_MSG_MILLIS 200
#define LED_CYCLE_MILLIS  4000
#endif

#define LONG_PRESS_MILLIS   1200

#ifndef UI_RECENT_LIST_SIZE
  #define UI_RECENT_LIST_SIZE 4
#endif

#if UI_HAS_JOYSTICK
  #define PRESS_LABEL "press Enter"
#else
  #define PRESS_LABEL "long press"
#endif

#include "icons.h"

#ifdef M5STACK_CARDPUTER_ADV
static char mapCardputerKey(const Keyboard_Class::KeysState& status) {
  for (uint8_t hid_key : status.hid_keys) {
    switch (hid_key) {
      case 0x50:  // HID left arrow
        return KEY_LEFT;
      case 0x4F:  // HID right arrow
        return KEY_RIGHT;
      case 0x52:  // HID up arrow
        return KEY_UP;
      case 0x51:  // HID down arrow
        return KEY_DOWN;
      case 0x29:  // HID Escape
        return KEY_PREV;
      default:
        break;
    }
  }

  if (status.enter) return KEY_ENTER;
  if (status.tab) return KEY_NEXT;
  if (status.del) return KEY_PREV;

  for (char ch : status.word) {
    switch (ch) {
      // WASD removed - these are needed for typing messages
      // case 'a':
      // case 'A':
      //   return KEY_LEFT;
      // case 'd':
      // case 'D':
      //   return KEY_RIGHT;
      // case 'w':
      // case 'W':
      //   return KEY_UP;
      // case 's':
      // case 'S':
      //   return KEY_DOWN;
      case ',':
        return KEY_LEFT;
      case '<':
        return KEY_LEFT;
      case '.':
        return KEY_DOWN;
      case '>':
        return KEY_DOWN;
      case '/':
        return KEY_RIGHT;
      case '?':
        return KEY_RIGHT;
      case ';':
        return KEY_UP;
      case ':':
        return KEY_UP;
      case '`':
        return KEY_PREV;
      case 'q':
      case 'Q':
        return KEY_PREV;
      case 'i':
      case 'I':
        return KEY_UP;
      case 'j':
      case 'J':
        return KEY_LEFT;
      case 'k':
      case 'K':
        return KEY_DOWN;
      case 'l':
      case 'L':
        return KEY_RIGHT;
      case 'e':
      case 'E':
        return KEY_NEXT;
      case ' ':
        return KEY_ENTER;
      default:
        break;
    }
  }

  return 0;
}
#endif

class SplashScreen : public UIScreen {
  UITask* _task;
  unsigned long dismiss_after;
  char _version_info[12];

public:
  SplashScreen(UITask* task) : _task(task) {
    // strip off dash and commit hash by changing dash to null terminator
    // e.g: v1.2.3-abcdef -> v1.2.3
    const char *ver = FIRMWARE_VERSION;
    const char *dash = strchr(ver, '-');

    int len = dash ? dash - ver : strlen(ver);
    if (len >= sizeof(_version_info)) len = sizeof(_version_info) - 1;
    memcpy(_version_info, ver, len);
    _version_info[len] = 0;

    dismiss_after = millis() + BOOT_SCREEN_MILLIS;
  }

  int render(DisplayDriver& display) override {
    // meshcore logo
    display.setColor(DisplayDriver::BLUE);
    int logoWidth = 128;
    display.drawXbm((display.width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // version info
    display.setColor(DisplayDriver::LIGHT);
    display.setTextSize(2);
    display.drawTextCentered(display.width()/2, 22, _version_info);

    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, 42, FIRMWARE_BUILD_DATE);

    return 1000;
  }

  void poll() override {
    if (millis() >= dismiss_after) {
      _task->gotoHomeScreen();
    }
  }
};

class HomeScreen : public UIScreen {
  enum HomePage {
    FIRST,
    RECENT,
#ifdef M5STACK_CARDPUTER_ADV
    MESSAGES,
    CHANNELS,
#endif
    RADIO,
    BLUETOOTH,
    ADVERT,
#if ENV_INCLUDE_GPS == 1
    GPS,
#endif
#if UI_SENSORS_PAGE == 1
    SENSORS,
#endif
    SHUTDOWN,
    Count    // keep as last
  };

  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  uint8_t _page;
  bool _shutdown_init;
  AdvertPath recent[UI_RECENT_LIST_SIZE];


  void renderBatteryIndicator(DisplayDriver& display, uint16_t batteryMilliVolts) {
    // Convert millivolts to percentage
    const int minMilliVolts = 3000; // Minimum voltage (e.g., 3.0V)
    const int maxMilliVolts = 4200; // Maximum voltage (e.g., 4.2V)
    int batteryPercentage = ((batteryMilliVolts - minMilliVolts) * 100) / (maxMilliVolts - minMilliVolts);
    if (batteryPercentage < 0) batteryPercentage = 0; // Clamp to 0%
    if (batteryPercentage > 100) batteryPercentage = 100; // Clamp to 100%

    // battery icon
    int iconWidth = 24;
    int iconHeight = 10;
    int iconX = display.width() - iconWidth - 5; // Position the icon near the top-right corner
    int iconY = 0;
    display.setColor(DisplayDriver::GREEN);

    // battery outline
    display.drawRect(iconX, iconY, iconWidth, iconHeight);

    // battery "cap"
    display.fillRect(iconX + iconWidth, iconY + (iconHeight / 4), 3, iconHeight / 2);

    // fill the battery based on the percentage
    int fillWidth = (batteryPercentage * (iconWidth - 4)) / 100;
    display.fillRect(iconX + 2, iconY + 2, fillWidth, iconHeight - 4);
  }

  CayenneLPP sensors_lpp;
  int sensors_nb = 0;
  bool sensors_scroll = false;
  int sensors_scroll_offset = 0;
  int next_sensors_refresh = 0;

  void refresh_sensors() {
    if (millis() > next_sensors_refresh) {
      sensors_lpp.reset();
      sensors_nb = 0;
      sensors_lpp.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      sensors.querySensors(0xFF, sensors_lpp);
      LPPReader reader (sensors_lpp.getBuffer(), sensors_lpp.getSize());
      uint8_t channel, type;
      while(reader.readHeader(channel, type)) {
        reader.skipData(type);
        sensors_nb ++;
      }
      sensors_scroll = sensors_nb > UI_RECENT_LIST_SIZE;
#if AUTO_OFF_MILLIS > 0
      next_sensors_refresh = millis() + 5000; // refresh sensor values every 5 sec
#else
      next_sensors_refresh = millis() + 60000; // refresh sensor values every 1 min
#endif
    }
  }

public:
  HomeScreen(UITask* task, mesh::RTCClock* rtc, SensorManager* sensors, NodePrefs* node_prefs)
     : _task(task), _rtc(rtc), _sensors(sensors), _node_prefs(node_prefs), _page(0), 
       _shutdown_init(false), sensors_lpp(200) {  }

  void poll() override {
    if (_shutdown_init && !_task->isButtonPressed()) {  // must wait for USR button to be released
      _task->shutdown();
    }
  }

  int render(DisplayDriver& display) override {
    char tmp[80];
    // node name
    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    char filtered_name[sizeof(_node_prefs->node_name)];
    display.translateUTF8ToBlocks(filtered_name, _node_prefs->node_name, sizeof(filtered_name));
    display.setCursor(0, 0);
    display.print(filtered_name);

    // battery voltage
    renderBatteryIndicator(display, _task->getBattMilliVolts());

    // curr page indicator
    int y = 14;
    int x = display.width() / 2 - 5 * (HomePage::Count-1);
    for (uint8_t i = 0; i < HomePage::Count; i++, x += 10) {
      if (i == _page) {
        display.fillRect(x-1, y-1, 3, 3);
      } else {
        display.fillRect(x, y, 1, 1);
      }
    }

    if (_page == HomePage::FIRST) {
      display.setColor(DisplayDriver::YELLOW);
      display.setTextSize(2);
      sprintf(tmp, "MSG: %d", _task->getMsgCount());
      display.drawTextCentered(display.width() / 2, 20, tmp);

      if (_task->hasConnection()) {
        display.setColor(DisplayDriver::GREEN);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, 43, "< Connected >");
      } else if (the_mesh.getBLEPin() != 0) { // BT pin
        display.setColor(DisplayDriver::RED);
        display.setTextSize(2);
        sprintf(tmp, "Pin:%d", the_mesh.getBLEPin());
        display.drawTextCentered(display.width() / 2, 43, tmp);
      }
    } else if (_page == HomePage::RECENT) {
      the_mesh.getRecentlyHeard(recent, UI_RECENT_LIST_SIZE);
      display.setColor(DisplayDriver::GREEN);
      int y = 20;
      for (int i = 0; i < UI_RECENT_LIST_SIZE; i++, y += 11) {
        auto a = &recent[i];
        if (a->name[0] == 0) continue;  // empty slot
        int secs = _rtc->getCurrentTime() - a->recv_timestamp;
        if (secs < 60) {
          sprintf(tmp, "%ds", secs);
        } else if (secs < 60*60) {
          sprintf(tmp, "%dm", secs / 60);
        } else {
          sprintf(tmp, "%dh", secs / (60*60));
        }

        int timestamp_width = display.getTextWidth(tmp);
        int max_name_width = display.width() - timestamp_width - 1;

        char filtered_recent_name[sizeof(a->name)];
        display.translateUTF8ToBlocks(filtered_recent_name, a->name, sizeof(filtered_recent_name));
        display.drawTextEllipsized(0, y, max_name_width, filtered_recent_name);
        display.setCursor(display.width() - timestamp_width - 1, y);
        display.print(tmp);
      }
#ifdef M5STACK_CARDPUTER_ADV
    } else if (_page == HomePage::MESSAGES) {
      display.setColor(DisplayDriver::YELLOW);
      display.setTextSize(2);
      display.drawTextCentered(display.width() / 2, 22, "Send Message");
      display.setTextSize(1);
      display.setColor(DisplayDriver::GREEN);
      display.drawTextCentered(display.width() / 2, 45, "Press " PRESS_LABEL " to compose");
    } else if (_page == HomePage::CHANNELS) {
      display.setColor(DisplayDriver::YELLOW);
      display.setTextSize(2);
      display.drawTextCentered(display.width() / 2, 22, "Channels");
      display.setTextSize(1);
      display.setColor(DisplayDriver::GREEN);
      int num_channels = 0;
      for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
        ChannelDetails ch;
        if (the_mesh.getChannel(i, ch) && ch.name[0] != '\0') {
          num_channels++;
        }
      }
      sprintf(tmp, "%d channel(s)", num_channels);
      display.drawTextCentered(display.width() / 2, 38, tmp);
      display.drawTextCentered(display.width() / 2, 50, "Press " PRESS_LABEL " to view");
#endif
    } else if (_page == HomePage::RADIO) {
      display.setColor(DisplayDriver::YELLOW);
      display.setTextSize(1);
      // freq / sf
      display.setCursor(0, 20);
      sprintf(tmp, "FQ: %06.3f   SF: %d", _node_prefs->freq, _node_prefs->sf);
      display.print(tmp);

      display.setCursor(0, 31);
      sprintf(tmp, "BW: %03.2f     CR: %d", _node_prefs->bw, _node_prefs->cr);
      display.print(tmp);

      // tx power,  noise floor
      display.setCursor(0, 42);
      sprintf(tmp, "TX: %ddBm", _node_prefs->tx_power_dbm);
      display.print(tmp);
      display.setCursor(0, 53);
      sprintf(tmp, "Noise floor: %d", radio_driver.getNoiseFloor());
      display.print(tmp);
    } else if (_page == HomePage::BLUETOOTH) {
      display.setColor(DisplayDriver::GREEN);
      display.drawXbm((display.width() - 32) / 2, 18,
          _task->isSerialEnabled() ? bluetooth_on : bluetooth_off,
          32, 32);
      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, 64 - 11, "toggle: " PRESS_LABEL);
    } else if (_page == HomePage::ADVERT) {
      display.setColor(DisplayDriver::GREEN);
      display.drawXbm((display.width() - 32) / 2, 18, advert_icon, 32, 32);
      display.drawTextCentered(display.width() / 2, 64 - 11, "advert: " PRESS_LABEL);
#if ENV_INCLUDE_GPS == 1
    } else if (_page == HomePage::GPS) {
      LocationProvider* nmea = sensors.getLocationProvider();
      char buf[50];
      int y = 18;
      bool gps_state = _task->getGPSState();
#ifdef PIN_GPS_SWITCH
      bool hw_gps_state = digitalRead(PIN_GPS_SWITCH);
      if (gps_state != hw_gps_state) {
        strcpy(buf, gps_state ? "gps off(hw)" : "gps off(sw)");
      } else {
        strcpy(buf, gps_state ? "gps on" : "gps off");
      }
#else
      strcpy(buf, gps_state ? "gps on" : "gps off");
#endif
      display.drawTextLeftAlign(0, y, buf);
      if (nmea == NULL) {
        y = y + 12;
        display.drawTextLeftAlign(0, y, "Can't access GPS");
      } else {
        strcpy(buf, nmea->isValid()?"fix":"no fix");
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "sat");
        sprintf(buf, "%d", nmea->satellitesCount());
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "pos");
        sprintf(buf, "%.4f %.4f", 
          nmea->getLatitude()/1000000., nmea->getLongitude()/1000000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "alt");
        sprintf(buf, "%.2f", nmea->getAltitude()/1000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
      }
#endif
#if UI_SENSORS_PAGE == 1
    } else if (_page == HomePage::SENSORS) {
      int y = 18;
      refresh_sensors();
      char buf[30];
      char name[30];
      LPPReader r(sensors_lpp.getBuffer(), sensors_lpp.getSize());

      for (int i = 0; i < sensors_scroll_offset; i++) {
        uint8_t channel, type;
        r.readHeader(channel, type);
        r.skipData(type);
      }

      for (int i = 0; i < (sensors_scroll?UI_RECENT_LIST_SIZE:sensors_nb); i++) {
        uint8_t channel, type;
        if (!r.readHeader(channel, type)) { // reached end, reset
          r.reset();
          r.readHeader(channel, type);
        }

        display.setCursor(0, y);
        float v;
        switch (type) {
          case LPP_GPS: // GPS
            float lat, lon, alt;
            r.readGPS(lat, lon, alt);
            strcpy(name, "gps"); sprintf(buf, "%.4f %.4f", lat, lon);
            break;
          case LPP_VOLTAGE:
            r.readVoltage(v);
            strcpy(name, "voltage"); sprintf(buf, "%6.2f", v);
            break;
          case LPP_CURRENT:
            r.readCurrent(v);
            strcpy(name, "current"); sprintf(buf, "%.3f", v);
            break;
          case LPP_TEMPERATURE:
            r.readTemperature(v);
            strcpy(name, "temperature"); sprintf(buf, "%.2f", v);
            break;
          case LPP_RELATIVE_HUMIDITY:
            r.readRelativeHumidity(v);
            strcpy(name, "humidity"); sprintf(buf, "%.2f", v);
            break;
          case LPP_BAROMETRIC_PRESSURE:
            r.readPressure(v);
            strcpy(name, "pressure"); sprintf(buf, "%.2f", v);
            break;
          case LPP_ALTITUDE:
            r.readAltitude(v);
            strcpy(name, "altitude"); sprintf(buf, "%.0f", v);
            break;
          case LPP_POWER:
            r.readPower(v);
            strcpy(name, "power"); sprintf(buf, "%6.2f", v);
            break;
          default:
            r.skipData(type);
            strcpy(name, "unk"); sprintf(buf, "");
        }
        display.setCursor(0, y);
        display.print(name);
        display.setCursor(
          display.width()-display.getTextWidth(buf)-1, y
        );
        display.print(buf);
        y = y + 12;
      }
      if (sensors_scroll) sensors_scroll_offset = (sensors_scroll_offset+1)%sensors_nb;
      else sensors_scroll_offset = 0;
#endif
    } else if (_page == HomePage::SHUTDOWN) {
      display.setColor(DisplayDriver::GREEN);
      display.setTextSize(1);
      if (_shutdown_init) {
        display.drawTextCentered(display.width() / 2, 34, "hibernating...");
      } else {
        display.drawXbm((display.width() - 32) / 2, 18, power_icon, 32, 32);
        display.drawTextCentered(display.width() / 2, 64 - 11, "hibernate:" PRESS_LABEL);
      }
    }
    return 5000;   // next render after 5000 ms
  }

  bool handleInput(char c) override {
    if (c == KEY_LEFT || c == KEY_PREV) {
      _page = (_page + HomePage::Count - 1) % HomePage::Count;
      return true;
    }
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      _page = (_page + 1) % HomePage::Count;
      if (_page == HomePage::RECENT) {
        _task->showAlert("Recent adverts", 800);
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::BLUETOOTH) {
      if (_task->isSerialEnabled()) {  // toggle Bluetooth on/off
        _task->disableSerial();
      } else {
        _task->enableSerial();
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::ADVERT) {
      _task->notify(UIEventType::ack);
      if (the_mesh.advert()) {
        _task->showAlert("Advert sent!", 1000);
      } else {
        _task->showAlert("Advert failed..", 1000);
      }
      return true;
    }
#if ENV_INCLUDE_GPS == 1
    if (c == KEY_ENTER && _page == HomePage::GPS) {
      _task->toggleGPS();
      return true;
    }
#endif
#if UI_SENSORS_PAGE == 1
    if (c == KEY_ENTER && _page == HomePage::SENSORS) {
      _task->toggleGPS();
      next_sensors_refresh=0;
      return true;
    }
#endif
    if (c == KEY_ENTER && _page == HomePage::SHUTDOWN) {
      _shutdown_init = true;  // need to wait for button to be released
      return true;
    }
#ifdef M5STACK_CARDPUTER_ADV
    if (c == KEY_ENTER && _page == HomePage::MESSAGES) {
      _task->startComposingMessage();
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::CHANNELS) {
      _task->startViewingChannels();
      return true;
    }
#endif

    return false;
  }
};

class MsgPreviewScreen : public UIScreen {
  UITask* _task;
  mesh::RTCClock* _rtc;

  struct MsgEntry {
    uint32_t timestamp;
    char origin[62];
    char msg[78];
  };
  #define MAX_UNREAD_MSGS   32
  int num_unread;
  MsgEntry unread[MAX_UNREAD_MSGS];

public:
  MsgPreviewScreen(UITask* task, mesh::RTCClock* rtc) : _task(task), _rtc(rtc) { num_unread = 0; }

  void addPreview(uint8_t path_len, const char* from_name, const char* msg) {
    if (num_unread >= MAX_UNREAD_MSGS) return;  // full

    auto p = &unread[num_unread++];
    p->timestamp = _rtc->getCurrentTime();
    if (path_len == 0xFF) {
      sprintf(p->origin, "(D) %s:", from_name);
    } else {
      sprintf(p->origin, "(%d) %s:", (uint32_t) path_len, from_name);
    }
    StrHelper::strncpy(p->msg, msg, sizeof(p->msg));
  }

  int render(DisplayDriver& display) override {
    char tmp[16];
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    sprintf(tmp, "Unread: %d", num_unread);
    display.print(tmp);

    auto p = &unread[0];

    int secs = _rtc->getCurrentTime() - p->timestamp;
    if (secs < 60) {
      sprintf(tmp, "%ds", secs);
    } else if (secs < 60*60) {
      sprintf(tmp, "%dm", secs / 60);
    } else {
      sprintf(tmp, "%dh", secs / (60*60));
    }
    display.setCursor(display.width() - display.getTextWidth(tmp) - 2, 0);
    display.print(tmp);

    display.drawRect(0, 11, display.width(), 1);  // horiz line

    display.setCursor(0, 14);
    display.setColor(DisplayDriver::YELLOW);
    char filtered_origin[sizeof(p->origin)];
    display.translateUTF8ToBlocks(filtered_origin, p->origin, sizeof(filtered_origin));
    display.print(filtered_origin);

    display.setCursor(0, 25);
    display.setColor(DisplayDriver::LIGHT);
    char filtered_msg[sizeof(p->msg)];
    display.translateUTF8ToBlocks(filtered_msg, p->msg, sizeof(filtered_msg));
    display.printWordWrap(filtered_msg, display.width());

#if AUTO_OFF_MILLIS==0 // probably e-ink
    return 10000; // 10 s
#else
    return 1000;  // next render after 1000 ms
#endif
  }

  bool handleInput(char c) override {
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      num_unread--;
      if (num_unread == 0) {
        _task->gotoHomeScreen();
      } else {
        // delete first/curr item from unread queue
        for (int i = 0; i < num_unread; i++) {
          unread[i] = unread[i + 1];
        }
      }
      return true;
    }
    if (c == KEY_ENTER) {
      num_unread = 0;  // clear unread queue
      _task->gotoHomeScreen();
      return true;
    }
    return false;
  }
};

// ====================  TEXT INPUT SCREENS FOR CARDPUTER ====================

#ifdef M5STACK_CARDPUTER_ADV

class TextInputScreen : public UIScreen {
  UITask* _task;
  char input_buffer[256];
  int cursor_pos;
  int scroll_offset;
  bool shift_active;
  ContactInfo* recipient;
  bool channel_mode;
  uint8_t channel_idx;
  char channel_name[32];
  UIScreen* return_screen;

public:
  TextInputScreen(UITask* task) : _task(task) {
    input_buffer[0] = 0;
    cursor_pos = 0;
    scroll_offset = 0;
    shift_active = false;
    recipient = NULL;
    channel_mode = false;
    channel_idx = 0;
    channel_name[0] = '\0';
    return_screen = NULL;
  }

  void setRecipient(ContactInfo* r) {
    recipient = r;
    channel_mode = false;
    input_buffer[0] = 0;
    cursor_pos = 0;
    scroll_offset = 0;
  }

  void setChannel(uint8_t idx) {
    channel_mode = true;
    channel_idx = idx;
    recipient = NULL;
    ChannelDetails ch;
    if (the_mesh.getChannel(idx, ch)) {
      strncpy(channel_name, ch.name, sizeof(channel_name) - 1);
      channel_name[sizeof(channel_name) - 1] = '\0';
    } else {
      snprintf(channel_name, sizeof(channel_name), "Channel %d", idx);
    }
    input_buffer[0] = 0;
    cursor_pos = 0;
    scroll_offset = 0;
  }

  void setReturnScreen(UIScreen* screen) { return_screen = screen; }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    display.setCursor(0, 0);

    char header[40];
    if (channel_mode) {
      snprintf(header, sizeof(header), "Ch: %.25s", channel_name);
    } else if (recipient) {
      snprintf(header, sizeof(header), "To: %.25s", recipient->name);
    } else {
      snprintf(header, sizeof(header), "Compose Message");
    }
    display.print(header);

    // Draw horizontal line
    display.setColor(DisplayDriver::LIGHT);
    display.drawRect(0, 11, display.width(), 1);

    // Display input text with word wrap
    display.setCursor(0, 14);
    display.setColor(DisplayDriver::YELLOW);
    int text_len = strlen(input_buffer);
    if (text_len > 0) {
      display.printWordWrap(input_buffer, display.width());
    } else {
      display.setColor(DisplayDriver::DARK);
      display.print("Type message...");
    }

    // Show cursor indicator and help text at bottom
    display.setColor(DisplayDriver::BLUE);
    display.setCursor(0, display.height() - 10);
    char status[40];
    if (shift_active) {
      snprintf(status, sizeof(status), "SHIFT | %d chars", text_len);
    } else {
      snprintf(status, sizeof(status), "Enter=Send Del=Back | %d", text_len);
    }
    display.print(status);

    return 200;
  }

  void poll() override {
    // Poll keyboard in poll() instead of handleInput() to avoid main loop interference
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      _task->wakeDisplay();
      auto& status = M5Cardputer.Keyboard.keysState();

      // Handle special keys
      if (status.del || status.word[0] == '\b') {
        if (cursor_pos > 0) {
          // Delete character
          cursor_pos--;
          input_buffer[cursor_pos] = 0;
        }
        return;
      }

      // Check for Shift/Fn key
      if (status.fn) {
        shift_active = !shift_active;
        return;
      }

      // Handle Enter key - send message
      if (status.enter) {
        if (strlen(input_buffer) > 0) {
          uint32_t timestamp = rtc_clock.getCurrentTime();
          if (channel_mode) {
            ChannelDetails ch;
            if (the_mesh.getChannel(channel_idx, ch) &&
                the_mesh.sendGroupMessage(timestamp, ch.channel, the_mesh.getNodeName(), input_buffer, strlen(input_buffer))) {
              _task->newChannelMessage(channel_idx, 0xFF, 0, timestamp, input_buffer);
              _task->showAlert("Channel sent!", 2000);
            } else {
              _task->showAlert("Send failed", 2000);
            }
          } else if (recipient != NULL) {
            uint32_t expected_ack = 0;
            uint32_t est_timeout = 0;
            int result = the_mesh.sendMessage(*recipient, timestamp, 0, input_buffer, expected_ack, est_timeout);
            if (result >= 0) {
              _task->showAlert("Message sent!", 2000);
            } else {
              _task->showAlert("Send failed", 2000);
            }
          } else {
            _task->showAlert("No recipient!", 2000);
          }
          if (return_screen) {
            _task->setCurrScreen(return_screen);
          } else {
            _task->gotoHomeScreen();
          }
          return;
        }
        return;
      }

      // Handle ESC key (HID code 0x29) - cancel
      for (uint8_t hid_key : status.hid_keys) {
        if (hid_key == 0x29) {  // HID Escape key
          if (return_screen) {
            _task->setCurrScreen(return_screen);
          } else {
            _task->gotoHomeScreen();
          }
          return;
        }
      }

      // Handle regular character input
      // In text input mode, we want to capture all printable characters
      for (char ch : status.word) {
        if (ch == 0) break;

        // Skip special characters that might cause issues
        if (ch == '\r' || ch == '\n' || ch == '\t') {
          continue;
        }
        if (ch == '`') {
          if (return_screen) {
            _task->setCurrScreen(return_screen);
          } else {
            _task->gotoHomeScreen();
          }
          return;
        }

        if (cursor_pos < sizeof(input_buffer) - 1) {
          if (shift_active && ch >= 'a' && ch <= 'z') {
            input_buffer[cursor_pos++] = ch - 32;  // Convert to uppercase
            shift_active = false;
          } else {
            input_buffer[cursor_pos++] = ch;
          }
          input_buffer[cursor_pos] = 0;
        }
      }
    }
  }

  bool handleInput(char c) override {
    // Navigation key handling from mapped keys (arrow keys, etc)
    if (c == KEY_PREV || c == KEY_LEFT) {
      if (return_screen) {
        _task->setCurrScreen(return_screen);
      } else {
        _task->gotoHomeScreen();
      }
      return true;
    }

    return false;
  }
};

class ContactSelectScreen : public UIScreen {
  UITask* _task;
  TextInputScreen* _text_input;
  ContactInfo contacts[10];
  int num_contacts;
  int selected_idx;
  int page_offset;
  int total_contacts;

public:
  ContactSelectScreen(UITask* task, TextInputScreen* text_input)
    : _task(task), _text_input(text_input), num_contacts(0), selected_idx(0), page_offset(0), total_contacts(0) {}

  void refresh() {
    // Get stored contacts from the mesh
    total_contacts = the_mesh.getNumContacts();
    page_offset = 0;
    loadPage();
  }

  void loadPage() {
    // Load contacts for current page
    num_contacts = 0;
    int start_idx = page_offset;
    int end_idx = min(start_idx + 10, total_contacts);

    for (int i = start_idx; i < end_idx; i++) {
      ContactInfo contact;
      if (the_mesh.getContactByIdx(i, contact)) {
        contacts[num_contacts++] = contact;
      }
    }

    // Adjust selected_idx if it's out of bounds
    if (selected_idx >= num_contacts) {
      selected_idx = num_contacts > 0 ? num_contacts - 1 : 0;
    }
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    display.setCursor(0, 0);

    char header[40];
    if (total_contacts > 0) {
      int current_page = (page_offset / 10) + 1;
      int total_pages = (total_contacts + 9) / 10; // Ceiling division
      snprintf(header, sizeof(header), "Contacts: %d/%d (p%d)", page_offset + selected_idx + 1, total_contacts, current_page);
    } else {
      snprintf(header, sizeof(header), "Select Recipient");
    }
    display.print(header);

    display.setColor(DisplayDriver::LIGHT);
    display.drawRect(0, 11, display.width(), 1);

    int y = 14;
    int display_count = min(num_contacts, 4); // Show max 4 contacts on screen
    for (int i = 0; i < display_count; i++) {
      if (i == selected_idx) {
        display.setColor(DisplayDriver::YELLOW);
        display.setCursor(0, y);
        display.print(">");
      } else {
        display.setColor(DisplayDriver::LIGHT);
      }

      display.setCursor(8, y);
      char line[40];
      snprintf(line, sizeof(line), "%.30s", contacts[i].name);
      display.print(line);
      y += 11;
    }

    if (num_contacts == 0) {
      display.setColor(DisplayDriver::RED);
      display.setCursor(0, 20);
      display.print("No contacts found");
      display.setCursor(0, 32);
      display.print("Add contacts via app");
    } else if (total_contacts > 4) {
      // Show scroll indicator
      display.setColor(DisplayDriver::BLUE);
      display.setCursor(0, display.height() - 10);
      display.print("Use arrows to scroll");
    }

    return 500;
  }

  bool handleInput(char c) override {
    if (c == KEY_PREV || c == KEY_LEFT) {
      _task->gotoHomeScreen();
      return true;
    }

    if (c == KEY_UP) {
      if (selected_idx > 0) {
        selected_idx--;
      } else if (page_offset > 0) {
        // Scroll to previous page
        page_offset -= 10;
        if (page_offset < 0) page_offset = 0;
        loadPage();
        selected_idx = min(3, num_contacts - 1); // Position at bottom of screen
      }
      return true;
    }

    if (c == KEY_DOWN) {
      if (selected_idx < min(num_contacts - 1, 3)) {
        // Can still scroll down on current page
        selected_idx++;
      } else if (page_offset + num_contacts < total_contacts) {
        // Load next page
        page_offset += 10;
        loadPage();
        selected_idx = 0; // Position at top of screen
      }
      return true;
    }

    if (c == KEY_ENTER || c == KEY_RIGHT) {
      if (num_contacts > 0 && selected_idx < num_contacts) {
        _text_input->setRecipient(&contacts[selected_idx]);
        _text_input->setReturnScreen(NULL);
        _task->setCurrScreen(_text_input);
      }
      return true;
    }

    return false;
  }
};

// ============================================================================
// ChannelMessageScreen - View messages for a specific channel
// ============================================================================
class ChannelMessageScreen : public UIScreen {
  UITask* _task;
  UIScreen* _channel_list;
  uint8_t current_channel_idx;
  char channel_name[32];
  int scroll_offset;
  int visible_messages;

  struct DisplayMessage {
    uint32_t timestamp;
    int8_t snr;
    uint8_t path_len;
    char text[128];
  };
  DisplayMessage messages[CHANNEL_MSG_BUFFER_SIZE];
  int num_messages;

public:
  ChannelMessageScreen(UITask* task, UIScreen* channel_list)
    : _task(task), _channel_list(channel_list), current_channel_idx(0), scroll_offset(0), visible_messages(1) {
    channel_name[0] = '\0';
    num_messages = 0;
  }

  void setChannel(uint8_t channel_idx) {
    current_channel_idx = channel_idx;
    scroll_offset = 0;

    // Get channel name
    ChannelDetails ch;
    if (the_mesh.getChannel(channel_idx, ch)) {
      strncpy(channel_name, ch.name, 31);
      channel_name[31] = '\0';
    } else {
      snprintf(channel_name, sizeof(channel_name), "Channel %d", channel_idx);
    }

    // Load messages for this channel
    loadMessages();
  }

  void loadMessages() {
    num_messages = 0;

    // Collect all messages for this channel from the buffer
    for (int i = 0; i < CHANNEL_MSG_BUFFER_SIZE; i++) {
      const auto& msg = _task->channel_msg_buffer[i];
      if (msg.valid && msg.channel_idx == current_channel_idx) {
        messages[num_messages].timestamp = msg.timestamp;
        messages[num_messages].snr = msg.snr;
        messages[num_messages].path_len = msg.path_len;
        strncpy(messages[num_messages].text, msg.text, 127);
        messages[num_messages].text[127] = '\0';
        num_messages++;
      }
    }

    // Sort by timestamp (newest first) - simple bubble sort
    for (int i = 0; i < num_messages - 1; i++) {
      for (int j = 0; j < num_messages - i - 1; j++) {
        if (messages[j].timestamp < messages[j + 1].timestamp) {
          DisplayMessage temp = messages[j];
          messages[j] = messages[j + 1];
          messages[j + 1] = temp;
        }
      }
    }
  }

  int render(DisplayDriver& display) override {
    display.clear();
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);

    // Header with channel name
    display.setCursor(2, 2);
    char header[30];
    strncpy(header, channel_name, 29);
    header[29] = '\0';
    display.print(header);

    if (num_messages == 0) {
      display.setCursor(5, 20);
      display.print("No messages");
      display.setCursor(5, 35);
      display.print("Enter=compose");
      return 0;
    }

    // Display messages
    int y = 15;
    const int line_height = 11;
    const int msg_spacing = 3;
    const int footer_height = 10;
    const int msg_block_height = line_height * 2 + msg_spacing;
    const int available_height = display.height() - footer_height - y;
    visible_messages = max(1, available_height / msg_block_height);

    int start_idx = scroll_offset;
    int end_idx = min(num_messages, start_idx + visible_messages);

    for (int i = start_idx; i < end_idx; i++) {
      if (y + msg_block_height > display.height() - footer_height) break;
      // Timestamp + SNR + path length
      display.setCursor(2, y);

      // Convert timestamp to relative time (assuming it's seconds since epoch)
      uint32_t now = millis() / 1000;
      uint32_t age_secs = (now > messages[i].timestamp) ? (now - messages[i].timestamp) : 0;

      char info[40];
      if (age_secs < 60) {
        snprintf(info, sizeof(info), "%lus", age_secs);
      } else if (age_secs < 3600) {
        snprintf(info, sizeof(info), "%lum", age_secs / 60);
      } else {
        snprintf(info, sizeof(info), "%luh", age_secs / 3600);
      }

      // Add SNR and hops
      char snr_str[10];
      snprintf(snr_str, sizeof(snr_str), " %ddB", (int)(messages[i].snr / 4));
      strcat(info, snr_str);

      if (messages[i].path_len < 0xFF) {
        char hop_str[10];
        snprintf(hop_str, sizeof(hop_str), " %dh", messages[i].path_len);
        strcat(info, hop_str);
      }

      display.print(info);
      y += line_height;

      // Message text (word wrap if needed)
      display.setCursor(5, y);

      // Truncate long messages
      char text_buf[100];
      strncpy(text_buf, messages[i].text, 99);
      text_buf[99] = '\0';

      // Simple word wrap - if too long, truncate with "..."
      if (strlen(text_buf) > 35) {
        text_buf[32] = '.';
        text_buf[33] = '.';
        text_buf[34] = '.';
        text_buf[35] = '\0';
      }

      display.print(text_buf);
      y += line_height;

      // Add spacing between messages
      y += msg_spacing;
    }

    // Footer with navigation
    display.setCursor(2, display.height() - 10);
    if (num_messages > visible_messages) {
      display.print("^/v scroll  Enter=compose");
    } else {
      display.print("Enter=compose  ESC=back");
    }

    return 0;
  }

  bool handleInput(char c) override {
    if (c == KEY_ENTER) {
      _task->startComposingChannel(current_channel_idx);
      return true;
    }

    if (c == KEY_UP) {
      if (scroll_offset > 0) {
        scroll_offset--;
      }
      return true;
    }

    if (c == KEY_DOWN) {
      if (scroll_offset < num_messages - visible_messages) {
        scroll_offset++;
      }
      return true;
    }

    if (c == KEY_PREV || c == KEY_LEFT) {
      _task->setCurrScreen(_channel_list);
      return true;
    }

    return false;
  }

  void poll() override {
    // Refresh messages periodically in case new ones arrived
    static unsigned long last_refresh = 0;
    if (millis() - last_refresh > 2000) {
      loadMessages();
      last_refresh = millis();
    }
  }
};

// ============================================================================
// ChannelListScreen - Browse and select channels
// ============================================================================
class ChannelListScreen : public UIScreen {
  UITask* _task;
  UIScreen* _channel_msg;
  ChannelDetails channels[MAX_GROUP_CHANNELS];
  uint8_t channel_indices[MAX_GROUP_CHANNELS];  // Track actual channel indices
  int num_channels;
  int selected_idx;

public:
  ChannelListScreen(UITask* task, UIScreen* channel_msg) : _task(task), _channel_msg(channel_msg) {
    num_channels = 0;
    selected_idx = 0;
  }

  void setChannelMsgScreen(UIScreen* screen) { _channel_msg = screen; }

  void refresh() {
    num_channels = 0;
    selected_idx = 0;

    // Load all configured channels
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      ChannelDetails ch;
      if (the_mesh.getChannel(i, ch) && ch.name[0] != '\0') {
        channels[num_channels] = ch;
        channel_indices[num_channels] = i;  // Store actual channel index
        num_channels++;
      }
    }
  }

  int render(DisplayDriver& display) override {
    display.clear();
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);

    if (num_channels == 0) {
      display.setCursor(5, 10);
      display.print("No channels configured");
      display.setCursor(5, 25);
      display.print("Use companion app to");
      display.setCursor(5, 40);
      display.print("add channels");
      return 0;
    }

    // Header
    display.setCursor(2, 2);
    char header[40];
    sprintf(header, "Channels: %d", num_channels);
    display.print(header);

    // Show up to 5 channels at a time
    int start_idx = max(0, selected_idx - 2);
    int end_idx = min(num_channels, start_idx + 5);

    int y = 15;
    for (int i = start_idx; i < end_idx; i++) {
      bool is_selected = (i == selected_idx);

      if (is_selected) {
        display.fillRect(0, y - 2, display.width(), 12);
        display.setColor(DisplayDriver::DARK);
      } else {
        display.setColor(DisplayDriver::LIGHT);
      }

      display.setCursor(5, y);

      // Channel name (truncate if too long)
      char name_buf[40];
      strncpy(name_buf, channels[i].name, 24);
      name_buf[24] = '\0';

      // Add message count using actual channel index
      int msg_count = _task->getChannelMessageCount(channel_indices[i]);
      if (msg_count > 0) {
        char count_str[10];
        sprintf(count_str, " (%d)", msg_count);
        strcat(name_buf, count_str);
      }

      display.print(name_buf);

      if (is_selected) {
        display.setColor(DisplayDriver::LIGHT);
      }

      y += 12;
    }

    // Scroll indicator
    display.setCursor(2, display.height() - 10);
    if (num_channels > 5) {
      display.print("^/v scroll  Enter=view");
    } else {
      display.print("Enter=view  ESC=back");
    }

    return 0;
  }

  bool handleInput(char c) override {
    if (num_channels == 0) {
      if (c == KEY_PREV || c == KEY_LEFT) {
        _task->gotoHomeScreen();
        return true;
      }
      return false;
    }

    if (c == KEY_UP) {
      if (selected_idx > 0) {
        selected_idx--;
      }
      return true;
    }

    if (c == KEY_DOWN) {
      if (selected_idx < num_channels - 1) {
        selected_idx++;
      }
      return true;
    }

    if (c == KEY_PREV || c == KEY_LEFT) {
      _task->gotoHomeScreen();
      return true;
    }

    if (c == KEY_ENTER || c == KEY_RIGHT) {
      if (num_channels > 0 && selected_idx < num_channels) {
        // Set selected channel and switch to message view (use actual channel index)
        ((ChannelMessageScreen*)_channel_msg)->setChannel(channel_indices[selected_idx]);
        _task->setCurrScreen(_channel_msg);
      }
      return true;
    }

    return false;
  }
};

#endif  // M5STACK_CARDPUTER_ADV

// ============================================================================

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _auto_off = millis() + AUTO_OFF_MILLIS;

#if defined(PIN_USER_BTN)
  user_btn.begin();
#endif
#if defined(PIN_USER_BTN_ANA)
  analog_btn.begin();
#endif

  _node_prefs = node_prefs;
  if (_display != NULL) {
    _display->turnOn();
  }

#ifdef PIN_BUZZER
  buzzer.begin();
  buzzer.quiet(_node_prefs->buzzer_quiet);
#endif

#ifdef PIN_VIBRATION
  vibration.begin();
#endif

  ui_started_at = millis();
  _alert_expiry = 0;

  splash = new SplashScreen(this);
  home = new HomeScreen(this, &rtc_clock, sensors, node_prefs);
  msg_preview = new MsgPreviewScreen(this, &rtc_clock);

#ifdef M5STACK_CARDPUTER_ADV
  text_input = new TextInputScreen(this);
  contact_select = new ContactSelectScreen(this, (TextInputScreen*)text_input);

  // Create channel screens with forward/back references
  channel_list = new ChannelListScreen(this, NULL);  // Will set channel_msg later
  channel_msg = new ChannelMessageScreen(this, channel_list);
  ((ChannelListScreen*)channel_list)->setChannelMsgScreen(channel_msg);  // Set forward reference

  // Initialize channel message buffer
  channel_msg_write_idx = 0;
  for (int i = 0; i < CHANNEL_MSG_BUFFER_SIZE; i++) {
    channel_msg_buffer[i].valid = false;
  }
#endif

  setCurrScreen(splash);
}

void UITask::showAlert(const char* text, int duration_millis) {
  strcpy(_alert, text);
  _alert_expiry = millis() + duration_millis;
}

void UITask::notify(UIEventType t) {
#if defined(PIN_BUZZER)
switch(t){
  case UIEventType::contactMessage:
    // gemini's pick
    buzzer.play("MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
    break;
  case UIEventType::channelMessage:
    buzzer.play("kerplop:d=16,o=6,b=120:32g#,32c#");
    break;
  case UIEventType::ack:
    buzzer.play("ack:d=32,o=8,b=120:c");
    break;
  case UIEventType::roomMessage:
  case UIEventType::newContactMessage:
  case UIEventType::none:
  default:
    break;
}
#endif

#ifdef PIN_VIBRATION
  // Trigger vibration for all UI events except none
  if (t != UIEventType::none) {
    vibration.trigger();
  }
#endif
}


void UITask::msgRead(int msgcount) {
  _msgcount = msgcount;
  if (msgcount == 0) {
    gotoHomeScreen();
  }
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) {
  _msgcount = msgcount;

  ((MsgPreviewScreen *) msg_preview)->addPreview(path_len, from_name, text);
  setCurrScreen(msg_preview);

  if (_display != NULL) {
    if (!_display->isOn()) _display->turnOn();
    _auto_off = millis() + AUTO_OFF_MILLIS;  // extend the auto-off timer
    _next_refresh = 100;  // trigger refresh
  }
}

void UITask::userLedHandler() {
#ifdef PIN_STATUS_LED
  int cur_time = millis();
  if (cur_time > next_led_change) {
    if (led_state == 0) {
      led_state = 1;
      if (_msgcount > 0) {
        last_led_increment = LED_ON_MSG_MILLIS;
      } else {
        last_led_increment = LED_ON_MILLIS;
      }
      next_led_change = cur_time + last_led_increment;
    } else {
      led_state = 0;
      next_led_change = cur_time + LED_CYCLE_MILLIS - last_led_increment;
    }
    digitalWrite(PIN_STATUS_LED, led_state == LED_STATE_ON);
  }
#endif
}

void UITask::setCurrScreen(UIScreen* c) {
  curr = c;
  _next_refresh = 100;
}

void UITask::wakeDisplay() {
  checkDisplayOn(0);
}

#ifdef M5STACK_CARDPUTER_ADV
void UITask::startComposingMessage() {
  ((ContactSelectScreen*)contact_select)->refresh();
  setCurrScreen(contact_select);
}

void UITask::startComposingChannel(uint8_t channel_idx) {
  ((TextInputScreen*)text_input)->setChannel(channel_idx);
  ((TextInputScreen*)text_input)->setReturnScreen(channel_msg);
  setCurrScreen(text_input);
}

void UITask::startViewingChannels() {
  ((ChannelListScreen*)channel_list)->refresh();
  setCurrScreen(channel_list);
}

void UITask::newChannelMessage(uint8_t channel_idx, uint8_t path_len, int8_t snr, uint32_t timestamp, const char* text) {
  // Store message in circular buffer
  channel_msg_buffer[channel_msg_write_idx].channel_idx = channel_idx;
  channel_msg_buffer[channel_msg_write_idx].path_len = path_len;
  channel_msg_buffer[channel_msg_write_idx].snr = snr;
  channel_msg_buffer[channel_msg_write_idx].timestamp = timestamp;
  strncpy(channel_msg_buffer[channel_msg_write_idx].text, text, 127);
  channel_msg_buffer[channel_msg_write_idx].text[127] = '\0';
  channel_msg_buffer[channel_msg_write_idx].valid = true;

  // Advance write index (circular)
  channel_msg_write_idx = (channel_msg_write_idx + 1) % CHANNEL_MSG_BUFFER_SIZE;
}

int UITask::getChannelMessageCount(uint8_t channel_idx) {
  int count = 0;
  for (int i = 0; i < CHANNEL_MSG_BUFFER_SIZE; i++) {
    if (channel_msg_buffer[i].valid && channel_msg_buffer[i].channel_idx == channel_idx) {
      count++;
    }
  }
  return count;
}
#endif

/*
  hardware-agnostic pre-shutdown activity should be done here
*/
void UITask::shutdown(bool restart){

  #ifdef PIN_BUZZER
  /* note: we have a choice here -
     we can do a blocking buzzer.loop() with non-deterministic consequences
     or we can set a flag and delay the shutdown for a couple of seconds
     while a non-blocking buzzer.loop() plays out in UITask::loop()
  */
  buzzer.shutdown();
  uint32_t buzzer_timer = millis(); // fail-safe shutdown
  while (buzzer.isPlaying() && (millis() - 2500) < buzzer_timer)
    buzzer.loop();

  #endif // PIN_BUZZER

  if (restart) {
    _board->reboot();
  } else {
    _display->turnOff();
    radio_driver.powerOff();
    _board->powerOff();
  }
}

bool UITask::isButtonPressed() const {
#ifdef PIN_USER_BTN
  return user_btn.isPressed();
#else
  return false;
#endif
}

void UITask::loop() {
  char c = 0;
#ifdef M5STACK_CARDPUTER_ADV
  M5Cardputer.update();
  static bool kb_seen = false;
  static uint32_t next_kb_init = 0;
  if (!kb_seen) {
    if (M5Cardputer.Keyboard.isPressed() || M5Cardputer.Keyboard.isChange()) {
      kb_seen = true;
    } else if (millis() >= next_kb_init) {
      M5Cardputer.Keyboard.begin();
      next_kb_init = millis() + 2000;
    }
  }
#endif
#if UI_HAS_JOYSTICK
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);  // REVISIT: could be mapped to different key code
  }
  ev = joystick_left.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_LEFT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_LEFT);
  }
  ev = joystick_right.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_RIGHT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_RIGHT);
  }
  ev = back_btn.check();
  if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#elif defined(PIN_USER_BTN)
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_NEXT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_PREV);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#endif
#if defined(PIN_USER_BTN_ANA)
  if (abs(millis() - _analogue_pin_read_millis) > 10) {
    ev = analog_btn.check();
    if (ev == BUTTON_EVENT_CLICK) {
      c = checkDisplayOn(KEY_NEXT);
    } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      c = handleLongPress(KEY_ENTER);
    } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
      c = handleDoubleClick(KEY_PREV);
    } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
      c = handleTripleClick(KEY_SELECT);
    }
    _analogue_pin_read_millis = millis();
  }
#endif
#if defined(BACKLIGHT_BTN)
  if (millis() > next_backlight_btn_check) {
    bool touch_state = digitalRead(PIN_BUTTON2);
#if defined(DISP_BACKLIGHT)
    digitalWrite(DISP_BACKLIGHT, !touch_state);
#elif defined(EXP_PIN_BACKLIGHT)
    expander.digitalWrite(EXP_PIN_BACKLIGHT, !touch_state);
#endif
    next_backlight_btn_check = millis() + 300;
  }
#endif

#ifdef M5STACK_CARDPUTER_ADV
  // Skip keyboard handling if on text_input screen - it polls keyboard in poll() method
  if (c == 0 && curr != text_input && M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    auto& status = M5Cardputer.Keyboard.keysState();

    // Map keyboard to navigation keys
    c = mapCardputerKey(status);
    if (c != 0) {
      c = checkDisplayOn(c);
    }
  }
#endif

  if (c != 0 && curr) {
    curr->handleInput(c);
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 100;  // trigger refresh
  }

  userLedHandler();

#ifdef PIN_BUZZER
  if (buzzer.isPlaying())  buzzer.loop();
#endif

  if (curr) curr->poll();

  if (_display != NULL && _display->isOn()) {
    if (millis() >= _next_refresh && curr) {
      _display->startFrame();
      int delay_millis = curr->render(*_display);
      if (millis() < _alert_expiry) {  // render alert popup
        _display->setTextSize(1);
        int y = _display->height() / 3;
        int p = _display->height() / 32;
        _display->setColor(DisplayDriver::DARK);
        _display->fillRect(p, y, _display->width() - p*2, y);
        _display->setColor(DisplayDriver::LIGHT);  // draw box border
        _display->drawRect(p, y, _display->width() - p*2, y);
        _display->drawTextCentered(_display->width() / 2, y + p*3, _alert);
        _next_refresh = _alert_expiry;   // will need refresh when alert is dismissed
      } else {
        _next_refresh = millis() + delay_millis;
      }
      _display->endFrame();
    }
#if AUTO_OFF_MILLIS > 0
    if (millis() > _auto_off) {
      _display->turnOff();
    }
#endif
  }

#ifdef PIN_VIBRATION
  vibration.loop();
#endif

#ifdef AUTO_SHUTDOWN_MILLIVOLTS
  if (millis() > next_batt_chck) {
    uint16_t milliVolts = getBattMilliVolts();
    if (milliVolts > 0 && milliVolts < AUTO_SHUTDOWN_MILLIVOLTS) {

      // show low battery shutdown alert
      // we should only do this for eink displays, which will persist after power loss
      #if defined(THINKNODE_M1) || defined(LILYGO_TECHO)
      if (_display != NULL) {
        _display->startFrame();
        _display->setTextSize(2);
        _display->setColor(DisplayDriver::RED);
        _display->drawTextCentered(_display->width() / 2, 20, "Low Battery.");
        _display->drawTextCentered(_display->width() / 2, 40, "Shutting Down!");
        _display->endFrame();
      }
      #endif

      shutdown();

    }
    next_batt_chck = millis() + 8000;
  }
#endif
}

char UITask::checkDisplayOn(char c) {
  if (_display != NULL) {
    if (!_display->isOn()) {
      _display->turnOn();   // turn display on and consume event
      c = 0;
    }
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 0;  // trigger refresh
  }
  return c;
}

char UITask::handleLongPress(char c) {
  if (millis() - ui_started_at < 8000) {   // long press in first 8 seconds since startup -> CLI/rescue
    the_mesh.enterCLIRescue();
    c = 0;   // consume event
  }
  return c;
}

char UITask::handleDoubleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: double click triggered");
  checkDisplayOn(c);
  return c;
}

char UITask::handleTripleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: triple click triggered");
  checkDisplayOn(c);
  toggleBuzzer();
  c = 0;
  return c;
}

bool UITask::getGPSState() {
  if (_sensors != NULL) {
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        return !strcmp(_sensors->getSettingValue(i), "1");
      }
    }
  } 
  return false;
}

void UITask::toggleGPS() {
    if (_sensors != NULL) {
    // toggle GPS on/off
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        if (strcmp(_sensors->getSettingValue(i), "1") == 0) {
          _sensors->setSettingValue("gps", "0");
          notify(UIEventType::ack);
          showAlert("GPS: Disabled", 800);
        } else {
          _sensors->setSettingValue("gps", "1");
          notify(UIEventType::ack);
          showAlert("GPS: Enabled", 800);
        }
        _next_refresh = 0;
        break;
      }
    }
  }
}

void UITask::toggleBuzzer() {
    // Toggle buzzer quiet mode
  #ifdef PIN_BUZZER
    if (buzzer.isQuiet()) {
      buzzer.quiet(false);
      notify(UIEventType::ack);
      showAlert("Buzzer: ON", 800);
    } else {
      buzzer.quiet(true);
      showAlert("Buzzer: OFF", 800);
    }
    _node_prefs->buzzer_quiet = buzzer.isQuiet();
    the_mesh.savePrefs();
    _next_refresh = 0;  // trigger refresh
  #endif
}
