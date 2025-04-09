#include "sim_hardware.h"

TIM_HandleTypeDef  *tim  = NULL; // Ссылка на устройство таймера
UART_HandleTypeDef *uart = NULL; // Ссылка на устройство UART
char     intBuf[INT_BUF_SIZE];            // Буфер для преобразования числа в строку
char    *cdcTxData[CDC_TX_DATA_SIZE];     // Данные для ПК
uint8_t  cdcRxData[CDC_RX_DATA_SIZE];     // Данные от ПК
uint8_t *cdcRxPos = cdcRxData;            // Позиция записи для данных от ПК
uint8_t  uartRxBuffer[UART_RX_DATA_SIZE]; // Буфер от SIM (для циклического DMA)
uint8_t  uartRxData[UART_RX_DATA_SIZE];   // Данные от SIM
uint8_t  firstTxData      = 0;   // Указатель на первый отправляемый буфер для ПК
uint8_t  lastTxData       = 0;   // Указатель на последний отправляемый буфер для ПК
uint8_t  cdcRxCountPkg    = 0;   // Количество принятых пакетов от ПК  
uint8_t  cdcTxIsReady     = 1;   // Флаг завершения отправки данных
uint8_t  cdcRxIsReady     = 0;   // Флаг получения данных из COM-порта
uint8_t  uartTxIsReady    = 1;   // Флаг завершения отправки данных
uint8_t  uartRxIsReady    = 0;   // Флаг получения данных 
uint8_t  uartRxIsRollover = 0;   // Флаг сброса DMA (переход к началу)
uint16_t uartRxPos        = 0;   // Позиция в буфере uartTxBuffer
uint16_t uartRxDataSize   = 0;   // Размер сообщения, записанного в uartRxData
uint16_t cdcRxDataSize    = 0;   // Размер сообщения, записанного в cdcRxData
uint64_t timer            = 0;   // Для хранения времени со старта STM
uint8_t  countBufPC       = 0;   // Количество буферов для отправки на ПК
uint8_t  isErrRxBufCDC    = 0;   // Флаг, что есть ошибка переполнения буфера ПК
uint8_t  isErrRxBufUART   = 0;   // Флаг, что есть ошибка переполнения буфера SIM
uint8_t  isErrUARTFE      = 0;   // Флаг, что случилась ошибка кадра
uint8_t  timerErrUARTFE   = 0;   // Таймер для интервального оповещения об ошибке кадра
uint8_t  pIntBuf          = 0;   // Ссылка на массив чисел

/*----------- Функции обратного вызова для переопределения в других файлах SIM ---------------*/

// Для продолжения инициализации (sim_main)
__weak void Init(void)
{
  SendToPC("Init function is undefined! File sim_hardware.c\n");
}

// Для продолжения цикла (sim_software)
__weak void Loop(void)
{
  SendToPC("Loop function is undefined! File sim_hardware.c\n");
}

// Для оповещения о приеме с ПК (sim_main)
__weak void CommandCallback(char* cmd)  
{
  SendToPC("CommandCallback function is undefined! File sim_hardware.c\n");
}

// Для оповещения о приеме с UART (sim_software)
__weak void ResponceCallback(char* msg) 
{
  SendToPC("ResponceCallback function is undefined! File sim_hardware.c\n");  
}

/*------------------------- Функции для интеграции в файлах STM ------------------------------*/

// Настройка UART
void SIM_UART_Init(UART_HandleTypeDef *huart, IRQn_Type IRQn)
{
  uart = huart;
  // Включение прерываний для USART
  HAL_NVIC_EnableIRQ(IRQn);
  __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);
  // Запуск DMA на прием данных
  HAL_UART_Receive_DMA(huart, uartRxBuffer, UART_RX_DATA_SIZE);
}

// Настройка таймера
void SIM_TIM_Init(TIM_HandleTypeDef *htim, IRQn_Type IRQn)
{
  tim = htim;
  // Настройка приоритета
  HAL_NVIC_SetPriority(IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(IRQn);
  // Запуск таймера
  HAL_TIM_Base_Init(htim);
  HAL_TIM_Base_Start_IT(htim);
}  

// Настройка других параметров
void SIM_Init()
{
  // Проверка установки параметров
  if (uart == NULL)
    SendToPC("The UART has not been set!\n");
  if (tim == NULL)
    SendToPC("The timer has not been set!\n");
  Init(); // Продолжение в sim_main
}

// Выполнение повторения на устройстве
void SIM_Loop(void)
{
  if (isErrRxBufCDC)
  {
    // Отправка сообщения о переполнения буфера приема от PC
    SendToPC("PC buffer overflow!\n"\
      "To view the contents of the buffer, enter the PC buffer \"pcbuf\".\n");
    isErrRxBufCDC = 0;    
  }
  if (isErrRxBufUART)
  {
    // Отправка сообщения о переполнения буфера приема от SIM
    SendToPC("SIM buffer overflow!\n"\
      "To view the contents of the buffer, enter the SIM buffer \"simbuf\".\n");
    isErrRxBufUART = 0;    
  }
  if (uartRxIsReady)
  {
    // Оповещение о приеме ответа от SIM
    uartRxData[uartRxDataSize] = '\0';
    ResponceCallback((char*)uartRxData);
    uartRxIsReady = 0;
  }
  if (cdcRxIsReady)
  {
    // Оповещение о приеме команды с ПК
    cdcRxData[cdcRxDataSize] = '\0';
    CommandCallback((char*)cdcRxData); // Продолжение в sim_main
    cdcRxIsReady = 0;
  }
  if (cdcTxIsReady && firstTxData != lastTxData)
  {
    // Отправка данных на ПК
    char* buf = cdcTxData[firstTxData];
    uint16_t size = strlen(buf);
    // Отправка сообщения
    CDC_Transmit_FS((uint8_t*)buf, size);
    firstTxData++;
    cdcTxIsReady = 0;
  }  
  if (isErrUARTFE)
  {
    // Оповещение и установка таймера
    if (timerErrUARTFE == 0)
    {
      SendToPC("UART frame error!\n");
      timerErrUARTFE = timer;
      isErrUARTFE = 0;
    }
  }
  if (timer > timerErrUARTFE)
  {
    // Сброс таймера, если ошибка не появилась вновь
    timerErrUARTFE = 0;
  }
  Loop(); // Продолжение в sim_software
}

// Сигнал о завершении передачи по COM-порту
void SIM_CDC_Tx_Callback(void)
{
  cdcTxIsReady = 1;
}

// Накопления пакетов в единое сообщение с ПК
void SIM_CDC_Rx_Callback(uint8_t* Buf, uint32_t *Len)
{
  // Если принимается первый пакет
  if (cdcRxPos == cdcRxData) 
  {
    cdcRxDataSize = 0;
    cdcRxIsReady = 0;
  }
  // Если превысили лимит получаемых пакетов 
  if (cdcRxCountPkg == CDC_RX_COUNT_PACKAGES)
  {
    cdcRxCountPkg = 0;
    cdcRxPos = cdcRxData;
    if (*Len == 64 && Buf[63] != '\0') {
      isErrRxBufCDC = 1;  // Подать сигнал о переполнении
    }
  }
  else 
  {
    // Копирование пакета в общий буфер
    cdcRxDataSize += *Len;
    memcpy(cdcRxPos, Buf, *Len);
    // Если это последний пакет
    if (*Len < 64 || Buf[63] == '\0')
    {
      cdcRxCountPkg = 0;
      cdcRxPos = cdcRxData;
      cdcRxIsReady = 1;
    }
    else // Иначе увеличиваем параметры для приема следующего
    {
      cdcRxCountPkg++;
      cdcRxPos += 64;
    }
  }
}

// Установка прерывания по простою на линии UART
void SIM_UART_SetCallbackForIdle(void)
{
  if (__HAL_UART_GET_FLAG(uart, UART_FLAG_IDLE))
    HAL_UART_RxCpltCallback(uart);
}

/*----------------------- Функции абстрагирующие использование STM ---------------------------*/

// Отправка данных на SIM
void SendToSIM(char* msg)
{
  HAL_UART_Transmit_DMA(uart, (uint8_t*)msg, strlen(msg));
}

// Отправка данных на ПК
void SendToPC(char* msg)
{
  // Добавление в очередь
  cdcTxData[lastTxData] = msg;
  lastTxData++;
}

// Отправка числа на ПК (COM-порт)
void SendI32ToPC(uint32_t value)
{
  uint32_t s = sprintf(intBuf + pIntBuf, "%d", value);
  cdcTxData[lastTxData] = intBuf + pIntBuf;
  pIntBuf += s + 1;
  lastTxData++;    
}    

// Добавляет переносы буферу для читабельности
void FormatBuffer(uint8_t* buf, uint16_t size)
{
  int lastWS = 0, j = 0;
  // Поиск пробельных символов, чтобы заменить их на перенос, 
  // в случае если строка длиннее 25 символов
  for (int i = 0; i < size; i++, j++)
  {
    // Замена нулевых, чтобы вывести полностью
    if (buf[i] == '\0')
      buf[i] = ' ';
    // Определение пробельных символов
    if (buf[i] == ' ' || buf[i] == '\t')
      lastWS = i;
    else if (buf[i] == '\n')
      j = 0;
    // Определение превышения границы
    if (j > 25)
    {
      if (lastWS == 0)
        buf[i] = '\n';
      else
        buf[lastWS] = '\n';
      j = 0;
    }
  }
  buf[size - 1] = '\0';
  // Помещение буфера в список
  cdcTxData[lastTxData] = (char*)buf;
  lastTxData++;
}

// Подготавливает и отображает буфер приема от SIM
void ShowBufSIM(void)
{
  FormatBuffer(uartRxBuffer, UART_RX_DATA_SIZE);
}

// Подготавливает и отображает буфер приема от ПК
void ShowBufPC(void)
{
 FormatBuffer(cdcRxData, CDC_RX_DATA_SIZE);
}

// Возвращает время со старта STM (секунды)
uint64_t Time(void)
{
  return timer;
}

/*------------------------ Переопределение встроенных функций STM ----------------------------*/

// Завершена отправка данных по UART
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  uartTxIsReady = 1;    // Оповещение о отправке данных   
}

// Получено новое сообщение по UART
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  // Определяем, прерывание вызвано остановкой считывания?
  if (__HAL_UART_GET_FLAG(uart, UART_FLAG_IDLE))
  {
    __HAL_UART_CLEAR_IDLEFLAG(uart);
    uint16_t len, start = uartRxPos;
    // Определяем новую позицию
    uartRxPos = UART_RX_DATA_SIZE - __HAL_DMA_GET_COUNTER(uart->hdmarx);
    // Определяем размер сообщения
    if (uartRxIsRollover)
    {
      if (uartRxPos <= start)
        len = UART_RX_DATA_SIZE + uartRxPos - start;
      else
        len = UART_RX_DATA_SIZE + 1; // Признак ошибки
    }
    else
    {
      len = uartRxPos - start;
    }
    // Перезаписываем данные в uartRxData
    if (len)
    {
      if (len && len <= UART_RX_DATA_SIZE)
      {
        uint16_t i;
        for (i = 0; i < len; i++)
        {
          if (uartRxBuffer[start] != '\0')
            uartRxData[i] = uartRxBuffer[start];
          else
            uartRxData[i] = ' ';
          if (start < UART_RX_DATA_SIZE)
            start++;
          else
            start = 0;
        }
        uartRxDataSize = i;
      }
      else
      {
        // Если не было ошибки кадра
        if (!isErrUARTFE)
          isErrRxBufUART = 1;  // Подать сигнал о переполнении
      }
    }
    uartRxIsReady = 1;        // Оповещение о приеме данных
    uartRxIsRollover = 0;
  }
  else
  {
    // Прерывание вызвано сбросом DMA
    // (Переполнения буфера и переход к его началу)
    uartRxIsRollover = 1;
  }
}

// Произошло обновление таймера
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
  if (htim->Instance == tim->Instance)
    timer++;
}

// На устройстве UART произошла ошибка
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  // Есть ошибка кадра
  if (huart->ErrorCode == HAL_UART_ERROR_FE)
    isErrUARTFE = 1;
}
