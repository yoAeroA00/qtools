# qtools
Набор инструментов для работы с flash модемов на чипсете qualcom
Набор состоит из пакета утилит и набора патченных загрузчиков.

qcommand - интерактивный терминал для ввода команд через командный порт. Идет на замену жутко неудобного revskills.
           Позволяет вводить побайтно командные пакеты, редактировать память, читать и просматривать любй сектор flash.

qread - программа для чтения дампа адресного пространства модема

qrlfash - программа для чтения flash. Умеет читать как диапазон блоков, так и разделы по карте разделов.

qdload - загрузчик загрузчиков. Требует, чтобы модем был в download mode, или режиме аварийной загрузки PBL

dload.sh - скрипт для перевода модема в режим загрузки и отправки в него указанного загрузчика.

Для работы программ требуются модифицированные версии загрузчиков. Они собраны в каталоге loaders, а исходник патча
лежит в cmd_05_write_patched.asm.