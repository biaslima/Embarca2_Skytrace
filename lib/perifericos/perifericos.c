#include <stdio.h>               
#include <string.h>              
#include <stdlib.h>  
#include "perifericos.h"

#include "pico/time.h"
#include "hardware/pwm.h" 
#include "hardware/pio.h"
#include "ws2812.pio.h"           

//Variaveis globais
static int buzzer_pin;
static uint slice_num;
static uint channel;

uint32_t leds[NUM_LEDS];
PIO pio = pio0;
int sm = 0;
bool leds_ativos = true; 

//Inicia os LEDs RGB
void gpio_led_bitdog(void) {
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);

    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, false);

    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, false);
}

//Atualizar LED RGB de acordo com modo
void atualiza_rgb_led() {
    if (alerta_temperatura == true)
    {
        gpio_put(LED_RED_PIN, true);
    } else {
        gpio_put(LED_RED_PIN, false);
    } if (alerta_umidade == true) {
        gpio_put(LED_BLUE_PIN, true);
    } else{
        gpio_put(LED_BLUE_PIN, false);
    }if (alerta_pressao == true){
        gpio_put(LED_GREEN_PIN, true);
    } else {
        gpio_put(LED_GREEN_PIN, false);
    }
}
//Inicializa o buzzer
void buzzer_init(int pin) {
    buzzer_pin = pin;

    gpio_set_function(buzzer_pin, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(buzzer_pin);
    channel = pwm_gpio_to_channel(buzzer_pin);
    
    pwm_set_clkdiv(slice_num, 125.0);  
    pwm_set_wrap(slice_num, 1000);     
    pwm_set_chan_level(slice_num, channel, 500); 
    
    printf("Buzzer inicializado", 
           pin, slice_num, channel);
}

// Liga o buzzer 
void buzzer_liga(int pin) {
    uint32_t clock = 125000000;
    uint32_t freq = 440;
    
    //PWM
    uint32_t wrap = clock / freq / 2;
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, PWM_CHAN_A, wrap / 2);
}

//Toca uma frequencia específica
void tocar_frequencia(int frequencia, int duracao_ms) {
    // Fórmula: wrap = 1_000_000 / frequência
    uint32_t wrap = 1000000 / frequencia;
    
    printf("Tocando frequência %d Hz (wrap=%d)\n", frequencia, wrap);
    
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, channel, wrap / 2);
    pwm_set_enabled(slice_num, true);
    
    sleep_ms(duracao_ms);
    
    pwm_set_enabled(slice_num, false);
}

//Inicializa a matriz
void matrix_init(PIO pio_inst, uint sm_num, uint pin) {
    pio = pio_inst;
    sm = sm_num;
    
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    
    uint offset = pio_add_program(pio, &ws2812_program);
    printf("Loaded PIO program at offset %d\n", offset);
    ws2812_program_init(pio, sm, offset, pin, 800000, IS_RGBW);

    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = 0; 
    }
    clear_matrix(pio, sm);
    update_leds(pio, sm);
    
    printf("Matriz de LEDs inicializada com sucesso!\n");
}

//Função pra localizar LEDs 
uint8_t matriz_posicao_xy(uint8_t x, uint8_t y) {
    if (y % 2 == 0) {
        return y * 5 + x;
    } else {
        return y * 5 + (4 - x);
    }
}

// Função para criar uma cor
uint32_t create_color(uint8_t green, uint8_t red, uint8_t blue) {
    return ((uint32_t)green << 16) | ((uint32_t)red << 8) | blue;
}

// Limpa todos os LEDs
void clear_matrix(PIO pio_inst, uint sm_num) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = 0;
    }
}

//Atauliza os LEDs
void update_leds(PIO pio_inst, uint sm_num) {
    for (int i = 0; i < NUM_LEDS; i++) {
        pio_sm_put_blocking(pio_inst, sm_num, leds[i] << 8u);
    }
}

// Exibe padrão para modo específico
void exibir_padrao() {
    clear_matrix(pio, sm);

    uint8_t r = 0, g = 0, b = 0;

    if (alerta_temperatura) {
        r += 20; 
    }
    if (alerta_umidade) {
        b += 20; 
    }
    if (alerta_pressao) {
        g += 20; 
    }

    uint32_t cor_fundo = create_color(g, r, b);

    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = cor_fundo;
    }

    uint8_t altura = 5;
    uint8_t posicoes[][2] = {
        {2, 0},
        {2, 1},
        {2, 2},
        {2, 4}
    };
    if (alerta_pressao == true || alerta_temperatura == true || alerta_umidade == true) {
        for (int i = 0; i < 4; i++) {
        uint8_t x = posicoes[i][0];
        uint8_t y = altura - 1 - posicoes[i][1];
        uint8_t index = matriz_posicao_xy(x, y);
        leds[index] = create_color(40, 150, 60);  
        }
    } else { 
        clear_matrix(pio, sm);
    }
    

    update_leds(pio, sm);
}
