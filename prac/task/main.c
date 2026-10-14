#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"

#include "FreeRTOS.h"
#include "task.h"

typedef struct {
    GPIO_TypeDef* GPIOx; 
    uint16_t GPIO_Pin;    
    float frequency;        
} LED_Config_t;

LED_Config_t led1_config = {GPIOA, GPIO_Pin_0, 0.1f};  // PA0, 0.1 Hz
LED_Config_t led2_config = {GPIOA, GPIO_Pin_1, 1.0f};  // PA1, 1 Hz
LED_Config_t led3_config = {GPIOA, GPIO_Pin_2, 10.0f}; // PA2, 10 Hz

void GPIO_Configuration(void);
void vLedBlinkTask(void *pvParameters);

int main(void)
{
    GPIO_Configuration();

    xTaskCreate(vLedBlinkTask, "LED_0.1Hz", configMINIMAL_STACK_SIZE, (void*)&led1_config, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vLedBlinkTask, "LED_1Hz",   configMINIMAL_STACK_SIZE, (void*)&led2_config, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vLedBlinkTask, "LED_10Hz",  configMINIMAL_STACK_SIZE, (void*)&led3_config, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();

    while (1);
}

void vLedBlinkTask(void *pvParameters)
{
    LED_Config_t *pConfig = (LED_Config_t *)pvParameters;

    uint32_t delay_ms = (uint32_t)(1000.0f / (2.0f * pConfig->frequency));

    TickType_t xDelayTicks = pdMS_TO_TICKS(delay_ms);

    for (;;)
    {
        pConfig->GPIOx->ODR ^= pConfig->GPIO_Pin;

        vTaskDelay(xDelayTicks);
    }
}

void GPIO_Configuration(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}
