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
- **即時圖表**（每秒取樣，保留約 4 分 40 秒）：
  - 網路頁：Wi-Fi 訊號強度（RSSI）、到閘道的 ping 延遲（封包遺失畫成紅線）。
  - 系統頁：剩餘記憶體、晶片內部溫度（未校準，只看趨勢）。
- 頁面每 10 秒自動輪播，螢幕左邊的圓點表示目前頁；短按 BOOT 鍵切到下一頁並停止輪播。
- 長按 BOOT 鍵 3 秒：清除已儲存的 Wi-Fi，重新開機進入配網。
- **OTA 無線更新**：連上 Wi-Fi 後可以不接 USB 直接更新韌體，更新時螢幕顯示進度條。需要設定密碼，見下方「OTA 無線更新」。
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

## OTA 無線更新

沒有設定密碼時，OTA 不會啟用（狀態頁底部顯示 `OTA off`），避免區網內任何人都能刷韌體。

1. **設定密碼**（密碼不進 git，擇一）：
   - 在專案根目錄建立 `ota_password.txt`，第一行寫密碼（已列入 `.gitignore`）；
   - 或設定環境變數 `ESP32_OTA_PASSWORD`（Windows：`setx ESP32_OTA_PASSWORD "你的密碼"`，之後要重開 VS Code 或終端機）。
   - 建議只用英文與數字，避免特殊字元在命令列被吃掉。
2. **第一次用 USB 燒錄**：`pio run -t upload`。此時韌體已含 OTA，連上 Wi-Fi 後狀態頁會顯示 `OTA ready`。
3. **之後改用無線更新**：`pio run -e ota -t upload`。主機名稱是 `esp32-lcd.local`；若電腦解析不到，改用 IP：`pio run -e ota -t upload --upload-port 192.168.x.x`（IP 看螢幕狀態頁）。

注意：

- 電腦與板子要在同一個區網。上傳時板子會連回電腦的隨機埠，Windows 防火牆若跳出提示請允許 Python。
- 目前使用預設分區表，單一韌體上限約 1.25MB（現在約 0.85MB）。快超過時要改用 16MB 分區表，這要等實機確認 Flash 容量後再做。
- 更新失敗（密碼錯、網路中斷）時螢幕會顯示原因，板子繼續跑舊韌體。

## Wokwi 模擬

在沒有實機時，可以用 [Wokwi](https://wokwi.com/) 模擬器先看版面與已連線後的流程。用的是 VS Code 外掛版（支援 PlatformIO），不是線上瀏覽器版。

1. 在 VS Code 安裝外掛「Wokwi Simulator」。
2. 按 `F1` →「Wokwi: Request a new License」，在瀏覽器登入免費帳號並確認。
3. 編譯模擬用韌體：`pio run -e wokwi`（或在 VS Code 底部切換到 `wokwi` 環境再按編譯）。
4. 按 `F1` →「Wokwi: Start Simulator」。

`diagram.json` 是模擬電路（接腳與實機相同，另外多一顆接 GPIO0 的綠色按鈕當 BOOT 鍵），`wokwi.toml` 指向編譯出的韌體。

模擬環境與實機的差異：

- Wokwi 沒有 ST7789，改用它的 ILI9341（240×320）。版面仍以 320×170 為準，下方留黑。
- 直接連虛擬網路 `Wokwi-GUEST`，跳過配網與 OTA（兩者在模擬中都測不了）。
- 模擬器不支援 ICMP，所以 ping 圖會一直顯示封包遺失（紅線）。
- RSSI 與晶片溫度不會有真實變化。
- **驗證不了**：ST7789V3 初始化、顏色反相、偏移 35、背光、BOOT 鍵實際接腳。這些仍要等實機。

## 板子到手後的驗證步驟

1. USB 接上電腦，確認裝置管理員出現 CH340 序列埠。
2. 先燒錄硬體測試程式（`pio run -e hwtest -t upload`），確認背光有亮、畫面依序顯示紅／綠／藍／白。這一步通過，再燒主程式，問題才好判斷是接線還是程式。
3. 若顏色不對（紅藍互換、顏色反相），調整 TFT_eSPI 的 `TFT_RGB_ORDER`、`TFT_INVERSION_ON` 或 `TFT_INVERSION_OFF`。
4. 若畫面邊緣被切掉或偏移，檢查尺寸與偏移設定。
5. 核對接腳與官方資料後，移除 `CLAUDE.md` 與本檔的「待驗證」說明。
