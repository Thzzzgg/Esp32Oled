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
- 設定見 `platformio.ini`，函式庫為 TFT_eSPI 與 QRCode

## 功能

- **Wi-Fi 配網**：第一次開機（或連線逾時）會開熱點，螢幕顯示 QR code；手機掃描加入後，在網頁選擇 Wi-Fi 並輸入密碼。帳密存在 ESP32 內，不會進 git。
- **連線狀態頁**：顯示 SSID、IP、訊號強度（dBm 與格數）、閘道、運行時間；斷線時顯示紅色並自動重連。
- 長按 BOOT 鍵 3 秒：清除已儲存的 Wi-Fi，重新開機進入配網。
- 只支援 2.4GHz Wi-Fi（ESP32 不支援 5GHz）。
- 螢幕內建字型只含英文與數字，中文 SSID 會顯示成亂碼。

## 編譯與燒錄

在 VS Code 開啟專案資料夾後，使用 PlatformIO 面板的 Build／Upload／Monitor；或用指令列：

```
pio run                 # 編譯主程式
pio run -t upload       # 燒錄主程式
pio device monitor      # 序列埠監看（115200）
pio run -e hwtest -t upload   # 燒錄硬體測試程式（只填色，不含 Wi-Fi）
```

`pio` 若不在 PATH，可使用 `C:\Users\<使用者>\.platformio\penv\Scripts\pio.exe`。

## 板子到手後的驗證步驟

1. USB 接上電腦，確認裝置管理員出現 CH340 序列埠。
2. 先燒錄硬體測試程式（`pio run -e hwtest -t upload`），確認背光有亮、畫面依序顯示紅／綠／藍／白。這一步通過，再燒主程式，問題才好判斷是接線還是程式。
3. 若顏色不對（紅藍互換、顏色反相），調整 TFT_eSPI 的 `TFT_RGB_ORDER`、`TFT_INVERSION_ON` 或 `TFT_INVERSION_OFF`。
4. 若畫面邊緣被切掉或偏移，檢查尺寸與偏移設定。
5. 核對接腳與官方資料後，移除 `CLAUDE.md` 與本檔的「待驗證」說明。
