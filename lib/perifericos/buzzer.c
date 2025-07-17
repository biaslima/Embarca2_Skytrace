#include "buzzer.h"
#include "hardware/pwm.h"
#include <stdio.h>               
#include <string.h>              
#include <stdlib.h>   


//Variaveis globais
static int buzzer_pin;
static uint slice_num;
static uint channel;
bool alarme_ativo = false;

//Notas para musiquinha
#define NOTE_C4  261
#define NOTE_D4  294
#define NOTE_E4  329
#define NOTE_G4  392
#define NOTE_A4  440

int melodia[] = { NOTE_C4, NOTE_D4, NOTE_E4, NOTE_C4 }; //array para fazer melodia
int duracoes[] = { 300, 300, 300, 600 }; //duração de cada nota
const int tamanho_musica = sizeof(melodia) / sizeof(melodia[0]);

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

//Desliga o buzzer
void buzzer_desliga(int pin){
    pwm_set_enabled(slice_num, false);
    gpio_put(pin, 0);
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

//Altera o estado da variável de alarme para true
void tocar_alarme(){
    alarme_ativo = true;
}

//Altera o estado da variável de alarme para false
void desligar_alarme() {
    alarme_ativo = false;
    buzzer_desliga(buzzer_pin); 
}

//Deixa o som do alarme intermitente
void alarme_loop() {
    static uint32_t ultima_execucao = 0;
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    if (!alarme_ativo) return;

    // A cada 400ms, toca um bipe de 200ms
    if (current_time - ultima_execucao >= 600) {
        tocar_frequencia(600, 600); // 1000 Hz por 200 ms
        ultima_execucao = current_time;
    }
}

//Toca uma nota da música
void buzzer_toca_nota(uint freq) {
    uint32_t wrap = 1000000 / freq;
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, channel, wrap / 2);
    pwm_set_enabled(slice_num, true);
}

//Para a nota na música
void buzzer_para_nota() {
    pwm_set_enabled(slice_num, false);
}
