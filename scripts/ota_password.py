# 讀取 OTA 密碼，注入編譯與上傳設定；密碼不寫進 git。
# 讀取順序：環境變數 ESP32_OTA_PASSWORD → 專案根目錄的 ota_password.txt（只讀第一行）
# - 有密碼：韌體會啟用 OTA（定義巨集 OTA_PASSWORD），ota 環境上傳時自動帶上 --auth
# - 沒密碼：韌體不啟用 OTA；ota 環境會直接報錯，避免誤以為更新成功
Import("env")  # noqa: F821  (PlatformIO 提供)

import os


def read_password():
    password = os.environ.get("ESP32_OTA_PASSWORD", "").strip()
    if password:
        return password
    path = os.path.join(env.subst("$PROJECT_DIR"), "ota_password.txt")  # noqa: F821
    if os.path.isfile(path):
        with open(path, encoding="utf-8") as f:
            return f.readline().strip()
    return ""


password = read_password()
uses_ota = env.GetProjectOption("upload_protocol", "") == "espota"  # noqa: F821

if password:
    env.Append(CPPDEFINES=[("OTA_PASSWORD", env.StringifyMacro(password))])  # noqa: F821
    if uses_ota:
        env.Append(UPLOAD_FLAGS=["--auth=" + password])  # noqa: F821
elif uses_ota:
    print("錯誤：找不到 OTA 密碼。請設定環境變數 ESP32_OTA_PASSWORD，或在專案根目錄建立 ota_password.txt（第一行為密碼）。")
    env.Exit(1)  # noqa: F821
