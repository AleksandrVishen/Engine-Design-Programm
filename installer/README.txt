Установщик Windows (Inno Setup 6)
==================================

Требования
----------
- Inno Setup 6.x (компилятор ISCC.exe в PATH или стандартный путь установки)
- Сборка Release x64 с wxWidgets (DLL рядом с exe, типичный случай vcpkg/MSVC)

Шаги сборки установщика
-----------------------
1) Конфигурация и сборка Release (из корня репозитория):

   cmake -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release

2) Подготовка каталога build\install (exe + все dll из build\Release):

   powershell -NoProfile -ExecutionPolicy Bypass -File installer\stage_release_install.ps1

3) Компиляция Inno Setup:

   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss

Готовый файл: build\installer\EngineDesign_Setup_1.0.0.exe (версия см. #define в installer.iss)

Через CMake (если найден ISCC и PowerShell)
-------------------------------------------
   cmake --build build --config Release --target installer_bundle

Примечания
----------
- Установка в Program Files (x64), нужны права администратора.
- Регистрируются расширения .edp и .eds (открытие двойным щелчком).
- Иконка приложения: assets\icon\EngineDesign.ico
