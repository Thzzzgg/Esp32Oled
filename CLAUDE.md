# CLAUDE.md

本檔為 `Esp32Oled` 專屬的專案指引。

## 語言

- 一律使用**繁體中文**與使用者對話。
- 程式碼註解、commit 訊息、文件也使用繁體中文；變數、函式、檔案名稱維持英文。

## 版本與 Git 規則

- 版本號格式：`V主版本.次版本.修訂版本`，**從 `V0.0.1` 起跳**（V 為大寫）。
  - 既有的 `V0.0.0` tag 是舊規則留下的，保留不動，不重編歷史。
  - 主版本：不相容的大改動（例如換開發板、換顯示函式庫）
  - 次版本：新增功能
  - 修訂版本：修 bug、小調整
- 每個版本打 git tag，名稱與版本號相同，例如 `V0.0.0`、`V0.1.0`。
- 專案已 `git init`，目前在 `main` 分支，已有 `V0.0.0` tag。要 commit、打 tag 或 push 前，先經使用者同意。
- Commit 訊息用繁體中文，第一行簡短描述改了什麼；有對應版本時寫在前面，例如 `V0.1.0 新增 WiFi 連線畫面`。

## 硬體（尚未購買，以下資料待實機驗證）

- 預計使用：ideaspark ESP32 開發板 16MB，內建 1.9 吋 **ST7789 TFT LCD**，解析度 170×320。
- 目標商品：Amazon.co.jp [B0GZTFV72M](https://www.amazon.co.jp/dp/B0GZTFV72M)（「ESP32 開発ボード 16MB 1.9インチ ST7789 170x320 TFT LCDディスプレイ搭載 WiFi+BLE CH340 USB Type-C」，品牌 ideaspark）。
- 注意：這是**彩色 TFT LCD，不是 OLED**。資料夾名稱 `Esp32Oled` 是沿用舊稱，不要改用 SSD1306 / SH1106 / U8g2 的寫法。
- 主控：ESP32-WROOM-32，USB Type-C，CH340 USB 轉串口。
- 顯示介面：SPI。社群資料的接腳如下（來源為 Amazon.com 同系列產品 B0D6QXC813 的使用者資料，**不是官方手冊**）：

  | 功能 | GPIO |
  |---|---|
  | MOSI | 23 |
  | SCLK | 18 |
  | CS | 15 |
  | DC | 2 |
  | RST | 4 |
  | BLK（背光） | 32 |

  - 第二個來源：Arduino 論壇 [Ideaspark st7789 ?](https://forum.arduino.cc/t/ideaspark-st7789/1310101) 有使用者貼出「廠商提供」的同一組接腳，與上表一致。仍是社群轉述，不是官方手冊，所以保持「待驗證」。
  - 第三個來源：上述目標商品 B0GZTFV72M 的賣家商品頁（2026-10-06 讀取），特色與商品說明都列出同一組接腳（MOSI 23、SCLK 18、CS 15、DC 2、RST 4、BLK 32），與上表一致。這是賣家的商品頁，仍不是官方資料手冊，所以保持「待驗證」。
  - 商品頁的其他規格：ESP32 雙核 240MHz、Flash 16MB、SRAM 520KB、Wi-Fi 802.11 b/g/n、BLE 4.2、3.3V；螢幕為 1.9 吋 IPS，顯示區 22.695×42.72 mm，背光為 3 顆白光 LED 並聯，30 針 FPC。
  - 商品頁寫螢幕驅動 IC 為 **ST7789V3**（專案目前用 TFT_eSPI 的 `ST7789_DRIVER`）。V3 的初始化參數可能與一般 ST7789 略有差異，實機若顏色、亮度或畫面不對，先往這個方向查。
  - 商品頁第一條特色寫螢幕支援「I2C」，與同頁的 SPI 接腳表矛盾，視為賣家筆誤，仍以 SPI 為準。
  - 商品頁沒有提到 GPIO4 微動開關、色彩反相、列偏移 35，這幾項仍需實機確認。

- 已知注意事項：
  - 背光可能需要 `pinMode(32, INPUT_PULLUP); digitalWrite(32, HIGH);` 才會亮。
  - TFT_eSPI 的寬高設 170×320，不是 240。ST7789 列偏移為 35（0x23）。
  - GPIO2、GPIO15 是 ESP32 的 strapping 腳，不要外接會拉高或拉低的電路。
- 實機到手後，先核對接腳與官方資料，確認無誤再移除本節的「待驗證」說明。

## 開發環境

- 使用 **PlatformIO**（VS Code 外掛 `platformio.platformio-ide` v3.3.4，PlatformIO Core 6.2.0）。不用 Arduino IDE。
- 設定集中在 `platformio.ini`：
  - platform：`espressif32@7.1.3`，board：`esp32dev`，framework：Arduino
  - 函式庫：`bodmer/TFT_eSPI@^2.5.43`（實際解析為 2.5.43）
  - TFT_eSPI 的驅動、尺寸、接腳全部寫在 `build_flags`，**不要改函式庫內的 `User_Setup.h`**。
  - TFT_eSPI 在寬 170、高 320 時會自動啟用 `CGRAM_OFFSET`，列偏移 35 由函式庫處理，不必另外設定。
  - 背光（GPIO32）不交給 TFT_eSPI（不定義 `TFT_BL`），由 `src/main.cpp` 自行控制。
  - 目前沿用 `esp32dev` 預設的 4MB 分區表；板子實測確認 16MB 後，再考慮改用 16MB 分區表。
- 編譯：`C:\Users\<使用者>\.platformio\penv\Scripts\pio.exe run`（`pio` 不在 PATH，需用完整路徑，或從 VS Code 的 PlatformIO 面板執行）。
- 燒錄：`pio run -t upload`；序列埠監看：`pio device monitor`（115200）。
- CH340 驅動：本機已安裝（`ch341ser.inf`，wch.cn，2014-08-08 版）。
- `src/main.cpp` 目前是最小測試程式（背光、紅綠藍白填色與文字），用來在實機上核對接腳、顏色與偏移。
- 已知風險（來自社群資料，待實機驗證）：
  - 有人回報 ST7789 170×320 需要色彩反相；TFT_eSPI 初始化已送出 `INVON`，顏色不對時再調整。
  - Arduino 論壇有使用者從 Adafruit ST7789 改用 TFT_eSPI 時出現畫面被切掉的情況，實機若遇到先檢查尺寸與偏移設定。
  - Tasmota 討論串提到板背面有一顆微動開關接在 GPIO4，而 RST 也是 GPIO4，需實機確認是否為同一條線。
