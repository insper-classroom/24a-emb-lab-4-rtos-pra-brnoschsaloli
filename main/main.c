#include <stdio.h>
#include "pico/stdlib.h"
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

SemaphoreHandle_t xSemaphore_t;

QueueHandle_t xQueueTime;
QueueHandle_t xQueueDistance;

const int trig_pin = 12;
const int echo_pin = 13;


void pin_callback(uint gpio, uint32_t events) {
    int time = 0;
    if (events == 0x4) {
        time = to_us_since_boot(get_absolute_time());
    } else if (events == 0x8) {
        time = to_us_since_boot(get_absolute_time());
    }
    xQueueSendFromISR(xQueueTime, &time, 0);

}

bool timer_0_callback(repeating_timer_t *rt) {
    xSemaphoreGiveFromISR(xSemaphore_t, 0);
    return true; // keep repeating
}

void trigger_task(void *p){
    gpio_init(trig_pin);
    gpio_set_dir(trig_pin, GPIO_OUT);

    repeating_timer_t timer_0;

    if (!add_repeating_timer_ms(1000, 
                                timer_0_callback,
                                NULL, 
                                &timer_0)) {
        printf("Failed to add timer\n");
    }



    while (1) {

        if (xSemaphoreTake(xSemaphore_t, pdMS_TO_TICKS(1000)) == pdTRUE) {
            gpio_put(trig_pin, 1);
            vTaskDelay(pdMS_TO_TICKS(10));
            gpio_put(trig_pin, 0);
        }
    }
}

void falha(){

    ssd1306_init();
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);
    gfx_clear_buffer(&disp);
    gfx_draw_string(&disp, 0, 0, 2, "FALHA");
    gfx_show(&disp);

}

void echo_task(void *p) {
    gpio_init(echo_pin);
    gpio_set_dir(echo_pin, GPIO_IN);
    gpio_set_irq_enabled_with_callback(echo_pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);


    int start_time;
    int end_time;
    int c = 0;

    while (true) {

        if (c == 0){
            if (xQueueReceive(xQueueTime, &start_time, pdMS_TO_TICKS(1000))) {
                c = 1;
            } else {
                falha();
            }
        } else {
            if (xQueueReceive(xQueueTime, &end_time, pdMS_TO_TICKS(1000))){
                c = 0;
                int pulse_duration_us = (end_time - start_time);
                int distance_cm = pulse_duration_us * 0.03403 / 2;
                xQueueSend(xQueueDistance, &distance_cm, 0);
            }else {
                falha();
            }
        }
    }
}

void oled_task(void *p){

    ssd1306_init();
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    int dist;

    while (1) {
        if (xQueueReceive(xQueueDistance, &dist,  pdMS_TO_TICKS(100))) {
            gfx_clear_buffer(&disp);
            char dist_str[10]; // String temporária para armazenar o valor de dist
            sprintf(dist_str, "%d", dist); // Convertendo dist para uma string
            if (dist > 0){
                gfx_draw_string(&disp, 0, 0, 1, "Dist: ");
                gfx_draw_string(&disp, 30, 0, 1, dist_str); // Exibindo o valor de dist
                gfx_draw_string(&disp, 50, 0, 1, "cm");
            } else if (dist < 0){
                gfx_draw_string(&disp, 0, 0, 2, "n/d");
            }

            gfx_draw_line(&disp, 15, 27, dist + 40,
                          27);
            gfx_show(&disp);
        }
    }

}

int main() {
    stdio_init_all();

    xSemaphore_t = xSemaphoreCreateBinary();

    xQueueTime = xQueueCreate(32, sizeof(int) );
    xQueueDistance = xQueueCreate(32, sizeof(int) );



    xTaskCreate(echo_task, "EchoTask", 4095, NULL, 1, NULL);
    xTaskCreate(trigger_task, "TriggerTask", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "OledTask", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    return 0;
}
