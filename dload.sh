#!/bin/sh
#
# Script for loading a bootloader into a modem in standard mode
#
# Script parameter - tty port name, by default - /dev/ttyUSB0
#
PORT=$1
if [ -z "$PORT" ]; then PORT=/dev/ttyUSB0; fi

LDR=$2
if [ -z "$LDR" ]; then LDR=loaders/NPRG9x15p.bin; fi


echo Diagnostic port: $PORT

# Waiting for the port to appear on the system
while [ ! -c $PORT ]
 do
  sleep 1
 done

# download mode command
echo entering download mode...
./qcommand -p $PORT -e -c "c 3a" >/dev/null

# Waiting for the port to disappear from the system
while [ -c $PORT ]
 do
  true
 done
echo diagnostic port removed

# Waiting for the new port to arrive, already downloading
while [ ! -c $PORT ]
 do
  sleep 1
 done

# Uploading the bootloader
echo download mode entered
sleep 2
./qdload -t -p $PORT -i  $LDR
