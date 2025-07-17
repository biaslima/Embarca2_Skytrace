#ifndef MATRIZ_LED_H
#define MATRIZ_LED_H

#include "hardware/pio.h"


#define NUM_LEDS 25
#define IS_RGBW false
#define led_matrix_pin 7

// Declaração de variáveis externas
extern uint32_t leds[NUM_LEDS];
extern PIO pio;
extern int sm;


// Protótipos de funções
void matrix_init(PIO pio_inst, uint sm_num, uint pin);
uint8_t matriz_posicao_xy(uint8_t x, uint8_t y);
uint32_t create_color(uint8_t green, uint8_t red, uint8_t blue);
void clear_matrix(PIO pio_inst, uint sm_num);
void update_leds(PIO pio_inst, uint sm_num);
void exibir_padrao(uint8_t padrao);


#endif