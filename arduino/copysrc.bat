@echo off
REM Copy the SSP sources this sketch needs into the arduino folder. The Arduino
REM IDE only builds files inside the sketch folder, so run this once before
REM opening arduino.ino (and again after updating the repository).

cd /d "%~dp0"
copy /y ..\ssp\*.c . >nul
copy /y ..\ssp\*.cpp . >nul
copy /y ..\ssp\*.h . >nul
copy /y ..\port\ssp_fault.c . >nul
copy /y ..\port\hal\arduino\ssp_hal_arduino.cpp . >nul
copy /y ..\port\hal\no_os\ssp_hal_mem_buf.c . >nul
copy /y ..\port\osal\no_os\ssp_osal_no_os.c . >nul
echo Copied SSP sources into %CD%
