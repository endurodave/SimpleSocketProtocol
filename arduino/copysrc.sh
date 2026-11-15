#!/bin/sh
# Copy the SSP sources this sketch needs into the arduino folder. The Arduino
# IDE only builds files inside the sketch folder, so run this once before
# opening arduino.ino (and again after updating the repository).
set -e
cd "$(dirname "$0")"
cp ../ssp/*.c ../ssp/*.cpp ../ssp/*.h .
cp ../port/ssp_fault.c ../port/hal/arduino/ssp_hal_arduino.cpp \
   ../port/hal/no_os/ssp_hal_mem_buf.c ../port/osal/no_os/ssp_osal_no_os.c .
echo "Copied SSP sources into $(pwd)"
