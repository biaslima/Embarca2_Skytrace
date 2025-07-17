#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h" 

#include <math.h>
#include "hardware/i2c.h"

#include "lib/sensores/aht20.h"
#include "lib/sensores/bmp280.h"
#include "lib/perifericos/buzzer.h"
#include "lib/perifericos/font.h"
#include "lib/perifericos/matriz_led.h"
#include "lib/perifericos/ssd1306.h"
#include "webserver.h" 
#include "wifi_secrets.h"

#define I2C_PORT i2c0               // i2c0 pinos 0 e 1, i2c1 pinos 2 e 3
#define I2C_SDA 0                   // 0 ou 2
#define I2C_SCL 1                   // 1 ou 3
#define SEA_LEVEL_PRESSURE 101325.0 // Pressão ao nível do mar em Pa
// Display na I2C
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define endereco 0x3C

// Limites para os sensores
volatile float lim_min_temp = 20.0;
volatile float lim_max_temp = 30.0;
volatile float lim_min_pressao = 95.0;
volatile float lim_max_pressao = 105.0;
volatile float lim_min_umi = 40.0;
volatile float lim_max_umi = 60.0;

// Variáveis para os valores atuais dos sensores (para o webserver)
float temperatura_atual = 0.0;
float pressao_atual = 0.0;
float umidade_atual = 0.0;

// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

int main()
{
    // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
   // Fim do trecho para modo BOOTSEL com botão B

    stdio_init_all();

    // I2C do Display funcionando em 400Khz.
    i2c_init(I2C_PORT_DISP, 400 * 1000);

    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    // Limpa o display
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicializa o I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa o BMP280
    bmp280_init(I2C_PORT);
    struct bmp280_calib_param params;
    bmp280_get_calib_params(I2C_PORT, &params);

    // Inicializa o AHT20
    aht20_reset(I2C_PORT);
    aht20_init(I2C_PORT);

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
    sleep_ms(3000); // Mostra o IP por 3 segundos

    // Estrutura para armazenar os dados do sensor
    AHT20_Data data;
    int32_t raw_temp_bmp;
    int32_t raw_pressure;

    char str_pa[8];
    char str_tmp2[8];
    char str_umi[8];

    bool cor = true;
    while (1)
    {
        cyw43_arch_poll();
        
        // Leitura do BMP280
        bmp280_read_raw(I2C_PORT, &raw_temp_bmp, &raw_pressure);
        int32_t temperature = bmp280_convert_temp(raw_temp_bmp, &params);
        int32_t pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);

        // Leitura do AHT20
        if (aht20_read(I2C_PORT, &data)) {
            // Atualiza as variáveis globais para o webserver
            temperatura_atual = data.temperature;
            pressao_atual = pressure / 1000.0; // Converte para kPa
            umidade_atual = data.humidity;
            
            printf("Pressao = %.1f kPa\n", pressao_atual);
            printf("Temperatura BMP: = %.1f C\n", temperature / 100.0);
            printf("Temperatura AHT: %.1f C\n", temperatura_atual);
            printf("Umidade: %.1f %%\n", umidade_atual);
            
            // Verificação dos limites
            if (temperatura_atual < lim_min_temp || temperatura_atual > lim_max_temp) {
                printf("ALERTA: Temperatura fora dos limites! (%.1f - %.1f)\n", lim_min_temp, lim_max_temp);
            }
            if (pressao_atual < lim_min_pressao || pressao_atual > lim_max_pressao) {
                printf("ALERTA: Pressão fora dos limites! (%.1f - %.1f)\n", lim_min_pressao, lim_max_pressao);
            }
            if (umidade_atual < lim_min_umi || umidade_atual > lim_max_umi) {
                printf("ALERTA: Umidade fora dos limites! (%.1f - %.1f)\n", lim_min_umi, lim_max_umi);
            }
        }
        else {
            printf("Erro na leitura do AHT20!\n");
        }

        sprintf(str_pa, "%.1fkPa", pressao_atual);
        sprintf(str_tmp2, "%.1fC", temperatura_atual);
        sprintf(str_umi, "%.1f%%", umidade_atual);
    
        // Atualiza o display
        ssd1306_fill(&ssd, !cor);
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);
        ssd1306_line(&ssd, 3, 13, 123, 13, cor);   
        ssd1306_line(&ssd, 3, 25, 123, 25, cor);
        ssd1306_line(&ssd, 3, 37, 123, 37, cor);
        ssd1306_line(&ssd, 3, 49, 123, 49, cor);    
        ssd1306_line(&ssd, 3, 61, 123, 61, cor);    
        ssd1306_draw_string(&ssd, "SKYTRACE", 32, 6);
        ssd1306_draw_string(&ssd, "WIFI STATUS: ", 7, 16);
        ssd1306_draw_string(&ssd, "PRESS: ", 7, 28);
        ssd1306_draw_string(&ssd, str_pa, 60, 28);  
        ssd1306_draw_string(&ssd, "TEMP: ", 7, 40); 
        ssd1306_draw_string(&ssd, str_tmp2, 60, 40);
        ssd1306_draw_string(&ssd, "HUM:  ", 7, 52);
        ssd1306_draw_string(&ssd, str_umi, 60, 52);
        ssd1306_send_data(&ssd);

        sleep_ms(500);
    }

    return 0;
}