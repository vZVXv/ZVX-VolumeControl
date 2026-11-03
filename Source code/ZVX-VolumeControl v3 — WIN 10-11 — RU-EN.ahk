#Requires AutoHotkey v2.0
#SingleInstance Force

; === Работа с конфигурацией (сохранение клавиши и языка) ===
global IniFile := A_ScriptDir "\settings.ini"
global HotkeyName := IniRead(IniFile, "Settings", "Hotkey", "AppsKey")
global CurrentLang := IniRead(IniFile, "Settings", "Language", "ru") ; ru / en

; === Словари локализации через Map ===
global Lang := Map(
    "ru", Map(
        "traySettings", "Настройки",
        "trayRestart", "Перезапустить скрипт",
        "trayExit", "Выход",
        "settingsTitle", "Настройки ZVX Volume Control",
        "tabGeneral", "Основные",
        "tabAbout", "Об авторе",
        "lblKey", "Горячая клавиша для регулировки громкости:",
        "lblHint", "Подсказка: Можно использовать модификаторы (например, ^F1 или !a).",
        "lblLang", "Язык интерфейса / Language:",
        "btnSave", "Сохранить",
        "errKey", "Не удалось назначить клавишу: ",
        "errKeyTitle", "Ошибка",
        "errSyntax", "Неверный синтаксис клавиши: ",
        "successMsg", "Горячая клавиша успешно изменена на: ",
        "successTitle", "Готово",
        "aboutText", "ZVX Volume Control.`n`nСоздатель: Евгений Губарев (ZVX)`nВерсия: 3.0"
    ),
    "en", Map(
        "traySettings", "Settings",
        "trayRestart", "Restart Script",
        "trayExit", "Exit",
        "settingsTitle", "ZVX Volume Control Settings",
        "tabGeneral", "General",
        "tabAbout", "About",
        "lblKey", "Volume Control Hotkey:",
        "lblHint", "Tip: You can use modifiers (e.g., ^F1 or !a).",
        "lblLang", "Interface Language:",
        "btnSave", "Save",
        "errKey", "Failed to assign hotkey: ",
        "errKeyTitle", "Error",
        "errSyntax", "Invalid hotkey syntax: ",
        "successMsg", "Hotkey successfully changed to: ",
        "successTitle", "Success",
        "aboutText", "ZVX Volume Control.`n`nCreator: Evgeny Gubarev (ZVX)`nVersion: 3.0"
    )
)

; === Глобальные переменные для хранения текущего оформления и состояния ===
global AccentColor := "0078D4"
global BgColor     := "202020"
global BarBgColor  := "3A3A3A"
global TextColor   := "FFFFFF"
global lastVal     := -1

; === Настройка трей-меню ===
UpdateTrayMenu()
A_TrayMenu.ClickCount := 1

UpdateThemeColors()

; === Создание UI громкости ===
MyGui := Gui("+AlwaysOnTop +ToolWindow -Caption +E0x08000000")
MyGui.BackColor := BgColor

MyGui.SetFont("s11 Bold c" TextColor, "Segoe UI Variable Text")
VolText := MyGui.AddText("x15 y10 w220 h25", Chr(0x1F50A) " 0%")
VolBar := MyGui.AddProgress("x18 y40 w214 h4 c" AccentColor " Background" BarBgColor " Range0-100", 0)

; === Отслеживание системного сообщения смены настроек (WM_SETTINGCHANGE) ===
OnMessage(0x001A, OnSettingChange)

OnSettingChange(wParam, lParam, msg, hwnd) {
    UpdateThemeColors()
    MyGui.BackColor := BgColor
    VolText.SetFont("c" TextColor)
    VolBar.Opt("c" AccentColor " Background" BarBgColor)
}

UpdateThemeColors() {
    global AccentColor, BgColor, BarBgColor, TextColor
    isDark := True
    try {
        isDark := !RegRead("HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize", "AppsUseLightTheme")
    }
    try {
        colorInt := RegRead("HKEY_CURRENT_USER\Software\Microsoft\Windows\DWM", "ColorizationColor")
        AccentColor := Format("{:06X}", colorInt & 0xFFFFFF)
    } catch {
        AccentColor := "0078D4"
    }
    if isDark {
        BgColor     := "202020"
        BarBgColor  := "3A3A3A"
        TextColor   := "FFFFFF"
    } else {
        BgColor     := "F3F3F3"
        BarBgColor  := "D0D0D0"
        TextColor   := "1E1E1E"
    }
}

UpdateTrayMenu() {
    global CurrentLang
    L := Lang[CurrentLang]
    A_TrayMenu.Delete()
    A_TrayMenu.Add(L["traySettings"], ShowSettings)
    A_TrayMenu.Add(L["trayRestart"], (*) => Reload())
    A_TrayMenu.Add(L["trayExit"], (*) => ExitApp())
    A_TrayMenu.Default := L["traySettings"]
}

; === Динамическая регистрация горячей клавиши ===
RegisterHotkey(HotkeyName)

RegisterHotkey(key) {
    global CurrentLang
    L := Lang[CurrentLang]
    try {
        Hotkey(key, VolumeControl, "On")
    } catch {
        MsgBox(L["errKey"] key ". Возвращаем AppsKey по умолчанию.", L["errKeyTitle"], "Icon!")
        Global HotkeyName := "AppsKey"
        Hotkey("AppsKey", VolumeControl, "On")
    }
}

; === Логика регулировки громкости ===
VolumeControl(ThisHotkey) {
    global lastVal
    MouseGetPos(&startX, &startY)
    startVol := SoundGetVolume()
    
    lastVal := -1 
    UpdateUI(startVol)
    
    MyGui.Show("xCenter y40 w250 h56 NoActivate")
    ApplyWindowStyle(MyGui.Hwnd)
    
    keyToCheck := RegExReplace(ThisHotkey, "^[~*!^+#<>]*")
    
    While GetKeyState(keyToCheck, "P")
    {
        MouseGetPos(&currentX, &currentY)
        deltaX := currentX - startX
        newVol := startVol + (deltaX / 12)
        
        if (newVol > 100)
            newVol := 100
        if (newVol < 0)
            newVol := 0
        
        SoundSetVolume(newVol)
        UpdateUI(newVol)
        Sleep(15)
    }
    MyGui.Hide()
}

UpdateUI(val) {
    global lastVal
    roundedVal := Round(val)
    VolBar.Value := roundedVal

    if (roundedVal != lastVal) {
        lastVal := roundedVal
        if (roundedVal == 0)
            icon := Chr(0x1F507) 
        else if (roundedVal < 35)
            icon := Chr(0x1F508) 
        else if (roundedVal < 70)
            icon := Chr(0x1F509) 
        else
            icon := Chr(0x1F50A) 
            
        VolText.Value := icon "  " roundedVal "%"
    }
}

; === Безопасное применение стилей окна с проверкой версии Windows ===
ApplyWindowStyle(hwnd) {
    if (VerCompare(A_OSVersion, ">=10.0.22000") >= 0) {
        static DWMWA_WINDOW_CORNER_PREFERENCE := 33
        static DWMWCP_ROUND := 2
        DllCall("dwmapi\DwmSetWindowAttribute", "Ptr", hwnd, "Int", DWMWA_WINDOW_CORNER_PREFERENCE, "Int*", DWMWCP_ROUND, "Int", 4)
    }
}

; === Унифицированное окно настроек с вкладками ===
ShowSettings(*) {
    global HotkeyName, CurrentLang
    L := Lang[CurrentLang]
    
    SetGui := Gui("+AlwaysOnTop", L["settingsTitle"])
    SetGui.SetFont("s10", "Segoe UI")
    
    Tabs := SetGui.Add("Tab3", "x10 y10 w360 h215", [L["tabGeneral"], L["tabAbout"]])
    
    ; --- Вкладка 1: Основные настройки ---
    Tabs.UseTab(1)
    SetGui.Add("Text", "x25 y50 w330", L["lblKey"])
    
    keysList := ["AppsKey", "RWin", "LWin", "F1", "F2", "F3", "XButton1", "XButton2", "CapsLock"]
    CB := SetGui.Add("ComboBox", "x25 y75 w330", keysList)
    CB.Text := HotkeyName
    
    SetGui.Add("Text", "x25 y105 w330 cGray", L["lblHint"])
    
    SetGui.Add("Text", "x25 y145 w140", L["lblLang"])
    LangDD := SetGui.Add("DropDownList", "x175 y141 w180", ["Русский", "English"])
    LangDD.Choose(CurrentLang == "ru" ? 1 : 2)
    
    ; --- Вкладка 2: Об авторе ---
    Tabs.UseTab(2)
    SetGui.Add("Text", "x25 y55 w330 h130", L["aboutText"])
    
    Tabs.UseTab(0)
    
    SaveBtn := SetGui.Add("Button", "Default x255 y235 w115 h30", L["btnSave"])
    
    ; Мгновенное переключение языка при выборе в списке
    LangDD.OnEvent("Change", (ctrl, info) => SwitchLanguage(ctrl.Text, SetGui))
    
    SaveBtn.OnEvent("Click", (*) => SaveSettings(CB.Text, LangDD.Text, SetGui))
    
    SetGui.Show("w380 h280")
}

SwitchLanguage(selectedText, guiObj) {
    global CurrentLang
    CurrentLang := (selectedText == "English") ? "en" : "ru"
    IniWrite(CurrentLang, IniFile, "Settings", "Language")
    UpdateTrayMenu()
    guiObj.Destroy()
    ShowSettings()
}

SaveSettings(newKey, selectedLangText, guiObj) {
    global HotkeyName, CurrentLang
    newKey := Trim(newKey)
    if (newKey == "")
        return
        
    L := Lang[CurrentLang]
    
    try {
        Hotkey(newKey, VolumeControl, "Off")
    } catch {
        MsgBox(L["errSyntax"] . newKey, L["errKeyTitle"], "Icon!")
        return
    }
        
    try {
        Hotkey(HotkeyName, "Off")
    }
    
    HotkeyName := newKey
    CurrentLang := (selectedLangText == "English") ? "en" : "ru"
    
    IniWrite(HotkeyName, IniFile, "Settings", "Hotkey")
    IniWrite(CurrentLang, IniFile, "Settings", "Language")
    
    RegisterHotkey(HotkeyName)
    UpdateTrayMenu()
    
    guiObj.Destroy()
    MsgBox(Lang[CurrentLang]["successMsg"] . HotkeyName, Lang[CurrentLang]["successTitle"], "Iconi T1.5")
}