#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>

/* --- Ð?NH NGHIA PH?N C?NG --- */
// Nút nh?n ng?t ngoài: PA0 (EXTI0)
#define BTN_EXTI_PIN        GPIO_Pin_0
#define BTN_EXTI_PORT       GPIOA
#define BTN_EXTI_LINE       EXTI_Line0
#define BTN_IRQn            EXTI0_IRQn

// UART1: PA9 (TX), PA10 (RX)
#define UART_TX_PIN         GPIO_Pin_9
#define UART_RX_PIN         GPIO_Pin_10
#define UART_GPIO           GPIOA

/* --- KHAI BÁO HANDLE --- */
QueueHandle_t xCountQueue;
TaskHandle_t xTaskUartHandle;

// Bi?n d?m s? l?n nh?n nút
volatile uint32_t press_count = 0;

/* --- C?U HÌNH PH?N C?NG (GPIO, EXTI, UART) --- */
void Hardware_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // 1. B?t Clock cho GPIOA, AFIO, UART1
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO | RCC_APB2Periph_USART1, ENABLE);

    // 2. C?u hình Nút nh?n PA0 làm Input Pull-Up
    GPIO_InitStructure.GPIO_Pin = BTN_EXTI_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(BTN_EXTI_PORT, &GPIO_InitStructure);

    // 3. C?u hình EXTI Line0 cho PA0 (C?nh xu?ng)
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);
    EXTI_InitStructure.EXTI_Line = BTN_EXTI_LINE;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    // 4. C?u hình NVIC cho EXTI0
    NVIC_InitStructure.NVIC_IRQChannel = BTN_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5; // Ð? uu tiên ng?t th?p hon FreeRTOS configMAX_SYSCALL_INTERRUPT_PRIORITY
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 5. C?u hình UART1 (TX: PA9, RX: PA10, Baudrate: 115200)
    GPIO_InitStructure.GPIO_Pin = UART_TX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(UART_GPIO, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = UART_RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(UART_GPIO, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
}

/* --- HÀM TRUY?N 1 KÝ T? QUA UART1 --- */
void UART1_SendChar(char c)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, (uint16_t)c);
}

/* --- HÀM TRUY?N CHU?I QUA UART1 --- */
void UART1_SendString(char *str)
{
    while (*str)
    {
        UART1_SendChar(*str++);
    }
}

/* --- HÀM PH?C V? NG?T NGOÀI EXTI0 (ISR) --- */
void EXTI0_IRQHandler(void)
{
    if (EXTI_GetITStatus(BTN_EXTI_LINE) != RESET)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        // 1. Tang bi?n d?m s? l?n nh?n nút[cite: 2]
        press_count++;

        // 2. G?i giá tr? s? d?m vào Queue t? ISR[cite: 2]
        xQueueSendFromISR(xCountQueue, &press_count, &xHigherPriorityTaskWoken);

        // 3. Xóa c? ng?t EXTI
        EXTI_ClearITPendingBit(BTN_EXTI_LINE);

        // 4. Chuy?n ng? c?nh ngay l?p t?c sang Task nh?n n?u có yêu c?u[cite: 2]
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* --- TASK TRUY?N D? LI?U UART --- */
void vTaskUART(void *pvParameters)
{
    uint32_t received_count = 0;
    char buffer[50];

    for (;;)
    {
        // 1. Ðua Task v? tr?ng thái Blocked d? ch? d? li?u t? Queue[cite: 2]
        if (xQueueReceive(xCountQueue, &received_count, portMAX_DELAY) == pdTRUE)
        {
            // 2. Nh?n giá tr? d?m và d?nh d?ng chu?i ký t? hi?n th?[cite: 2]
            sprintf(buffer, "So lan nhan nut: %lu\r\n", received_count);

            // 3. Truy?n chu?i ký t? qua UART lên Serial Terminal c?a máy tính[cite: 2]
            UART1_SendString(buffer);
        }
    }
}

/* --- HÀM MAIN --- */
int main(void)
{
    // Kh?i t?o h? th?ng và ph?n c?ng
    SystemInit();
    Hardware_Init();

    // T?o Queue ch?a t?i da 10 ph?n t? ki?u uint32_t
    xCountQueue = xQueueCreate(10, sizeof(uint32_t));

    if (xCountQueue != NULL)
    {
        // T?o Task UART
        xTaskCreate(vTaskUART, "UART_Task", configMINIMAL_STACK_SIZE, NULL, 2, &xTaskUartHandle);

        // B?t d?u b? l?p l?ch FreeRTOS
        vTaskStartScheduler();
    }

    while (1)
    {
        // Chuong trình không ch?y d?n dây n?u Scheduler ho?t d?ng bình thu?ng
    }
}