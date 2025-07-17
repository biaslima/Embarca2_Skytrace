#ifndef BUZZER_H
#define BUZZER_H

#include "pico/stdlib.h"

extern bool alarme_ativo;

void buzzer_init(int pin);
void tocar_frequencia(int frequencia, int duracao_ms);
void buzzer_desliga(int pin);
void buzzer_liga(int pin);
void tocar_alarme();
void desligar_alarme();
void alarme_loop();
void musica_festa_loop();

#endif