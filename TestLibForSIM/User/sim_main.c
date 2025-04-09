#include "sim_main.h"

char** ParseCmd(char *pcmd, command *cmd, uint8_t *pcount); // Производит анализ команды
void Help(char *cmd, char **param, uint8_t pcount);         // Вывод справки по командам

// Задание списка команд:
// - название команды
// - шаблон переменных  
// - описание переменных (шаблон, описание)
// - функция команды
command commands[] = {
  // Отключает GPRS
  "closegprs",
  "\tDisables GPRS.\n\n",
  NULL,
  СloseGPRS,
  // Получение параметров IP-адреса и порта сервера
  "gettcp",
  "\tObtaining server IP address and port parameters.\n\n",
  NULL,
  GetIPPort,  
  // Вывод справки по параметрам
  "help",
  "\tIf no parameters are specified, a list of commands is displayed.\n"\
  "\tIf the parameter is 'all', then the full help is displayed.\n"\
  "\tOtherwise, help is displayed only for the specified command.",
  &(paramInfo) {
    "[command]",
    "\t[command] - command for which help is displayed.\n\n"
  },  
  Help,
  // Активация GPRS
  "opengprs",
  "\tGPRS activation.\n\n",
  NULL,
  OpenGPRS,  
  // Отображение буфера на прием от ПК
  "pcbuf",
  "\tDisplaying the buffer for receiving from the PC.\n\n",
  NULL,
  GetPCBuf,
  // Отправка сообщения на сервер
  "sendtcp",
  "\tSending a message to the server.",
  NULL,
  SendTCP,
  // Установка параметров IP-адреса и порта сервера
  "settcp",
  "\tSetting the server IP address and port parameters.",
  &(paramInfo) {
    "<ip> <port>",
    "\t<ip> - N.N.N.N, N = 0..255\n"\
    "\t<port> - value in ranges 0..65535\n\n"
  },
  SetIPPort,
  // Отображение буфера на прием от SIM
  "simbuf",
  "\tDisplaying the buffer for receiving from the SIM.\n\n",
  NULL,
  GetSIMBuf,
  // Отправка пакетов на сервер в течении заданного времени
  "starttcp",
  "\tSending packets to the server within a specified time.",
  &(paramInfo) {
    "<time>",
    "\t<time> - the time the packages were sent.\n\n"
  },
  StartTCP,
  // Остановка отправки пакетов на сервер
  "stoptcp",
  "\tStop sending packets to the server.\n\n",
  NULL,
  StopTCP
};
int countCmd; // Количество команд

// Продолжение инициализации из sim_hardware
void Init(void)
{
  countCmd = sizeof(commands) / sizeof(commands[0]);
  HAL_Delay(1000);
  SendToPC("STM32 launched.\n");
  InitSIM();
  SetIPPortInit("3.134.93.252", "3334");
}

// Переопределение методов из sim_hardware
void CommandCallback(char* cmd)
{
  // Если требуется перенаправить вывод
  // для заполнения массива
  if (IsUserInput())
  {
    SetUserInput(cmd);
  }
  else
  {
    char* c;
    uint8_t pcount;
    uint8_t isFound = 0;
    // Попытка определить команду
    for (int i = 0; i < countCmd; i++)
      if ((c = strstr(cmd, commands[i].name)) != NULL)
      {
        // Анализ и вызов команды  
        char** pc = ParseCmd(c, &commands[i], &pcount);
        // Название команды, параметры, количество
        commands[i].func(c, pc, pcount);
        // Отметка, что нашли команду
        isFound = 1;
        break;
      }
    // Перессылка команды на устройство
    if (!isFound)
      SendToSIM(cmd);
  }
}

// Для проверки на пробельные символы
#define WS(p) (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')

// Производит анализ команды
char** ParseCmd(char *pcmd, command *cmd, uint8_t *pcount)
{
  char *pt, **p, **params = NULL;
  uint8_t *count = 0;
  *pcount = 0;
  // Выделение команды
  while (!WS(pcmd) && *pcmd != '\0') pcmd++;
  // Проверка на параметры
  if (cmd->param != NULL)
  {
    pt = cmd->param->ptemp;
    // Подсчёт параметров
    while (*pt != '\0')
    {
      if (*pt == '<' || *pt == '[')
        count++;
      pt++;
    }
    pt = cmd->param->ptemp;
    // Выделение памяти
    uint32_t s = sizeof(char*) * (uint32_t)count;
    p = params = (char**)malloc(s);
     // Поиск переменных
    while (*pt != '\0')
    {
      // Есть ли параметры
      if (WS(pcmd))
      {
        *pcmd = '\0'; // Ограничение предыдущего указателя
        pcmd++;
        // Перемотка пробельных символов
        while (WS(pcmd)) pcmd++;  
        // Нашли ли слово
        if (*pcmd != '\0')
        {
          *p = pcmd;  // Сохранение переменной
          p++;
          (*pcount)++;  // Увеличение количества
          // Перемотка до следующей
          while (!WS(pcmd) && *pcmd != '\0') pcmd++;
          while (!WS(pt) && *pt != '\0') pt++;
          while (WS(pt)) pt++;
          continue;
        }
      }      
      // Если параметр не помечен как обязательный
      if (*pt != '[')
      {
        SendToPC("Parameter ");
        SendToPC(pt);
        SendToPC(" not set!\n");
      }
      break;
    }
    *(pcmd) = '\0';
  }
  else
    *pcmd = '\0';
  return params;
}

// Вывод справки по командам
void Help(char *cmd, char **param, uint8_t pcount)
{
  // Проверка параметров
  if (pcount == 1)
  {
    // Вывод всех "all" или только указанной команды
    for (int i = 0; i < countCmd; i++)
      if (strcmp(param[0], "all") == 0 || strcmp(param[0], commands[i].name) == 0)
      {
        SendToPC(commands[i].name);
        if (commands[i].param != NULL)
        {
          SendToPC(" ");
          SendToPC(commands[i].param->ptemp);
        }
        SendToPC("\nDescription:\n");
        SendToPC(commands[i].desc);
        if (commands[i].param != NULL)
        {
          SendToPC("\nParameters:\n");
          SendToPC(commands[i].param->desc);
        }
      }
  }
  else
  {
    // Вывод только названий команд
    SendToPC("help <command>\ncommand:\n\tall - full help is displayed\n\t");
    for (int i = 0; i < countCmd; i++)
    {
      SendToPC(commands[i].name);
      SendToPC("\n\t");
    }
    SendToPC("\n");
  }
  free(param);
}
