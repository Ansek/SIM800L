#ifndef __SIM_HARDWARE_H
#define __SIM_HARDWARE_H

#define CDC_RX_COUNT_PACKAGES 10                     // Количество пакетов, принимаемых от ПК
#define CDC_RX_DATA_SIZE 64 * CDC_RX_COUNT_PACKAGES  // Размер буфера для данных от ПК
#define CDC_TX_DATA_SIZE     255                     // Размер стека буферов для отправки данных на ПК
#define UART_RX_DATA_SIZE   1024                     // Размер буфера для данных от SIM
#define INT_BUF_SIZE          15                     // Размер буфера для преобразования числа в строку

#include "stm32f4xx_hal.h"
#include "usbd_cdc_if.h"
#include <string.h>

// Методы для файлов STM
void SIM_UART_Init(UART_HandleTypeDef *huart, IRQn_Type IRQn); // Инициализация UART
void SIM_TIM_Init(TIM_HandleTypeDef *tim, IRQn_Type IRQn);     // Инициализация таймера
void SIM_Init(void);                                      // Инициализация для SIM
void SIM_Loop(void);                                      // Выполнение кода в цикле main
void SIM_CDC_Rx_Callback(uint8_t* Buf, uint32_t *Len);    // Функция для приема данных с COM-порта
void SIM_CDC_Tx_Callback(void);                           // Сигнал о завершении передачи по COM-порту
void SIM_UART_SetCallbackForIdle(void);                   // Установка вызова при прерывании по Idle

// Методы для файлов SIM
void SendToSIM(char* msg);	        // Отправка данных на SIM (UART)
void SendToPC(char* msg);           // Отправка данных на ПК (COM-порт)
void SendI32ToPC(uint32_t value);   // Отправка числа на ПК (COM-порт)
void ShowBufSIM(void);              // Подготавливает и отображает буфер приема от SIM
void ShowBufPC(void);               // Подготавливает и отображает буфер приема от ПК
uint64_t Time(void);                // Возвращает время со старта STM (секунды)
#endif /* __SIM_HARDWARE_H */
