# spotifyesp32

A desktop companion built for ESP32 microcontrollers. When your desk is quiet, it functions as a minimalist, clock and weather dashboard. The second you hit play on Spotify, it instantly transforms into an interactive live music visualiser with album art, marquee titles, and song progress tracking.


<img width="320" height="240" alt="Clock screen" src="https://github.com/user-attachments/assets/f1ece82e-c72a-43c1-ac5e-0219d406e920" />
<img width="320" height="240" alt="Visualiser screen" src="https://github.com/user-attachments/assets/36cbfe6f-e715-4857-8943-e96b4f9ffc43" />

---

## ✨ Features

* **🕰️ Clock Dashboard:** A clean, modern idle screen featuring a massive digital time display, localized weather icons, current temperature, and a sleek centered date readout.
* **🎵 Real-Time Spotify Visualizer:** 
  * High-performance album art rendering via JPEG decoding (`TJpgDec`).
  * Smooth marquee scrolling for long song titles.
  * Live track progress bar and elapsed/remaining timers.
  * Pause state badges and touch-controlled immersive **Fullscreen Art Mode**.
* **⚡ Flicker-Free Optimization:** The clock screen intelligently updates only when the minute changes, keeping the display static and crisp when idle.
* **🌐 Flexible Connectivity:** Built-in `WiFiManager` portal fallback for easy network configuration alongside multi-AP support.

---

## 🛠️ Hardware Flexibility

While originally optimized for my CYD (ESP32-2432S028) (320x240 TFT LCD with XPT2046 touch), `spotifyesp32` can be ported to other hardware layouts:
* **ESP32 CYD** (320x240 TFT LCD + Touch)
* **LilyGO T-Display-S3** (High-brightness IPS displays)
* Other ESP32 boards paired with compatible SPI/I2C displays and touch panels.

---

## 💻 Development Frameworks

You aren't strictly limited to the Arduino IDE, but it's what I used for this project. This project can be built using:
* **Arduino IDE** (Standard setup with Library Manager)
* **PlatformIO (VS Code):** Recommended for better code organization, autocomplete, and automated dependency management via `platformio.ini`.

---

## 📦 Required Libraries

Make sure you have the following installed: (if using the Arduino IDE, via the **Library Manager**):

* [`TFT_eSPI`](https://github.com/Bodmer/TFT_eSPI) **(Configure your `User_Setup.h` for your specific display)**
* [`SpotifyArduino`](https://github.com/witnessmenow/Spotify-Arduino-Client)
* [`ArduinoJson`](https://arduinojson.org/) (v6 or v7)
* [`TJpg_Decoder`](https://github.com/Bodmer/TJpg_Decoder)
* [`XPT2046_Touchscreen`](https://github.com/PaulStoffregen/XPT2046_Touchscreen)
* [`WiFiManager`](https://github.com/tzapu/WiFiManager)
* [`NTPClient`](https://github.com/taranais/NTPClient)

---

## ⚙️ Configuring `TFT_eSPI` (`User_Setup.h`)

Before uploading, you must configure the `TFT_eSPI` library to match your display hardware.

1. Navigate to your Arduino libraries folder (e.g., `Documents/Arduino/libraries/TFT_eSPI/`).
2. Open `User_Setup.h` in any text editor.
3. Make sure your driver and pin definitions match your CYD board specifications:
   * **Driver:** `#define ILI9341_DRIVER` (or `#define ST7789_DRIVER` for newer USB-C models).
   * **Dimensions:** `#define TFT_WIDTH 240` and `#define TFT_HEIGHT 320`.
   * **SPI Pinout:**
     ```cpp
     #define TFT_MISO 12
     #define TFT_MOSI 13
     #define TFT_SCLK 14
     #define TFT_CS   15
     #define TFT_DC    2
     #define TFT_RST  -1
     ```

---

## 🚀 Getting Started

1. **Clone the Repository:**
   ```bash
   git clone [https://github.com/itssmnle/spotifyesp32.git](https://github.com/itssmnle/spotifyesp32.git)

2. Copy the sketch from `sams-visualiser-ver-1.6.ino`. Paste it in your IDE, and configure your Spotify, network credentials and your OpenWeather API key as well as your location!
      * const char* openWeatherApiKey = "INSERT HERE";
      * char clientId[] = "CLIENT-ID";
      * char clientSecret[] = "CLIENT-SECRET";
      * char refreshToken[] = "REFRESH-TOKEN";
  
      * wifiMulti.addAP("SSID", "PASSWD"); - There will be 2, first will be your primary.

4. Configure the board and port, then upload the sketch and you should be done!

---

I've also added an enclosure design I made for the 2.8" model of the CYD (2432S028) if any of you would like it.
M2 screws to hold them together, and I attached both case faces for USB-C and microUSB.


<img width="1227" height="668" alt="Fusion 360 face" src="https://github.com/user-attachments/assets/c1f38fc3-7e06-4c4b-94ef-e9d131b82a57" />
<img width="1024" height="768" alt="The case IRL!" src="https://github.com/user-attachments/assets/fc9a5ee0-a58b-467a-b807-c4b6156f3441" />
