#ifndef __SIM_SOFTWARE_H
#define __SIM_SOFTWARE_H

#include "sim_hardware.h"

#define MODE_TCP_NONE              0  // Сообщение не передаются
#define MODE_TCP_SEND              1  // Отправка только одного сообщения
#define MODE_TCP_START             2  // Отправка нескольких сообщений за заданное время
#define NONE_AT_PROG               0  // Ничего не делать
#define CONTINUE_AT_PROG           1  // Передвинуть указатель до следующей команды
#define CANCEL_AT_PROG             2  // Передвинуть указатель до следующей программы
#define WAIT_AT_CMD                3  // Оставить указатель и увеличить время ожидания
#define MAX_TIMEOUT               10  // Задает время ожидания ответа от SIM (секунд)
#define MAX_AT_CMD_QUEUE_SIZE    255  // Максимальный размер очереди команд
#define MAX_AT_PROG_QUEUE_SIZE    64  // Максимальный размер очереди программ
#define MAX_SOFT_BUF_SIZE       1536  // Максимальный размер буфера для отправки на сервер
#define MAX_SERV_BUF_SIZE        255  // Максимальный размер буфера получения ответа с сервера
#define MAX_USER_INPUT           900  // Максимальный размер буфера введенного пользователем

// Функция для возврата управления
// Должен вернуть NONE_AT_PROG, CANCEL_AT_PROG, CONTINUE_AT_PROG или WAIT_AT_CMD
typedef uint8_t (*funcAT)(char* resp);

// Определяет команду для отправки на SIM
typedef struct atCommand 
{
  char *sendAT;         // Команда, которая отправляется
  char *ifResponse;     // Ответ, который ожидается
  funcAT thenCallback;  // Функция для вызова, при ожидаемом ответе
  funcAT elseCallback;  // Функция для вызова, при неожиданном ответе
  uint16_t timeout;     // Время ожидания выполнения команды
} atCommand;

// Хранит наименование текущей программы
typedef struct atProgram
{
  char *name;           // Название
  uint8_t  last;         // Указатель на последнюю команду
} atProgram;

// Для вызова из sim_main
void SetIPPortInit(char *IP, char *Port); // Установка параметров IP-адреса и порта сервера
void InitSIM(void);                       // Инициализация модуля SIM
void OpenGPRS(char *cmd, char **param, uint8_t pcount);  // Активация GPRS
void СloseGPRS(char *cmd, char **param, uint8_t pcount); // Отключение GPRS
// Функции для sim_main
void SetIPPort(char *cmd, char **param, uint8_t pcount); // Установка IP-адреса и порта сервера
void GetIPPort(char *cmd, char **param, uint8_t pcount); // Получение IP-адреса и порта сервера
// Вывод содержимого буфера
void GetSIMBuf(char *cmd, char **param, uint8_t pcount); // Отображение буфера на прием от SIM
void GetPCBuf(char *cmd, char **param, uint8_t pcount);  // Отображение буфера на прием от ПК
// Функции работы с TCP
void SendTCP(char *cmd, char **param, uint8_t pcount);   // Отправляет сообщение по TCP
void StartTCP(char *cmd, char **param, uint8_t pcount);  // Отправка пакетов в течении времени
void StopTCP(char *cmd, char **param, uint8_t pcount);   // Преждевременная остановка отправки
void BegUserInput(void);      // Запуск отслеживания ввода
void SetUserInput(char *msg); // Данные для передачи ввода
int8_t IsUserInput(void);     // Определяет, готовы ли к вводу данных 
#endif /* __SIM_SOFTWARE_H */
