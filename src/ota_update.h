// 無線更新（OTA）：連上 Wi-Fi 後，可以用 PlatformIO 的 ota 環境直接更新韌體，不用接 USB
#pragma once

namespace OtaUpdate {

// 編譯時有提供 OTA 密碼才會啟用；沒有密碼就不開放無線更新，避免區網內任何人都能刷韌體
bool enabled();

// 連上 Wi-Fi 後呼叫一次；重複呼叫無效
void begin();

// 放在 loop() 內；收到更新時會一路卡在這裡直到更新結束
void handle();

}  // namespace OtaUpdate
