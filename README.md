# qtools mod@yoAeroA00

A Set of Tools for working with flash on devices with Qualcomm Chipsets. The toolkit consists of a package of utilities and a set of patched loaders.

qcommand - an interactive terminal for entering commands through the command port. It replaces the highly inconvenient "revskills."
            It allows you to enter command packets byte by byte, edit memory, read and view any flash sector.

qrmem - a program for reading the dump of the modem's address space.

qrflash - a program for reading flash. It can read both ranges of blocks and partitions based on the partition map.

qwflash - a program for writing partition images through the user partitions loader mode, similar to QPST.

qwdirect - a program for directly writing flash blocks with or without spare area through controller ports (without the involvement of loader logic).

qdload - a program for loading loaders. It requires the modem to be in download mode or PBL (Primary Boot Loader) emergency boot mode.

dload.sh - a script for putting the modem into download mode and sending the specified loader to it.

To use these programs, modified versions of the loaders are required. They are assembled in the "loaders" directory, and the source code for the patch is located in cmd_05_write_patched.asm.
