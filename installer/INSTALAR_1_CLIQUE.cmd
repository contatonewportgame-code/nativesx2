@echo off
cd /d "%~dp0"
echo ==========================================================
echo   Configurando NativeSX2 (Modo Portatil - 100%% Seguro)
echo   Plataforma de Jogos PS2 Nativos para PC
echo ==========================================================
echo.
echo [1/3] Vinculando associacao de arquivos .ISO ao NativeSX2...
reg add "HKCU\Software\Classes\.iso" /ve /d "NativeSX2.iso" /f >nul 2>&1
reg add "HKCU\Software\Classes\.iso\OpenWithProgids" /v "NativeSX2.iso" /t REG_NONE /f >nul 2>&1
reg add "HKCU\Software\Classes\NativeSX2.iso" /ve /d "Jogo de PS2 - NativeSX2 (by NewPortGame)" /f >nul 2>&1
reg add "HKCU\Software\Classes\NativeSX2.iso\DefaultIcon" /ve /d "\"%~dp0nativesx2_iso.ico\"" /f >nul 2>&1
reg add "HKCU\Software\Classes\NativeSX2.iso\shell\open\command" /ve /d "\"%~dp0NativeSX2.exe\" \"%%1\"" /f >nul 2>&1
reg add "HKCU\Software\Classes\Applications\NativeSX2.exe" /v "FriendlyAppName" /d "NativeSX2 (by NewPortGame)" /f >nul 2>&1
reg add "HKCU\Software\Classes\Applications\NativeSX2.exe\shell\open\command" /ve /d "\"%~dp0NativeSX2.exe\" \"%%1\"" /f >nul 2>&1

echo [2/3] Configurando placa grafica para Alto Desempenho...
reg add "HKCU\Software\Microsoft\DirectX\UserGpuPreferences" /v "%~dp0ps2EntryRunner.exe" /d "GpuPreference=2;" /f >nul 2>&1
reg add "HKCU\Software\Microsoft\DirectX\UserGpuPreferences" /v "%~dp0NativeSX2.exe" /d "GpuPreference=2;" /f >nul 2>&1

echo [3/3] Criando atalho oficial na Area de Trabalho...
powershell -NoProfile -Command "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut([Environment]::GetFolderPath('Desktop') + '\NativeSX2.lnk'); $s.TargetPath = '%~dp0NativeSX2.exe'; $s.WorkingDirectory = '%~dp0'; $s.IconLocation = '%~dp0nativesx2.ico'; $s.Save()" >nul 2>&1

echo.
echo ==========================================================
echo  TUDO PRONTO!
echo  - O atalho NativeSX2 foi criado na sua Area de Trabalho;
echo  - Arquivos .ISO agora mostram o icone personalizado de PS2;
echo  - Basta dar 2 cliques em qualquer .ISO para jogar direto!
echo ==========================================================
echo.
pause
