@echo off
cd /d "%~dp0"
echo ==========================================================
echo   CRIANDO INSTALADOR OFICIAL DE FACIL INSTALACAO
echo   God of War PC Port Nativo (NativeSX2)
echo ==========================================================
echo.

set "ISCC=C:\Users\MARIANO\AppData\Local\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    if exist "C:\Program Files\Inno Setup 6\ISCC.exe" set "ISCC=C:\Program Files\Inno Setup 6\ISCC.exe"
)

if not exist "%ISCC%" (
    echo [ERRO] Compilador Inno Setup nao localizado!
    pause
    exit /b 1
)

if not exist "%~dp0PACOTE_PORTATIL_NATIVESX2\ps2EntryRunner.exe" (
    echo [ERRO] ps2EntryRunner.exe nao encontrado na pasta PACOTE_PORTATIL_NATIVESX2!
    pause
    exit /b 1
)

echo Compilando instalador para o disco E:\INSTALADOR_GOW ...
echo.
"%ISCC%" "%~dp0inno_setup_script.iss"
if errorlevel 1 (
    echo.
    echo [ERRO] Ocorreu uma falha ao gerar o instalador.
    pause
    exit /b 1
)

echo.
echo ==========================================================
echo  INSTALADOR GERADO COM SUCESSO!
echo  Localizado em: E:\INSTALADOR_GOW\Instalador_God_of_War_PC.exe
echo ==========================================================
echo.
pause
