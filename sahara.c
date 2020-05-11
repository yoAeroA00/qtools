#include "include.h"

//****************************************
//* Загрузка через сахару
//****************************************
int dload_sahara() {

FILE* in;
char infilename[200]="loaders/";
unsigned char sendbuf[131072];
unsigned char replybuf[128];
unsigned int iolen,offset,len,donestat,imgid;
unsigned char helloreply[60]={
 02, 00, 00, 00, 48, 00, 00, 00, 02, 00, 00, 00, 01, 00, 00, 00,
 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00
}; 
unsigned char donemes[8]={5,0,0,0,8,0,0,0};

qprintf("\n Ожидаем пакет Hello от устройства...\n");
port_timeout(100); // пакета Hello будем ждать 10 секунд
iolen=qread(siofd,replybuf,48);  // читаем Hello
if ((iolen != 48)||(replybuf[0] != 1)) {
	sendbuf[0]=0x3a; // может быть любое число
	qwrite(siofd,sendbuf,1); // инициируем отправку пакета Hello 
	iolen=qread(siofd,replybuf,48);  // пробуем читать Hello ещё раз
	if ((iolen != 48)||(replybuf[0] != 1)) { // теперь всё - больше ждать нечего
		qprintf("\n Пакет Hello от устройства не получен\n");
		dump(replybuf,iolen,0);
		return 1;
	}
}

// Получили Hello, 
ttyflush();  // очищаем буфер приема
port_timeout(10); // теперь обмен пакетами пойдет быстрее - таймаут 1 с
qwrite(siofd,helloreply,48);   // отправляем Hello Response с переключением режима
iolen=qread(siofd,replybuf,20); // ответный пакет
  if (iolen == 0) {
    qprintf("\n Нет ответа от устройства\n");
    return 1;
  }  
// в replybuf должен быть запрос первого блока загрузчика
imgid=*((unsigned int*)&replybuf[8]); // идентификатор образа
qprintf("\n Идентификатор образа для загрузки: %08x\n",imgid);
switch (imgid) {

	case 0x07:
	  strcat(infilename,get_nprg());
	break;

	case 0x0d:
	  strcat(infilename,get_enprg());
	break;

	default:
          qprintf("\n Неизвестный идентификатор - нет такого образа!\n");
	return 1;
}
qprintf("\n Загружаем %s...\n",infilename); fflush(stdout);
in=fopen(infilename,"rb");
if (in == 0) {
  qprintf("\n Ошибка открытия входного файла %s\n",infilename);
  return 1;
}

// Основной цикл передачи кода загрузчика
qprintf("\n Передаём загрузчик в устройство...\n");
while(replybuf[0] != 4) { // сообщение EOIT
 if (replybuf[0] != 3) { // сообщение Read Data
    qprintf("\n Пакет с недопустимым кодом - прерываем загрузку!");
    dump(replybuf,iolen,0);
    fclose(in);
    return 1;
 }
  // выделяем параметры фрагмента файла
  offset=*((unsigned int*)&replybuf[12]);
  len=*((unsigned int*)&replybuf[16]);
//  qprintf("\n адрес=%08x длина=%08x",offset,len);
  fseek(in,offset,SEEK_SET);
  fread(sendbuf,1,len,in);
  // отправляем блок данных сахаре
  qwrite(siofd,sendbuf,len);
  // получаем ответ
  iolen=qread(siofd,replybuf,20);      // ответный пакет
  if (iolen == 0) {
    qprintf("\n Нет ответа от устройства\n");
    fclose(in);
    return 1;
  }  
}
// получили EOIT, конец загрузки
qwrite(siofd,donemes,8);   // отправляем пакет Done
iolen=qread(siofd,replybuf,12); // ожидаем Done Response
if (iolen == 0) {
  qprintf("\n Нет ответа от устройства\n");
  fclose(in);
  return 1;
} 
// получаем статус
donestat=*((unsigned int*)&replybuf[12]); 
if (donestat == 0) {
  qprintf("\n Загрузчик передан успешно\n");
} else {
  qprintf("\n Ошибка передачи загрузчика\n");
}
fclose(in);

return donestat;

}

