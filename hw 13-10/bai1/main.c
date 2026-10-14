#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* --- Ð?NH NGHIA PH?N C?NG --- */
// 3 Kênh PWM c?a TIM2 (PA0, PA1, PA2)
#define LED1_PIN        GPIO_Pin_0
#define LED2_PIN        GPIO_Pin_1
#define LED3_PIN        GPIO_Pin_2
#define LED_PORT        GPIOA

// 3 Nút nh?n d?i sang PA3, PA4, PA5
#define BTN_MODE_PIN    GPIO_Pin_3  // Nút chuy?n LED
#define BTN_UP_PIN      GPIO_Pin_4  // Nút tang d? sáng
#define BTN_DOWN_PIN    GPIO_Pin_5  // Nút gi?m d? sáng
#define BTN_PORT        GPIOA       // Ð?i sang GPIOA luôn cho d?ng b?

/* --- C?U TRÚC D? LI?U C?A QUEUE --- */
typedef struct {
    uint8_t led_id;  // 0: LED1, 1: LED2, 2: LED3
    int8_t  step;    // +100 (Tang sáng) ho?c -100 (Gi?m sáng)
} Message_t;

/* --- KHAI BÁO HANDLE --- */
QueueHandle_t xLedQueue;
TaskHandle_t xTaskBtnHandle;
TaskHandle_t xTaskLed1Handle;
TaskHandle_t xTaskLed2Handle;
TaskHandle_t xTaskLed3Handle;

volatile uint8_t current_led = 0;

/* --- HÀM C?U HÌNH PH?N C?NG --- */
void GPIO_PWM_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    // B?t Clock cho GPIOA và TIM2 (không c?n GPIOB n?a n?u t?t c? n?m ? GPIOA)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // C?u hình Chân PA0, PA1, PA2 làm AF_PP (PWM) và PA3, PA4, PA5 làm Input Pull-Up (Nút nh?n)
    GPIO_InitStructure.GPIO_Pin = LED1_PIN | LED2_PIN | LED3_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_PORT, &GPIO_InitStructure);

    // C?u hình Nút nh?n PA3, PA4, PA5 làm Input Pull-Up
    GPIO_InitStructure.GPIO_Pin = BTN_MODE_PIN | BTN_UP_PIN | BTN_DOWN_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(BTN_PORT, &GPIO_InitStructure);

    // C?u hình TIM2 t?o xung 1kHz
    TIM_TimeBaseStructure.TIM_Period = 999;             // ARR = 999
    TIM_TimeBaseStructure.TIM_Prescaler = 71;           // PSC = 71
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    // C?u hình Ch? d? PWM cho Channel 1, 2, 3
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_Pulse = 0;                  // Ban d?u t?t h?t LED

    TIM_OC1Init(TIM2, &TIM_OCInitStructure);
    TIM_OC2Init(TIM2, &TIM_OCInitStructure);
    TIM_OC3Init(TIM2, &TIM_OCInitStructure);

    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_OC3PreloadConfig(TIM2, TIM_OCPreload_Enable);

    TIM_Cmd(TIM2, ENABLE);
}

/* --- TASK Ð?C NÚT NH?N --- */
void vTaskButton(void *pvParameters)
{
    Message_t msg;

    for (;;)
    {
        // 1. Nút Chuy?n LED (PA3)
        if (GPIO_ReadInputDataBit(BTN_PORT, BTN_MODE_PIN) == Bit_RESET)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (GPIO_ReadInputDataBit(BTN_PORT, BTN_MODE_PIN) == Bit_RESET)
            {
                current_led = (current_led + 1) % 3;
                while (GPIO_ReadInputDataBit(BTN_PORT, BTN_MODE_PIN) == Bit_RESET)
                {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
        }

        // 2. Nút Tang Ð? Sáng (PA4)
        if (GPIO_ReadInputDataBit(BTN_PORT, BTN_UP_PIN) == Bit_RESET)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (GPIO_ReadInputDataBit(BTN_PORT, BTN_UP_PIN) == Bit_RESET)
            {
                msg.led_id = current_led;
                msg.step = 100;
                xQueueSend(xLedQueue, &msg, portMAX_DELAY);
                while (GPIO_ReadInputDataBit(BTN_PORT, BTN_UP_PIN) == Bit_RESET)
                {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
        }

        // 3. Nút Gi?m Ð? Sáng (PA5)
        if (GPIO_ReadInputDataBit(BTN_PORT, BTN_DOWN_PIN) == Bit_RESET)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (GPIO_ReadInputDataBit(BTN_PORT, BTN_DOWN_PIN) == Bit_RESET)
            {
                msg.led_id = current_led;
                msg.step = -100;
                xQueueSend(xLedQueue, &msg, portMAX_DELAY);
                while (GPIO_ReadInputDataBit(BTN_PORT, BTN_DOWN_PIN) == Bit_RESET)
                {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* --- CÁC TASK LED (GI? NGUYÊN NHU CU) --- */
void vTaskLed1(void *pvParameters)
{
    Message_t rx_msg;
    int16_t duty = 0;
    for (;;)
    {
        if (xQueueReceive(xLedQueue, &rx_msg, portMAX_DELAY) == pdTRUE)
        {
            if (rx_msg.led_id == 0)
            {
                duty += rx_msg.step;
                if (duty > 999) duty = 999;
                if (duty < 0)   duty = 0;
                TIM_SetCompare1(TIM2, (uint16_t)duty);
            }
            else
            {
                xQueueSendToFront(xLedQueue, &rx_msg, 0);
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
    }
}

void vTaskLed2(void *pvParameters)
{
    Message_t rx_msg;
    int16_t duty = 0;
    for (;;)
    {
        if (xQueueReceive(xLedQueue, &rx_msg, portMAX_DELAY) == pdTRUE)
        {
            if (rx_msg.led_id == 1)
            {
                duty += rx_msg.step;
                if (duty > 999) duty = 999;
                if (duty < 0)   duty = 0;
                TIM_SetCompare2(TIM2, (uint16_t)duty);
            }
            else
            {
                xQueueSendToFront(xLedQueue, &rx_msg, 0);
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
    }
}

void vTaskLed3(void *pvParameters)
{
    Message_t rx_msg;
    int16_t duty = 0;
    for (;;)
    {
        if (xQueueReceive(xLedQueue, &rx_msg, portMAX_DELAY) == pdTRUE)
        {
            if (rx_msg.led_id == 2)
            {
                duty += rx_msg.step;
                if (duty > 999) duty = 999;
                if (duty < 0)   duty = 0;
                TIM_SetCompare3(TIM2, (uint16_t)duty);
            }
            else
            {
                xQueueSendToFront(xLedQueue, &rx_msg, 0);
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
    }
}

/* --- HÀM MAIN --- */
int main(void)
{
    SystemInit();
    GPIO_PWM_Init();

    xLedQueue = xQueueCreate(10, sizeof(Message_t));

    if (xLedQueue != NULL)
    {
        xTaskCreate(vTaskButton, "BtnTask", configMINIMAL_STACK_SIZE, NULL, 2, &xTaskBtnHandle);
        xTaskCreate(vTaskLed1, "LED1Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskLed1Handle);
        xTaskCreate(vTaskLed2, "LED2Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskLed2Handle);
        xTaskCreate(vTaskLed3, "LED3Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskLed3Handle);

        vTaskStartScheduler();
    }

    while (1);
}