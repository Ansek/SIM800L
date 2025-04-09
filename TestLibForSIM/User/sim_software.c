#include "sim_software.h"

char softBuffer[MAX_SOFT_BUF_SIZE];   // Для отправки сообщений на сервера
char serverBuffer[MAX_SERV_BUF_SIZE]; // Для приема сообщений с сервера
char CIPSTART[45];       // Команда установки адреса сервера и порта
char IPPORT[35];         // Для вывода текущего адреса сервера и порта
char* response;          // Указатель на ожидаемый ответ (если не весь ответ пришёл сразу)
char OKSEND[7] = ">";    // Для регулирования ответа при отправке на сервер

// Работа с AT-программами 
atCommand *queueATCmd[MAX_AT_CMD_QUEUE_SIZE];  // Список указателей на AT-команды
atProgram queueATProg[MAX_AT_PROG_QUEUE_SIZE]; // Список запущенных программ (введенных команд)
uint8_t firstQueueATCmd  = 0;  // Указатель на первую команду
uint8_t lastQueueATCmd   = 0;  // Указатель на последнюю команду
uint8_t countATCmd       = 0;  // Количество активных команд
uint8_t firstQueueATProg = 0;  // Указатель на первую программу
uint8_t lastQueueATProg  = 0;  // Указатель на последнюю программу
uint8_t countATProg      = 0;  // Количество активных программ
uint8_t nextCmdIsReady   = 0;  // Опредляет готовность отправки следующей команды
uint8_t cmdIsWaitResp    = 0;  // Флаг, что SIM ожидает ответа от SIM
uint8_t isLockSoftBuffer = 0;  // Заблокирован ли буфер для записи
uint8_t isCheckTimeout   = 0;  // Флаг, что следует отслеживать время ожидания
uint8_t errorCounter     = 0;  // Счётчик для отслеживания ошибок
uint64_t timerTimeout    = 0;  // Хранит границы отсчёта
// TCP
uint8_t modeTCP          = 0;  // Указывает в каком режиме сейчас идет отправка
uint64_t timerTCP        = 0;  // Хранит границы отсчёта
uint32_t timerPkg        = 0;  // Время с отправки пакета
uint32_t timerPkgAll     = 0;  // Полное время с отправки пакета
uint32_t numberPkg       = 0;  // Номер отправляемого пакета
uint16_t userInputSize   = 0;  // Указывает размер введенных данных 
uint8_t isUserInput      = 0;  // Для оповещения о готовности приема в буфер

uint8_t NoError(char* msg);    // Для продолжения, если нет сообщения ERROR
uint8_t InitСompl(char* msg);  // Оповещение о завершении настроек
uint8_t ErrCSTT(char* msg);    // Функция осуществляет попытку переключения
uint8_t ShowValue(char* msg);  // Отображает полученное содержимое
uint8_t TCPSetMsg(char* msg);  // Установка сообщения для отправки на сервер
uint8_t TCPSendMsg(char* msg); // Совершает отправку на сервер
uint8_t TCPMsgSent(char* msg); // Передача на сервер завершена

//#define DONT_SHOW_SERV_MSG_AUTO // Запрашивать ли ответ с сервера вручную

// Инициализация набора команда
// Команда, ожидаемый ответ, функция при ожидаемом ответе, 
// функция при неожиданном ответе, время ожидания в секундах

// Инициализация SIM
atCommand initSim[] = {
  "AT\n",             "ATOK",         NULL,        NoError,   MAX_TIMEOUT, // Автонастройка скорости
  "ATE0\n",           "ATE0OK",       NULL,        NoError,   MAX_TIMEOUT, // Отключение эха
#ifdef  DONT_SHOW_SERV_MSG_AUTO
  "AT+CIPRXGET=1\n",  "OK",            NULL,        NULL,      MAX_TIMEOUT, // Вывод данных с TCP по запросу
#endif
  "AT+CIPSPRT=1\n",   "OK",            InitСompl,   NULL,      MAX_TIMEOUT  // Отобр. курсор (CIPSEND)
};

// Активация GPRS
#define CSTT "AT+CSTT=\"internet\",\"gdata\",\"gdata\"\n"
atCommand openGPRS[] = {
  CSTT,               "OK",            NULL,         ErrCSTT,   MAX_TIMEOUT, // Настройка контекста
  "AT+CIICR\n",       "OK",            NULL,         NULL,      MAX_TIMEOUT, // Активация
  "AT+CIFSR\n",       "",              ShowValue,   NULL,      MAX_TIMEOUT  // Получение IP
};

// Отключение GPRS
atCommand closeGPRS[] = {
  "AT+CIPSHUT\n",     "SHUTOK",       NULL,        NULL,      MAX_TIMEOUT  // Отклчение  
};

// Программа для отправки сообщения на сервер
atCommand sendTCP[] = {
  CIPSTART,           "OKCONNECTOK",   TCPSetMsg,   NULL,      MAX_TIMEOUT, // Подключение 
  "AT+CIPSEND\n",     OKSEND,         NULL,         NULL,      15,          // Отключение
  "AT+CIPCLOSE\n",    "CLOSEOK",      TCPMsgSent,  NULL,      MAX_TIMEOUT  // Завершение 
};
#define COUNT_TCP_CMD 3;

/*----------------------- Определение механизма отправки AT-команд ---------------------------*/

// Добавление набора команд в начало очереди
// Возвращает 1 если список добавлен
uint8_t AddToBeginQueue(atCommand *cmds, uint16_t n)
{
  // Проверка возможности добавления команды
  if (countATCmd < MAX_AT_CMD_QUEUE_SIZE - n)
  {
    for (int i = n; i >= 0; i--)
    {
      queueATCmd[--firstQueueATCmd] = &cmds[i];
      countATCmd++;
    }
    return 1;
  }
  else
  {
    SendToPC("The command queue is full!");
    return 0;
  }
}

// Добавление набора команд в конец очереди
// Возвращает 1 если список добавлен
uint8_t AddToEndQueue(atCommand *cmds, uint16_t n)
{
  // Проверка возможности добавления команды
  if (countATCmd < MAX_AT_CMD_QUEUE_SIZE - n)
  {
    for (int i = 0; i < n; i++)
    {
      queueATCmd[lastQueueATCmd++] = &cmds[i];
      countATCmd++;
    }
    return 1;
  }
  else
  {
    SendToPC("The command queue is full!");
    return 0;
  }
}

// Добавление программы в очередь
// Возвращает ссылку на запись о программе
atProgram *RegisterProgram(char* name)
{
  if (countATProg < MAX_AT_CMD_QUEUE_SIZE - 1)
  {
    queueATProg[lastQueueATProg++].name = name;
    countATProg++;
    return &queueATProg[lastQueueATProg - 1];
  }
  else
  {
    SendToPC("The program queue is full!");
    return NULL;
  }  
}

// Запускает текущую команду на выполнение
void RunCommand()
{
  if (firstQueueATCmd != lastQueueATCmd)
  {
    // Сохранение ожидаемого ответа
    response = queueATCmd[firstQueueATCmd]->ifResponse;
    // Установка таймера на ожидание
    timerTimeout = Time() + queueATCmd[firstQueueATCmd]->timeout;
    isCheckTimeout = 1;
    cmdIsWaitResp = 1;
    // Отправка команды
    SendToSIM(queueATCmd[firstQueueATCmd]->sendAT);
  }
}

// Переход к следующей команде
void RunNextCommand()
{
  firstQueueATCmd++;
  countATCmd--;
  RunCommand();
}

// Сохраняет имя запущенной программы
// и загружает команды на выполнение
void RunProgram(char* cmd, atCommand *cmds, uint16_t n)
{
  // Проверка возможности добавить программу
  atProgram *atProg = RegisterProgram(cmd);
  // Загрузка набора команд в очередь
  if (atProg != NULL && AddToEndQueue(cmds, n))
  {
    // Сохранение индекса конца и запуск команды
    atProg->last = lastQueueATCmd; 
    RunCommand();
  }
}

// Отмена первой команды
void CancelProgram(void)
{
  // Очистка команд
  while (firstQueueATCmd != queueATProg[firstQueueATProg].last)
  {
    firstQueueATCmd++;
    countATCmd--;
  }
  // Удаление программ
  firstQueueATProg++;
  countATProg--;
  // Если остались еще команды следующей программы
  RunCommand();
}

// Для проверки на пробельные символы
#define WS(p) (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')

// Сравнивает полученный с ожидаемым
void ParseResponce(char *msg)
{
  uint8_t mode = 0; // CANCEL_AT_PROG, CONTINUE_AT_PROG или WAIT_AT_CMD
  char* tmsg = msg; // Сохранение исходного сообщения для вызова в обработчиках
  char* resp = response;
  isCheckTimeout = 0; // Сброс флага ожидания
  while (*msg != '\0')
  {
    // Пропуск пробельных символов
    if (!WS(msg))
    {
      // Сравнение символов
      if (*resp == *msg)
        resp++;
      else 
      {
        // Если есть обработчик события
        if (queueATCmd[firstQueueATCmd]->elseCallback != NULL)
          mode = queueATCmd[firstQueueATCmd]->elseCallback(tmsg);
        else
          mode = CANCEL_AT_PROG;
        break;
      }      
    }
    // Если полностью просмотрели шаблон 
    if (*resp == '\0')
    {
      // Если есть обработчик события
      if (queueATCmd[firstQueueATCmd]->thenCallback != NULL)
        mode = queueATCmd[firstQueueATCmd]->thenCallback(tmsg);
      else
        mode = CONTINUE_AT_PROG;
      break;
    }    
    msg++;
  }
  // Если часть ответа просмотрели успешно (ответ пришел не полностью)
  if (*msg == '\0' && *resp != '\0')
  {
    response = resp; // Обновляем для следующего разбора  
    mode = WAIT_AT_CMD;
  }    
  // Выбор дальнейшего действия
  switch (mode)
  {
    case CONTINUE_AT_PROG:  
      RunNextCommand();  // Запускаем следующую команду
    break;
    case CANCEL_AT_PROG:  
      SendToPC("The \"");
      SendToPC(queueATProg[firstQueueATProg].name);
      SendToPC("\" program has ended ahead of schedule.\nLast command: ");
      SendToPC(queueATCmd[firstQueueATCmd]->sendAT);
      SendToPC("Responce: ");
      SendToPC(tmsg);
      SendToPC("\n");
      CancelProgram();    // Отмена текущей команды
    break;
    case WAIT_AT_CMD:  
      // Обновление таймера
      timerTimeout = Time() + queueATCmd[firstQueueATCmd]->timeout;
      isCheckTimeout = 1;
      cmdIsWaitResp = 1;      
    break;
  }    
}

// Продолжение цикла из sim_hardware
void Loop()
{
  if (isCheckTimeout && Time() > timerTimeout)
  {
    // Оповещение по истечении срока ожидания ответа от SIM
    SendToPC("Timeout error. Completion of the \"");
    SendToPC(queueATProg[firstQueueATProg].name);
    SendToPC("\" program.\nLast command: ");
    SendToPC(queueATCmd[firstQueueATCmd]->sendAT);
    SendToPC("\n");
    isCheckTimeout = 0;
    CancelProgram(); // Очистка команд для данной программы
  }
}

// Получение ответа от SIM из sim_hardware
void ResponceCallback(char* msg) 
{
  #ifdef DONT_SHOW_SERV_MSG_AUTO
  // Получение сообщения в ручном режиме
  if (strstr(msg, "+CIPRXGET: 1") != NULL)
  {
    // Уточнение размера принятых данных
    SendToSIM("AT+CIPRXGET=4\n");
  }  
  else if (strstr(msg, "+CIPRXGET: 4") != NULL)
  {
    // Получение значения размера, присланных данных
    char* s = strstr(msg, ",");
    uint16_t i = atoi(s);
    // Проверка, что не равно 0
    if (i)
    {
      if (i > MAX_SERV_BUF_SIZE)
        i = MAX_SERV_BUF_SIZE;
      sprintf(serverBuffer, "AT+CIPRXGET=2,%d\n", i);
      SendToSIM(serverBuffer);
    }
  }
  else if (strstr(msg, "+CIPRXGET: 2") != NULL)
  {
    //Вывод сообщения
    SendToPC("Server response:");
    SendToPC(msg);
  }
  #else
  // Получение сообщения в автоматическом режиме
  if (strstr(msg, "Hello!") != NULL)
  {
    char *s = strstr(msg, "\n");
    msg = strstr(msg, "CLOSE");    
    *s = '\0';
    SendToPC(s);
    SendToPC("\n");
  }
  #endif
  if (cmdIsWaitResp)
  {
    // Определение результата работы AT-команды
    cmdIsWaitResp = 0;
    ParseResponce(msg);
  }
  else
    // Перенаправление на ПК
    SendToPC(msg);
}

/*------------------------- Функции для обработки ответов от SIM -----------------------------*/

// Для продолжения, если нет сообщения ERROR
uint8_t NoError(char* msg)
{
  if (strstr(msg, "ERROR") == NULL)
    return CONTINUE_AT_PROG;
  return CANCEL_AT_PROG;
}  

// Оповещение о завершении настроек
uint8_t InitСompl(char* msg)
{
  SendToPC("SIM configuration completed successfully.\n");
  return CONTINUE_AT_PROG;  
}

// Функция осуществляет попытку переключения
uint8_t ErrCSTT(char* msg)
{
  if (strstr(msg, "ERROR") != NULL && errorCounter == 0)
  {
    // Помещение команды отключения перед подключением
    AddToBeginQueue(closeGPRS, 1);
    errorCounter++;
    return WAIT_AT_CMD;
  }
  else
  {
    errorCounter = 0;
    return CANCEL_AT_PROG;
  }
}

// Отображает полученное содержимое
uint8_t ShowValue(char* msg)
{
  SendToPC(msg);
  return CONTINUE_AT_PROG;
}

// Установка сообщения для отправки на сервер
uint8_t TCPSetMsg(char* msg)
{
  strcpy(OKSEND, ">");
  queueATCmd[firstQueueATCmd + 1]->thenCallback = TCPSendMsg;
  return CONTINUE_AT_PROG;
}  

// Совершает отправку на сервер
uint8_t TCPSendMsg(char* msg)
{
  strcpy(OKSEND, "SENDOK");  
  queueATCmd[firstQueueATCmd]->thenCallback = NULL;
  if (modeTCP == MODE_TCP_SEND)
  {
    
    SendToSIM(softBuffer);
    return WAIT_AT_CMD;
  }
  else if (modeTCP == MODE_TCP_START)
  {
    char* temp; // Создание своего массива
    temp = softBuffer;
    for (int i = 0; i < 900; i++)
    {
      *temp = 'a';
      temp++;
    }
    *temp = 0x1A;
    SendToSIM(softBuffer);
    return WAIT_AT_CMD;
  }
  return CONTINUE_AT_PROG;
}

// Передача на сервер завершена
uint8_t TCPMsgSent(char* msg)
{
  if (modeTCP == MODE_TCP_NONE)
  {
    timerTCP = Time();
    return CANCEL_AT_PROG;
  }
  if (modeTCP == MODE_TCP_SEND)
  {
    SendToPC("The message has been sent to the server.\n");
  }
  else if (modeTCP == MODE_TCP_START)
  {
    SendToPC("Package #");
    SendI32ToPC(++numberPkg);
    SendToPC(" sent (");
    SendI32ToPC(Time() - timerPkg);
    SendToPC(" s.)\n");
    timerPkg = Time();
    // Зацикливание, если время не истекло
    if (Time() < timerTCP)
    {
      // Переход на три команды назад
      firstQueueATCmd -= COUNT_TCP_CMD;
      countATCmd += COUNT_TCP_CMD;  
    }
    else
    {
      SendToPC("End sending packages -  ");
      SendI32ToPC(Time() - timerPkgAll);
      SendToPC(" s.\n");
    }
  }
  return CONTINUE_AT_PROG;
}

/*------------------------- Функции для использования в sim_main -----------------------------*/

// Установка параметров IP-адреса и порта сервера
void SetIPPortInit(char* IP, char* Port)
{
  sprintf(CIPSTART, "AT+CIPSTART=\"TCP\",\"%s\",\"%s\"\n", IP, Port);
  sprintf(IPPORT, "IP: %s\nPort: %s\n", IP, Port);
}

// Инициализация модуля SIM
void InitSIM(void)
{
  uint16_t n = sizeof(initSim) / sizeof(initSim[0]);
  RunProgram("initSim", initSim, n);
}

// Активация GPRS
void OpenGPRS(char *cmd, char **param, uint8_t pcount)
{
  uint16_t n = sizeof(openGPRS) / sizeof(openGPRS[0]);
  RunProgram(cmd, openGPRS, n);
}

// Отключение GPRS
void СloseGPRS(char *cmd, char **param, uint8_t pcount)
{
  RunProgram(cmd, closeGPRS, 1);
}

// Установка IP-адреса и порта сервера
void SetIPPort(char *cmd, char **param, uint8_t pcount)
{
  if (pcount == 2)
  {
    sprintf(CIPSTART, "AT+CIPSTART=\"TCP\",\"%s\",\"%s\"\n", param[0], param[1]);
    sprintf(IPPORT, "IP: %s\nPort: %s\n", param[0], param[1]);
    GetIPPort(cmd, param, pcount);
  }    
}

// Получение IP-адреса и порта сервера
void GetIPPort(char *cmd, char **param, uint8_t pcount)
{
  SendToPC(IPPORT);
}

// Отправляет сообщение по TCP
void SendTCP(char *cmd, char **param, uint8_t pcount)
{
  modeTCP = MODE_TCP_SEND;
  BegUserInput();
}

// Отправка пакетов в течении времени
void StartTCP(char *cmd, char **param, uint8_t pcount)
{
  if (pcount == 1)
  {
    int t = atoi(param[0]);
    if (t)
    {
      uint16_t n = sizeof(sendTCP) / sizeof(sendTCP[0]);
      SendToPC("Start sending packages\n");      
      // Установка времени  
      timerTCP = Time() + t;
      numberPkg = 0;
      timerPkg = Time();
      timerPkgAll = Time();
      // Установка режима
      modeTCP = MODE_TCP_START;
      // Настройка и запуск программы
      RunProgram(cmd, sendTCP, n);
    }
  }
}

// Преждевременная остановка отправки
void StopTCP(char *cmd, char **param, uint8_t pcount)
{
  if (modeTCP == MODE_TCP_START)
  {
    modeTCP = MODE_TCP_NONE;
    // Сброс состояний ()
    timerTCP = Time() - 1;
    timerTimeout = Time() - 1;
    response = "";
    isCheckTimeout = 0;
    cmdIsWaitResp = 0;
    firstQueueATProg = queueATProg[firstQueueATProg].last;
    firstQueueATProg++;
    countATProg--;
  }
}

// Отображение буфера на прием от SIM
void GetSIMBuf(char *cmd, char **param, uint8_t pcount)
{
  SendToPC("SIM buffer content: \"\n");
  ShowBufSIM();
  SendToPC("\"\n\n");
}

// Отображение буфера на прием от ПК
void GetPCBuf(char *cmd, char **param, uint8_t pcount)
{
  SendToPC("PC buffer content: \"\n");
  ShowBufPC();
  SendToPC("\"\n\n");
}

// Запуск отслеживания ввода
void BegUserInput(void)
{
  SendToPC("Enter data. Press \"Enter\" again to finish.\n");
  isUserInput = 1;
}

// Данные для передачи ввода
void SetUserInput(char *msg)
{
  int16_t size = strlen(msg);
  if (strcmp(msg, "\n") != 0 &&
        userInputSize < MAX_USER_INPUT - size)
  {
    // Копирование в буфер для вывода
    strcpy(softBuffer + userInputSize, msg);
    userInputSize += size;
  }
  else
  {
    // Завершение передачи
    isUserInput = 0;
    // Завершение обработки строк
    softBuffer[userInputSize] = 0x1A;
    softBuffer[userInputSize+1] = '\0';
    userInputSize = 0;    
    SendToPC("Data recorded.\n");
    // Запуск программы для отправки по TCP
    if (modeTCP == MODE_TCP_SEND)
    {
      uint16_t n = sizeof(sendTCP) / sizeof(sendTCP[0]);    
      // Настройка и запуск программы
      RunProgram("sendtcp", sendTCP, n);
    }
  }
}

// Определяет, готовы ли к вводу данных 
int8_t IsUserInput(void)
{
  return isUserInput;
}

