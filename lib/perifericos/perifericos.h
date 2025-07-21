#ifndef PERIFERICOS_H
#define PERIFERICOS_H

#include "pico/stdlib.h"
#include "hardware/pio.h"

#define LED_PIN CYW43_WL_GPIO_LED_PIN  
#define LED_BLUE_PIN 12                 
#define LED_GREEN_PIN 11                
#define LED_RED_PIN 13
#define BUZZER_PIN 21           
#define LED_MATRIX_PIN 7


extern bool alerta_temperatura;
extern bool alerta_umidade;
extern bool alerta_pressao; 

/*
#define NUM_LEDS 25
#define IS_RGBW false
#define led_matrix_pin 7
#define BUZZER_PIN 21

// Declaração de variáveis externas
extern uint32_t leds[NUM_LEDS];
extern PIO pio;
extern int sm;
*/
void gpio_led_bitdog(void);
void atualiza_rgb_led();

void buzzer_init(int pin);
void tocar_frequencia(int frequencia, int duracao_ms);
void buzzer_desliga(int pin);
void buzzer_liga(int pin);

/*
// Protótipos de funções
void matrix_init(PIO pio_inst, uint sm_num, uint pin);
uint8_t matriz_posicao_xy(uint8_t x, uint8_t y);
uint32_t create_color(uint8_t green, uint8_t red, uint8_t blue);
void clear_matrix(PIO pio_inst, uint sm_num);
void update_leds(PIO pio_inst, uint sm_num);
void exibir_padrao(uint8_t padrao);

*/
#endif
