#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include <math.h>
#include "hardware/i2c.h"

#include "lib/sensores/aht20.h"
#include "lib/sensores/bmp280.h"
#include "lib/perifericos/perifericos.h"
#include "lib/perifericos/font.h"
#include "lib/perifericos/ssd1306.h"
#include "webserver.h"

#define I2C_PORT i2c0               // i2c0 pinos 0 e 1, i2c1 pinos 2 e 3
#define I2C_SDA 0                   // 0 ou 2
#define I2C_SCL 1                   // 1 ou 3
#define SEA_LEVEL_PRESSURE 101325.0 // Pressão ao nível do mar em Pa

// Limites para os sensores
volatile float lim_min_temp = -10.0;
volatile float lim_max_temp = 60.0;
volatile float lim_min_pressao = 90.0;
volatile float lim_max_pressao = 107.0;
volatile float lim_min_umi = 0.0;
volatile float lim_max_umi = 100.0;

// Variáveis para os valores atuais dos sensores 
float temperatura_atual = 0.0;
float pressao_atual = 0.0;
float umidade_atual = 0.0;

bool alerta_temperatura = false;
bool alerta_umidade = false;
bool alerta_pressao = false;

// Variáveis pra controle da exibição de configurações
volatile uint32_t display_config_until = 0; 

// Variáveis globais para debounce
uint32_t last_interrupt_time_A = 0;
uint32_t last_interrupt_time_B = 0;

static void buttons_callback(uint gpio, uint32_t events) {
    uint32_t current_time_us = to_us_since_boot(get_absolute_time());
    const uint32_t DEBOUNCE_DELAY_US = 200000; // 200ms

    // Botão A - Reset de Configurações
    if (gpio == BUTTON_PIN_A && (current_time_us - last_interrupt_time_A > DEBOUNCE_DELAY_US)) {
        last_interrupt_time_A = current_time_us;
        lim_min_temp = -10.0;
        lim_max_temp = 60.0;
        lim_min_pressao = 90.0;
        lim_max_pressao = 107.0;
        lim_min_umi = 0.0;
        lim_max_umi = 100.0;
        offset_temp = 0.0;
        offset_pressao = 0.0;
        offset_umidade = 0.0;
        printf("Configuracoes resetadas via Botao A.\n");

        alerta_temperatura = false;
        alerta_umidade = false;
        alerta_pressao = false;

        exibir_padrao(); 
        atualiza_rgb_led();

    }
    //Botão B -- Exibição de limites atuais
    else if (gpio == BUTTON_PIN_B && (current_time_us - last_interrupt_time_B > DEBOUNCE_DELAY_US)) {
        last_interrupt_time_B = current_time_us;
        display_config_until = to_ms_since_boot(get_absolute_time()) + 5000; // Exibe por 5 segundos
        printf("Botao B pressionado. Exibindo configuracoes por 5s.\n");
    }
}

int main()
{
    stdio_init_all();
    gpio_led_bitdog();
    buzzer_init(BUZZER_PIN);
    matrix_init(pio0, 0, LED_MATRIX_PIN);
    clear_matrix(pio0, 0);
    update_leds(pio0, 0); 

    // I2C do Display
    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicializa o I2C para sensores
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa BMP280 e AHT20
    bmp280_init(I2C_PORT);
    struct bmp280_calib_param params;
    bmp280_get_calib_params(I2C_PORT, &params);
    aht20_reset(I2C_PORT);
    aht20_init(I2C_PORT);

    //Interrupções
    gpio_set_irq_enabled_with_callback(BUTTON_PIN_A, GPIO_IRQ_EDGE_FALL, true, buttons_callback);
    gpio_set_irq_enabled_with_callback(BUTTON_PIN_B, GPIO_IRQ_EDGE_FALL, true, buttons_callback);


    // Conexão Wi-Fi e servidor
    ssd1306_draw_string(&ssd, "Conectando WiFi", 6, 22);
    ssd1306_send_data(&ssd);

    if (!webserver_init()) {
        printf("Falha ao iniciar o servidor web.\n");
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi: FALHA", 8, 22);
        ssd1306_send_data(&ssd);
        while(true);
    }

    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    char ip_str[24];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    printf("IP: %s\n", ip_str);

    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "IP:", 8, 6);
    ssd1306_draw_string(&ssd, ip_str, 8, 22);
    ssd1306_send_data(&ssd);
    sleep_ms(3000); 

    // Estrutura para armazenar os dados do sensor
    AHT20_Data data;
    int32_t raw_temp_bmp;
    int32_t raw_pressure;

    char str_pa[12];
    char str_tmp2[12];
    char str_umi[12];
    char line_buffer[20]; 

    while (1)
    {
        cyw43_arch_poll();

        bmp280_read_raw(I2C_PORT, &raw_temp_bmp, &raw_pressure);
        int32_t temperature = bmp280_convert_temp(raw_temp_bmp, &params);
        int32_t pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);

        if (aht20_read(I2C_PORT, &data)) {
            temperatura_atual = data.temperature;
            pressao_atual = pressure / 1000.0; 
            umidade_atual = data.humidity;

            printf("Pressao = %.1f kPa\n", pressao_atual + offset_pressao);
            printf("Temperatura BMP: = %.1f C\n", temperature / 100.0 + offset_temp);
            printf("Temperatura AHT: %.1f C\n", temperatura_atual + offset_temp);
            printf("Umidade: %.1f %%\n", umidade_atual + offset_umidade);

            // Verificação dos limites 
            if ((temperatura_atual + offset_temp) < lim_min_temp || (temperatura_atual + offset_temp) > lim_max_temp) {
                printf("ALERTA: Temperatura fora dos limites! (%.1f - %.1f)\n", lim_min_temp, lim_max_temp);
                alerta_temperatura = true;
            } else { alerta_temperatura = false; } 
            
            if ((pressao_atual + offset_pressao) < lim_min_pressao || (pressao_atual + offset_pressao) > lim_max_pressao) {
                printf("ALERTA: Pressão fora dos limites! (%.1f - %.1f)\n", lim_min_pressao, lim_max_pressao);
                alerta_pressao = true;
            } else { alerta_pressao = false; } 

            if ((umidade_atual + offset_umidade) < lim_min_umi || (umidade_atual + offset_umidade) > lim_max_umi) {
                printf("ALERTA: Umidade fora dos limites! (%.1f - %.1f)\n", lim_min_umi, lim_max_umi);
                alerta_umidade = true;
            } else { alerta_umidade = false; } 
        }
        else {
            printf("Erro na leitura do AHT20!\n");
        }
        
        exibir_padrao();
        atualiza_rgb_led();


        ssd1306_fill(&ssd, false);

        if (to_ms_since_boot(get_absolute_time()) < display_config_until) {

            ssd1306_draw_string(&ssd, "CONFIGS ATUAIS:", 5, 0); // Título

            snprintf(line_buffer, sizeof(line_buffer), "T:%.1f/%.1fC", lim_min_temp, lim_max_temp);
            ssd1306_draw_string(&ssd, line_buffer, 5, 12);

            snprintf(line_buffer, sizeof(line_buffer), "P:%.1f/%.1fkPa", lim_min_pressao, lim_max_pressao);
            ssd1306_draw_string(&ssd, line_buffer, 5, 24);

            snprintf(line_buffer, sizeof(line_buffer), "U:%.1f/%.1f%%", lim_min_umi, lim_max_umi);
            ssd1306_draw_string(&ssd, line_buffer, 5, 36);

        } else {
            sprintf(str_pa, "%.1fkPa", pressao_atual);
            sprintf(str_tmp2, "%.1fC", temperatura_atual);
            sprintf(str_umi, "%.1f%%", umidade_atual);

            ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);
            ssd1306_line(&ssd, 3, 13, 123, 13, true);
            ssd1306_line(&ssd, 3, 25, 123, 25, true);
            ssd1306_line(&ssd, 3, 37, 123, 37, true);
            ssd1306_line(&ssd, 3, 49, 123, 49, true);
            ssd1306_line(&ssd, 3, 61, 123, 61, true);
            ssd1306_draw_string(&ssd, "SKYTRACE", 32, 6);
            ssd1306_draw_string(&ssd, "SENSORES:", 28, 16);
            ssd1306_draw_string(&ssd, "PRESS: ", 7, 28);
            ssd1306_draw_string(&ssd, str_pa, 60, 28);
            ssd1306_draw_string(&ssd, "TEMP: ", 7, 40);
            ssd1306_draw_string(&ssd, str_tmp2, 60, 40);
            ssd1306_draw_string(&ssd, "HUM:  ", 7, 52);
            ssd1306_draw_string(&ssd, str_umi, 60, 52);
        }

        ssd1306_send_data(&ssd); 
        sleep_ms(500); 
    }
    return 0;
}