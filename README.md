# qtools
A set of tools for working with flash on Qualcom chipset based modems. The set consists of a package of utilities and a set of patched bootloaders.

qcommand - an interactive terminal for entering commands through the command port. It replaces the terribly inconvenient revskills. Allows you to enter command packets byte-by-byte, edit memory, read and view any flash sector.

qrmem - a program for reading a modem address space dump.

qrflas - a flash reading program. Can read both a range of blocks and partitions from a partition map.

qwflash - a program for writing partition images through the user partitions mode of the bootloader, similar to QPST.

qwdirect - a program for direct writing of flash drive blocks with/without OOB through controller ports (without the participation of bootloader logic).

qdload - a program for downloading loaders. Requires the modem to be in download mode or PBL emergency boot mode.

qnvram - a program to write directly on EFS partition.

dload.sh - a script for putting the modem into download mode and sending the specified loader to it.

Programs require modified versions of bootloaders to operate. They are collected in the loaders directory, and the patch source is in cmd_05_write_patched.asm.