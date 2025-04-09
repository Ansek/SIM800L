#ifndef __SIM_MAIN_H
#define __SIM_MAIN_H

#include "sim_software.h"

// Шаблон функции команды
// Имя команды, параметры, число параметров.
typedef void (*cmdFunc)(char *cmd, char **param, uint8_t pcount);
  
// Информация о переменных  
typedef struct pInfo
{
  char *ptemp;  // Шаблон переменных (<обязательный> или [нет])
  char *desc;   // Описание переменных
} paramInfo;

// Определяет команду для отправки на SIM
typedef struct command
{
  char *name;       // Название команды
  char *desc;       // Описание команды
  paramInfo *param; // Описание переменных
  cmdFunc func;     // Функция команды
} command;

#endif /* __SIM_MAIN_H */
