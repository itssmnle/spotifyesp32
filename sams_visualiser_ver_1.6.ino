#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SpotifyArduino.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <TJpg_Decoder.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>

// ==========================================
// CONFIGURATION & CREDENTIALS PLACEHOLDERS
// ==========================================

// --- SPOTIFY API CREDENTIALS ---
// Obtain these from https://developer.spotify.com/dashboard
char clientId[]     = "YOUR_SPOTIFY_CLIENT_ID";
char clientSecret[] = "YOUR_SPOTIFY_CLIENT_SECRET";
char refreshToken[] = "YOUR_SPOTIFY_REFRESH_TOKEN";

// --- OPENWEATHERMAP & TIMEZONE CONFIG ---
// Create a free account at https://openweathermap.org/ to get an API key
const char* openWeatherApiKey = "YOUR_OPENWEATHER_API_KEY";
String weatherCity            = "YOUR_CITY_NAME"; // e.g., "London", "Tokyo", "New York"

// UTC offset in seconds (e.g., GMT+7 = 25200, GMT+0 = 0, GMT-5 = -18000)
const long utcOffsetInSeconds = 25200; 

// --- HARDWARE TOUCH PINS (ESP32 CYD Defaults) ---
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

// --- TOUCH CALIBRATION ---
#define TOUCH_MIN_X 360
#define TOUCH_MAX_X 3680
#define TOUCH_MIN_Y 370
#define TOUCH_MAX_Y 3800

// --- LAYOUT CONSTANTS (320x240 landscape) ---
#define SCR_W 320
#define SCR_H 240

#define HEADER_H 26

#define ART_SIZE 135
#define ART_X    12
#define ART_Y    36

#define TEXT_X0 (ART_X + ART_SIZE + 12)
#define TEXT_X1 (SCR_W - 12)

// Vertical layout alignments for Spotify player elements
#define TITLE_Y       (ART_Y + 16)
#define ARTIST_Y      (TITLE_Y + 34)
#define ALBUM_Y       (ARTIST_Y + 24)
#define PAUSE_BADGE_Y (ALBUM_Y + 28)

#define TIME_TEXT_Y 188
#define BAR_Y       204
#define BAR_H       6
#define BAR_X       15
#define BAR_W       (SCR_W - BAR_X * 2)

#define MAX_IMAGE_SIZE 40000

// --- COLOR PALETTE ---
uint16_t COLOR_WHITE         = TFT_WHITE;
uint16_t COLOR_GREY          = 0xB5B6;     
uint16_t COLOR_DARKGREY      = 0x4208; 
uint16_t COLOR_SPOTIFY_GREEN = 0x3EDF; 
uint16_t COLOR_BG            = TFT_BLACK;         

// --- OBJECTS & GLOBAL VARIABLES ---
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

SPIClass touchSpi = SPIClass(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();
WiFiClientSecure client;
SpotifyArduino spotify(client, clientId, clientSecret, refreshToken);
WiFiMulti wifiMulti;

enum ScreenState { CLOCK_WEATHER_MODE, PLAYER_MODE, FULLSCREEN_ART_MODE };
ScreenState currentScreen = CLOCK_WEATHER_MODE;

unsigned long lastRequest       = 0;
unsigned long lastSecondTick     = 0;
unsigned long lastWeatherUpdate = 0;
unsigned long pauseStartMillis  = 0;
int lastDisplayedMinute         = -1;

bool isPlaying             = false;
bool lastWasPlaying        = false;
String currentImgUrl       = "";
String lastTrackName       = "";
String currentTrackTitle   = "";
String currentArtistName   = "";
String currentAlbumName    = "";
String weatherTemp         = "--°C";
String weatherCondition   = "Clear";

String marqueeText         = "";
int marqueeOffset          = 0;
bool marqueeActive         = false;
unsigned long lastMarqueeUpdate = 0;

long currentProgressMs = 0;
long totalDurationMs   = 0;

// Forward Declarations
void drawBaseInterface();
void redrawPlayerScreen();
void drawClockWeatherScreen();
void drawProgressRow(long progressMs, long durationMs);
void updateMarquee();
void drawCover(const char *url);
void drawCoverFullscreen(const char *url);
void updateDisplay(CurrentlyPlaying current);
void drawFullscreenScreen();
void drawPausedBadge();
void fetchWeather();
void drawHeader();
void drawWeatherIcon(int x, int y, String condition);
void drawMarqueeTitle();

String fitText(const String &text, int maxWidth) {
  if (tft.textWidth(text) <= maxWidth) return text;
  String t = text;
  while (t.length() > 0 && tft.textWidth(t + "...") > maxWidth) {
    t.remove(t.length() - 1);
  }
  return t + "...";
}

String formatTime(long ms) {
  if (ms < 0) ms = 0;
  long totalSec = ms / 1000;
  int m = totalSec / 60;
  int s = totalSec % 60;
  return String(m) + ":" + (s < 10 ? "0" : "") + String(s);
}

// Header Date/Time formatter for Player Mode ("24 oct 13:55")
String getSpotifyHeaderDateTime() {
  time_t adjustedTime = timeClient.getEpochTime();
  struct tm *tm_info = gmtime(&adjustedTime);
  if (!tm_info) return "";
  
  int day = tm_info->tm_mday;
  int monthIdx = tm_info->tm_mon;
  int hour = tm_info->tm_hour;
  int min = tm_info->tm_min;
  
  const char* monthNames[] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
  String monthStr = (monthIdx >= 0 && monthIdx < 12) ? monthNames[monthIdx] : "";
  
  String minStr = (min < 10 ? "0" : "") + String(min);
  String hourStr = (hour < 10 ? "0" : "") + String(hour);
  
  return String(day) + " " + monthStr + " " + hourStr + ":" + minStr;
}

// Date formatter for Clock Mode ("Mon Jun 23")
String getClockDateString() {
  time_t adjustedTime = timeClient.getEpochTime();
  struct tm *tm_info = gmtime(&adjustedTime);
  if (!tm_info) return "";
  
  int day = tm_info->tm_mday;
  int monthIdx = tm_info->tm_mon;
  int wday = tm_info->tm_wday;
  
  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  
  String wdayStr = (wday >= 0 && wday < 7) ? days[wday] : "";
  String monthStr = (monthIdx >= 0 && monthIdx < 12) ? months[monthIdx] : "";
  
  return wdayStr + " " + monthStr + " " + String(day);
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (y >= tft.height()) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

void configModeCallback(WiFiManager *myWiFiManager) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("WiFi Setup Portal");
  tft.println(myWiFiManager->getConfigPortalSSID());
}

void setup() {
  Serial.begin(115200);

  // Turn on Backlight (GPIO 21 on CYD)
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);
  tft.setTextWrap(false);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSpi);
  ts.setRotation(1);

  // --- HARDCODED PREFERRED NETWORKS (OPTIONAL) ---
  wifiMulti.addAP("YOUR_PRIMARY_SSID", "YOUR_PRIMARY_PASSWORD");
  wifiMulti.addAP("YOUR_SECONDARY_SSID", "YOUR_SECONDARY_PASSWORD");

  tft.fillScreen(COLOR_BG);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_WHITE, COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Connecting to Wi-Fi...", SCR_W / 2, SCR_H / 2);
  tft.setTextDatum(TL_DATUM);

  unsigned long startAttempt = millis();
  bool connected = false;
  
  while (millis() - startAttempt < 10000) {
    if (wifiMulti.run() == WL_CONNECTED) {
      connected = true;
      break;
    }
    delay(500);
  }

  // Fallback to WiFiManager Captive Portal if hardcoded APs fail
  if (!connected) {
    WiFiManager wm;
    wm.setAPCallback(configModeCallback);
    if (!wm.autoConnect("Spotify-CYD")) {
      ESP.restart();
    }
  }

  timeClient.begin();
  timeClient.update();

  client.setInsecure();
  spotify.refreshAccessToken();

  // Boot into Clock/Weather Screen
  drawClockWeatherScreen();
}

void loop() {
  timeClient.update();

  // Periodic weather update every 10 minutes
  if (millis() - lastWeatherUpdate > 600000 || lastWeatherUpdate == 0) {
    fetchWeather();
    lastWeatherUpdate = millis();
  }

  // Touch handling
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int screenX = constrain(map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCR_W), 0, SCR_W);
    int screenY = constrain(map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCR_H), 0, SCR_H);

    if (currentScreen == CLOCK_WEATHER_MODE) {
      spotify.getCurrentlyPlaying(updateDisplay, "US");
      lastRequest = millis();
      if (isPlaying) {
        currentScreen = PLAYER_MODE;
        redrawPlayerScreen();
      }
      delay(300);
    } 
    else if (currentScreen == PLAYER_MODE) {
      if (screenX >= ART_X && screenX <= (ART_X + ART_SIZE) && screenY >= ART_Y && screenY <= (ART_Y + ART_SIZE)) {
        spotify.getCurrentlyPlaying(updateDisplay, "US");
        lastRequest = millis();
        drawFullscreenScreen();
        delay(300);
      }
    } 
    else if (currentScreen == FULLSCREEN_ART_MODE) {
      if (screenX >= 240 && screenY <= 70) {
        currentScreen = PLAYER_MODE;
        redrawPlayerScreen();
        delay(300);
      }
    }
  }

  // Minute-change check to redraw clock without full-screen flicker
  int currentMinute = timeClient.getMinutes();
  if (currentScreen == CLOCK_WEATHER_MODE && currentMinute != lastDisplayedMinute) {
    drawClockWeatherScreen();
  }

  // 1-second tick loop
  if (millis() - lastSecondTick >= 1000) {
    lastSecondTick = millis();
    
    if (currentScreen == PLAYER_MODE) {
      drawHeader();
    }

    if (isPlaying && totalDurationMs > 0) {
      currentProgressMs += 1000;
      if (currentProgressMs > totalDurationMs) {
        currentProgressMs = totalDurationMs;
      }
      if (currentScreen == PLAYER_MODE) {
        drawProgressRow(currentProgressMs, totalDurationMs);
      }

      if ((totalDurationMs - currentProgressMs <= 8000) && (millis() - lastRequest > 3000)) {
        spotify.getCurrentlyPlaying(updateDisplay, "US");
        lastRequest = millis();
      }
    }

    // Auto-return to Clock Mode after 30 seconds of paused/stopped playback
    if (!isPlaying && currentScreen == PLAYER_MODE) {
      if (pauseStartMillis == 0) pauseStartMillis = millis();
      else if (millis() - pauseStartMillis > 30000) {
        currentScreen = CLOCK_WEATHER_MODE;
        lastDisplayedMinute = -1; // Force clock redraw on return
        drawClockWeatherScreen();
      }
    } else {
      pauseStartMillis = 0;
    }
  }

  if (currentScreen == PLAYER_MODE) {
    updateMarquee();
  }

  // Poll Spotify API every 4.5 seconds
  if (millis() - lastRequest > 4500) {
    spotify.getCurrentlyPlaying(updateDisplay, "US");
    lastRequest = millis();
  }
}

// ==========================================
// UI DRAWING FUNCTIONS
// ==========================================

void drawHeader() {
  tft.fillRect(0, 0, SCR_W, HEADER_H, TFT_BLACK);
  
  // Left header title
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("sam's buddy | v1.6", 10, HEADER_H / 2);

  // Right header readout
  tft.setTextColor(COLOR_SPOTIFY_GREEN, TFT_BLACK);
  tft.setTextDatum(MR_DATUM);
  if (currentScreen == CLOCK_WEATHER_MODE) {
    tft.drawString("version 1.6", SCR_W - 10, HEADER_H / 2);
  } else {
    tft.drawString(getSpotifyHeaderDateTime(), SCR_W - 10, HEADER_H / 2);
  }
  tft.setTextDatum(TL_DATUM);
}

void drawWeatherIcon(int x, int y, String condition) {
  condition.toLowerCase();
  if (condition.indexOf("rain") >= 0 || condition.indexOf("drizzle") >= 0) {
    tft.fillCircle(x, y + 4, 5, COLOR_GREY);
    tft.fillCircle(x + 5, y + 2, 7, COLOR_GREY);
    tft.fillCircle(x + 10, y + 4, 5, COLOR_GREY);
    tft.fillRect(x - 2, y + 6, 14, 4, COLOR_GREY);
    tft.drawLine(x + 2, y + 11, x, y + 15, TFT_CYAN);
    tft.drawLine(x + 7, y + 11, x + 5, y + 15, TFT_CYAN);
  } else if (condition.indexOf("cloud") >= 0) {
    tft.fillCircle(x, y + 5, 5, COLOR_GREY);
    tft.fillCircle(x + 6, y + 2, 7, COLOR_GREY);
    tft.fillCircle(x + 12, y + 5, 5, COLOR_GREY);
    tft.fillRect(x - 2, y + 6, 16, 5, COLOR_GREY);
  } else {
    tft.fillCircle(x + 5, y + 6, 6, TFT_YELLOW);
    tft.drawCircle(x + 5, y + 6, 9, TFT_ORANGE);
  }
}

void drawClockWeatherScreen() {
  if (currentScreen != CLOCK_WEATHER_MODE) return;
  
  lastDisplayedMinute = timeClient.getMinutes();
  tft.fillScreen(COLOR_BG);
  drawHeader(); 

  // 1. Weather details padded safely below header
  int startX = 85;
  int startY = 44;
  drawWeatherIcon(startX, startY, weatherCondition);

  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_SPOTIFY_GREEN, COLOR_BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(weatherCity + "  " + weatherTemp + "  •  " + weatherCondition, startX + 22, startY + 6);

  // 2. Massive Clock (HH:MM)
  String rawTime = timeClient.getFormattedTime(); 
  String hoursMinutes = rawTime.substring(0, 5); 
  
  tft.setTextFont(4); 
  tft.setTextSize(4); 
  tft.setTextColor(COLOR_WHITE, COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(hoursMinutes, SCR_W / 2, 130);

  // 3. Centered Date
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY, COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(getClockDateString(), SCR_W / 2, 192);
  
  tft.setTextDatum(TL_DATUM);
}

void fetchWeather() {
  if (String(openWeatherApiKey) == "YOUR_OPENWEATHER_API_KEY") return;
  
  HTTPClient http;
  String url = "https://api.openweathermap.org/data/2.5/weather?q=" + weatherCity + "&units=metric&appid=" + String(openWeatherApiKey);
  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    JsonDocument doc;
    deserializeJson(doc, payload);
    float temp = doc["main"]["temp"];
    weatherTemp = String((int)temp) + "°C";
    weatherCondition = doc["weather"][0]["main"].as<String>();
  }
  http.end();
}

void drawPausedBadge() {
  int colWidth = TEXT_X1 - TEXT_X0;
  tft.fillRect(TEXT_X0, PAUSE_BADGE_Y, colWidth, 20, COLOR_BG);
  if (!isPlaying) {
    tft.fillRoundRect(TEXT_X0, PAUSE_BADGE_Y, 70, 18, 3, TFT_ORANGE);
    tft.setTextColor(TFT_BLACK, TFT_ORANGE);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("PAUSED", TEXT_X0 + 35, PAUSE_BADGE_Y + 9);
    tft.setTextDatum(TL_DATUM);
  }
}

void drawArtPlaceholder() {
  tft.fillRoundRect(ART_X, ART_Y, ART_SIZE, ART_SIZE, 6, COLOR_DARKGREY);
  int cx = ART_X + ART_SIZE / 2, cy = ART_Y + ART_SIZE / 2;
  tft.fillCircle(cx - 12, cy + 16, 8, COLOR_GREY);
  tft.fillRect(cx - 5, cy - 25, 4, 42, COLOR_GREY);
  tft.fillRect(cx - 5, cy - 25, 18, 7, COLOR_GREY);
}

void fillBackgroundExceptArt() {
  tft.fillRect(0, HEADER_H, SCR_W, ART_Y - HEADER_H, COLOR_BG);
  tft.fillRect(0, ART_Y, ART_X, SCR_H - ART_Y, COLOR_BG);
  tft.fillRect(ART_X + ART_SIZE, ART_Y, SCR_W - (ART_X + ART_SIZE), SCR_H - ART_Y, COLOR_BG);
  tft.fillRect(ART_X, ART_Y + ART_SIZE, ART_SIZE, SCR_H - (ART_Y + ART_SIZE), COLOR_BG);
}

void drawBaseInterface() {
  tft.fillScreen(COLOR_BG);
  tft.setTextWrap(false);

  drawHeader();
  drawArtPlaceholder();

  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY, COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Waiting for", TEXT_X0, TITLE_Y);
  tft.drawString("playback...", TEXT_X0, TITLE_Y + 22);

  lastTrackName = "";
  currentImgUrl = "";
  marqueeText = "";
  marqueeOffset = 0;
  marqueeActive = false;
}

void redrawPlayerScreen() {
  drawBaseInterface();
  if (currentTrackTitle.length() > 0) {
    if (currentImgUrl.length() > 0) {
      drawCover(currentImgUrl.c_str());
    }
    fillBackgroundExceptArt();
    drawHeader();
    tft.drawRoundRect(ART_X - 3, ART_Y - 3, ART_SIZE + 6, ART_SIZE + 6, 8, COLOR_DARKGREY);
    
    setMarqueeText(currentTrackTitle);
    drawMarqueeTitle();

    int colWidth = TEXT_X1 - TEXT_X0;

    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GREY, COLOR_BG);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(fitText(currentArtistName, colWidth), TEXT_X0, ARTIST_Y);
    tft.drawString(fitText(currentAlbumName, colWidth), TEXT_X0, ALBUM_Y);
    
    drawPausedBadge();
    drawProgressRow(currentProgressMs, totalDurationMs);
  }
}

void setMarqueeText(const String &text) {
  int colWidth = TEXT_X1 - TEXT_X0;
  marqueeText = text;
  marqueeOffset = 0;

  tft.setTextFont(4);
  tft.setTextSize(1);
  marqueeActive = (tft.textWidth(marqueeText) > colWidth);
  lastMarqueeUpdate = millis();
}

void drawMarqueeTitle() {
  int colWidth = TEXT_X1 - TEXT_X0;

  tft.setTextFont(4);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_WHITE, COLOR_BG);
  tft.setTextDatum(TL_DATUM);

  tft.fillRect(TEXT_X0, TITLE_Y, colWidth, 24, COLOR_BG);
  tft.setViewport(TEXT_X0, TITLE_Y, colWidth, 24);

  if (!marqueeActive) {
    tft.drawString(fitText(marqueeText, colWidth), 0, 0);
    tft.resetViewport();
    return;
  }

  int textW = tft.textWidth(marqueeText);
  const int gap = 28;
  int x = -marqueeOffset;

  tft.drawString(marqueeText, x, 0);
  tft.drawString(marqueeText, x + textW + gap, 0);

  tft.resetViewport();
}

void updateMarquee() {
  if (!marqueeActive || currentScreen != PLAYER_MODE) return;

  if (millis() - lastMarqueeUpdate < 70) return;
  lastMarqueeUpdate = millis();

  tft.setTextFont(4);
  tft.setTextSize(1);

  int textW = tft.textWidth(marqueeText);
  const int gap = 28;
  marqueeOffset += 2;

  if (marqueeOffset >= textW + gap) {
    marqueeOffset = 0;
  }

  drawMarqueeTitle();
}

void drawTrackText(CurrentlyPlaying &current) {
  int colWidth = TEXT_X1 - TEXT_X0;

  setMarqueeText(String(current.trackName));
  drawMarqueeTitle();

  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY, COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(fitText(String(current.artists[0].artistName), colWidth), TEXT_X0, ARTIST_Y);
  tft.drawString(fitText(String(current.albumName), colWidth), TEXT_X0, ALBUM_Y);

  drawPausedBadge();
}

void drawProgressRow(long progressMs, long durationMs) {
  tft.fillRect(BAR_X, TIME_TEXT_Y, BAR_W, 12, COLOR_BG);

  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_GREY, COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(formatTime(progressMs), BAR_X, TIME_TEXT_Y);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(formatTime(durationMs), BAR_X + BAR_W, TIME_TEXT_Y);
  tft.setTextDatum(TL_DATUM);

  int fillW = durationMs > 0 ? map(progressMs, 0, durationMs, 0, BAR_W) : 0;
  fillW = constrain(fillW, 0, BAR_W);

  tft.fillRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, 2, COLOR_DARKGREY);
  if (fillW > 0) tft.fillRoundRect(BAR_X, BAR_Y, fillW, BAR_H, 2, COLOR_WHITE);
}

void updateDisplay(CurrentlyPlaying current) {
  bool trackChanged = (String(current.trackName) != lastTrackName);
  bool playStateChanged = (current.isPlaying != lastWasPlaying);
  
  isPlaying = current.isPlaying;
  lastWasPlaying = isPlaying;
  
  currentProgressMs = current.progressMs;
  totalDurationMs = current.durationMs;

  currentTrackTitle = String(current.trackName);
  currentArtistName = String(current.artists[0].artistName);
  currentAlbumName = String(current.albumName);

  if (isPlaying && currentScreen == CLOCK_WEATHER_MODE) {
    currentScreen = PLAYER_MODE;
    redrawPlayerScreen();
  }

  const char *newUrl = "";
  if (current.numImages > 0) {
    int imgIndex = 0;
    int minDiff = 9999;
    for (int i = 0; i < current.numImages; i++) {
      int diff = abs(current.albumImages[i].width - 300);
      if (diff < minDiff) {
        minDiff = diff;
        imgIndex = i;
      }
    }
    newUrl = current.albumImages[imgIndex].url;
  }

  bool artChanged = (String(newUrl) != currentImgUrl);
  if (artChanged && strlen(newUrl) > 0) {
    currentImgUrl = String(newUrl);
  }

  if (currentScreen == PLAYER_MODE) {
    if (playStateChanged) {
      drawHeader();
      drawPausedBadge();
    }

    if (trackChanged || artChanged) {
      if (artChanged && currentImgUrl.length() > 0) {
        drawCover(currentImgUrl.c_str());
      }

      fillBackgroundExceptArt();
      drawHeader();
      tft.drawRoundRect(ART_X - 3, ART_Y - 3, ART_SIZE + 6, ART_SIZE + 6, 8, COLOR_DARKGREY);
      drawTrackText(current);

      if (trackChanged) {
        lastTrackName = String(current.trackName);
      }
    }

    drawProgressRow(currentProgressMs, totalDurationMs);
  } 
  else if (currentScreen == FULLSCREEN_ART_MODE) {
    if (trackChanged || artChanged) {
      if (trackChanged) {
        lastTrackName = String(current.trackName);
      }
      drawFullscreenScreen();
    }
  }
}

void drawCover(const char *url) {
  if (url == nullptr || strlen(url) == 0) return;

  WiFiClientSecure artClient;
  artClient.setInsecure();
  artClient.setTimeout(8000);

  HTTPClient https;
  https.setConnectTimeout(8000);
  https.setTimeout(10000);
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!https.begin(artClient, String(url))) return;
  int httpCode = https.GET();
  if (httpCode != HTTP_CODE_OK) {
    https.end();
    return;
  }

  int contentLength = https.getSize();
  size_t capacity = (contentLength > 0 && contentLength < MAX_IMAGE_SIZE) ? (size_t)contentLength : MAX_IMAGE_SIZE;
  uint8_t *jpgBuffer = (uint8_t *)malloc(capacity);
  if (jpgBuffer == nullptr) {
    https.end();
    return;
  }

  WiFiClient *stream = https.getStreamPtr();
  size_t received = 0;
  unsigned long start = millis();

  while (millis() - start < 10000 && received < capacity) {
    int availableBytes = stream->available();
    if (availableBytes > 0) {
      size_t want = min((size_t)availableBytes, capacity - received);
      int got = stream->readBytes((char *)jpgBuffer + received, want);
      if (got > 0) {
        received += (size_t)got;
        start = millis();
      }
    } else {
      if (!https.connected() && contentLength < 0) break;
      delay(2);
    }
  }
  https.end();

  if (received < 2 || jpgBuffer[0] != 0xFF || jpgBuffer[1] != 0xD8) {
    free(jpgBuffer);
    return;
  }

  uint16_t jpgW = 0, jpgH = 0;
  JRESULT sizeResult = TJpgDec.getJpgSize(&jpgW, &jpgH, jpgBuffer, received);

  if (sizeResult == JDR_OK) {
    uint8_t scale = 1;
    if (jpgW >= 500) scale = 4;
    else if (jpgW >= 250) scale = 2;
    TJpgDec.setJpgScale(scale);

    int scaledW = jpgW / scale;
    int scaledH = jpgH / scale;
    int drawX = ART_X + (ART_SIZE - scaledW) / 2;
    int drawY = ART_Y + (ART_SIZE - scaledH) / 2;

    tft.fillRect(ART_X, ART_Y, ART_SIZE, ART_SIZE, TFT_BLACK);
    TJpgDec.drawJpg(drawX, drawY, jpgBuffer, received);
  }
  free(jpgBuffer);
}

// ==========================================
// FULLSCREEN ART MODE
// ==========================================
#define FS_ART_SIZE 150
#define FS_ART_X ((SCR_W - FS_ART_SIZE) / 2)
#define FS_ART_Y 20

void drawFullscreenScreen() {
  currentScreen = FULLSCREEN_ART_MODE;
  tft.fillScreen(COLOR_BG);

  int btnX = 290;
  int btnY = 25;
  tft.fillCircle(btnX, btnY, 15, COLOR_DARKGREY);

  int xOffset = 4;
  tft.drawLine(btnX - xOffset, btnY - xOffset, btnX + xOffset, btnY + xOffset, TFT_WHITE);
  tft.drawLine(btnX - xOffset + 1, btnY - xOffset, btnX + xOffset + 1, btnY + xOffset, TFT_WHITE);
  tft.drawLine(btnX - xOffset, btnY + xOffset, btnX + xOffset, btnY - xOffset, TFT_WHITE);
  tft.drawLine(btnX - xOffset + 1, btnY + xOffset, btnX + xOffset + 1, btnY - xOffset, TFT_WHITE);

  if (currentImgUrl.length() > 0) {
    drawCoverFullscreen(currentImgUrl.c_str());
  } else {
    tft.fillRoundRect(FS_ART_X, FS_ART_Y, FS_ART_SIZE, FS_ART_SIZE, 6, COLOR_DARKGREY);
  }
  tft.drawRoundRect(FS_ART_X - 2, FS_ART_Y - 2, FS_ART_SIZE + 4, FS_ART_SIZE + 4, 8, COLOR_DARKGREY);

  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_WHITE, COLOR_BG);
  String infoLine = currentTrackTitle + "  •  " + currentArtistName;
  infoLine = fitText(infoLine, SCR_W - 20);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(infoLine, SCR_W / 2, FS_ART_Y + FS_ART_SIZE + 18);

  tft.setTextFont(1);
  tft.setTextColor(COLOR_GREY, COLOR_BG);
  String albumLine = fitText(currentAlbumName, SCR_W - 20);
  tft.drawString(albumLine, SCR_W / 2, FS_ART_Y + FS_ART_SIZE + 36);
  tft.setTextDatum(TL_DATUM);
}

void drawCoverFullscreen(const char *url) {
  if (url == nullptr || strlen(url) == 0) return;

  WiFiClientSecure artClient;
  artClient.setInsecure();
  artClient.setTimeout(8000);

  HTTPClient https;
  https.setConnectTimeout(8000);
  https.setTimeout(10000);
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!https.begin(artClient, String(url))) return;
  int httpCode = https.GET();
  if (httpCode != HTTP_CODE_OK) {
    https.end();
    return;
  }

  int contentLength = https.getSize();
  size_t capacity = (contentLength > 0 && contentLength < MAX_IMAGE_SIZE) ? (size_t)contentLength : MAX_IMAGE_SIZE;
  uint8_t *jpgBuffer = (uint8_t *)malloc(capacity);
  if (jpgBuffer == nullptr) {
    https.end();
    return;
  }

  WiFiClient *stream = https.getStreamPtr();
  size_t received = 0;
  unsigned long start = millis();

  while (millis() - start < 10000 && received < capacity) {
    int availableBytes = stream->available();
    if (availableBytes > 0) {
      size_t want = min((size_t)availableBytes, capacity - received);
      int got = stream->readBytes((char *)jpgBuffer + received, want);
      if (got > 0) {
        received += (size_t)got;
        start = millis();
      }
    } else {
      if (!https.connected() && contentLength < 0) break;
      delay(2);
    }
  }
  https.end();

  if (received < 2 || jpgBuffer[0] != 0xFF || jpgBuffer[1] != 0xD8) {
    free(jpgBuffer);
    return;
  }

  uint16_t jpgW = 0, jpgH = 0;
  JRESULT sizeResult = TJpgDec.getJpgSize(&jpgW, &jpgH, jpgBuffer, received);

  if (sizeResult == JDR_OK) {
    uint8_t scale = 1;
    if (jpgW >= 500) scale = 4;
    else if (jpgW >= 250) scale = 2;
    TJpgDec.setJpgScale(scale);

    int scaledW = jpgW / scale;
    int scaledH = jpgH / scale;
    int drawX = FS_ART_X + (FS_ART_SIZE - scaledW) / 2;
    int drawY = FS_ART_Y + (FS_ART_SIZE - scaledH) / 2;

    tft.fillRect(FS_ART_X, FS_ART_Y, FS_ART_SIZE, FS_ART_SIZE, TFT_BLACK);
    TJpgDec.drawJpg(drawX, drawY, jpgBuffer, received);
  }
  free(jpgBuffer);
}
