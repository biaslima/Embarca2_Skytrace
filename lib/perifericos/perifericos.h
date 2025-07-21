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

#define BUTTON_PIN_A 5   
#define BUTTON_PIN_B 6  

// Display
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define endereco 0x3C

extern float offset_temp, offset_pressao, offset_umidade; 

#define NUM_LEDS 25
#define IS_RGBW false
#define led_matrix_pin 7
#define BUZZER_PIN 21

// Declaração de variáveis externas
extern uint32_t leds[NUM_LEDS];
extern PIO pio;
extern int sm;

//Protótipos de função
void gpio_led_bitdog(void);
void atualiza_rgb_led();

void buzzer_init(int pin);
void tocar_frequencia(int frequencia, int duracao_ms);
void buzzer_desliga(int pin);
void buzzer_liga(int pin);

void matrix_init(PIO pio_inst, uint sm_num, uint pin);
uint8_t matriz_posicao_xy(uint8_t x, uint8_t y);
uint32_t create_color(uint8_t green, uint8_t red, uint8_t blue);
void clear_matrix(PIO pio_inst, uint sm_num);
void update_leds(PIO pio_inst, uint sm_num);
void exibir_padrao();

#endif
