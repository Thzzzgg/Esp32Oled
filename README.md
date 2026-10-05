# Esp32Oled

ideaspark ESP32 開發板（16MB）內建 1.9 吋 ST7789 彩色 TFT LCD（170×320）的專案。

> 注意：這是彩色 TFT LCD，不是 OLED。資料夾名稱沿用舊稱。

## 硬體

- 主控：ESP32-WROOM-32，USB Type-C，CH340 USB 轉串口
- 顯示：ST7789，SPI 介面

| 功能 | GPIO |
|---|---|
| MOSI | 23 |
| SCLK | 18 |
| CS | 15 |
| DC | 2 |
| RST | 4 |
| BLK（背光） | 32 |

以上接腳來自社群資料，**尚待實機驗證**，不是官方手冊。

## 開發環境

- VS Code + PlatformIO 外掛（`platformio.platformio-ide`）
- 設定見 `platformio.ini`，函式庫為 TFT_eSPI

## 編譯與燒錄

在 VS Code 開啟專案資料夾後，使用 PlatformIO 面板的 Build／Upload／Monitor；或用指令列：

```
pio run                 # 編譯
pio run -t upload       # 燒錄
pio device monitor      # 序列埠監看（115200）
```

`pio` 若不在 PATH，可使用 `C:\Users\<使用者>\.platformio\penv\Scripts\pio.exe`。

## 板子到手後的驗證步驟

1. USB 接上電腦，確認裝置管理員出現 CH340 序列埠。
2. 燒錄目前的測試程式，確認背光有亮、畫面依序顯示紅／綠／藍／白。
3. 若顏色不對（紅藍互換、顏色反相），調整 TFT_eSPI 的 `TFT_RGB_ORDER`、`TFT_INVERSION_ON` 或 `TFT_INVERSION_OFF`。
4. 若畫面邊緣被切掉或偏移，檢查尺寸與偏移設定。
5. 核對接腳與官方資料後，移除 `CLAUDE.md` 與本檔的「待驗證」說明。
