#!/bin/sh
#
# Script for flashing a bootloader into a modem in standard mode
#
# Script parameter - tty port name, default is /dev/ttyUSB0
#
PORT=$1
if [ -z "$PORT" ]; then PORT=/dev/ttyUSB0; fi

LDR=$2
if [ -z "$LDR" ]; then LDR=loaders/NPRG9x15p.bin; fi

echo Diagnostic port: $PORT

# Wait for the port to appear in the system
while [ ! -c $PORT ]
 do
  sleep 1
 done

# Command to switch to download mode
echo entering download mode...
./qcommand -p $PORT -e -c "c 3a" >/dev/null

# Wait for the port to disappear from the system
while [ -c $PORT ]
 do
  true
 done
echo diagnostic port removed

# Wait for the appearance of a new port, now in download mode
while [ ! -c $PORT ]
 do
  sleep 1
 done

# Flash the bootloader
echo download mode entered
sleep 2
./qdload -t -p $PORT -i  $LDR
