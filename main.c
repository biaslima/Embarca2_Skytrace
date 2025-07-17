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


#define I2C_PORT i2c0               // i2c0 pinos 0 e 1, i2c1 pinos 2 e 3
#define I2C_SDA 0                   // 0 ou 2
#define I2C_SCL 1                   // 1 ou 3
#define SEA_LEVEL_PRESSURE 101325.0 // Pressão ao nível do mar em Pa
// Display na I2C
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define endereco 0x3C

//Limite Mínimo e Máximo de Temperatura
volatile float lim_min_temp = 30.0;
volatile float lim_max_temp = 70.0;
volatile float lim_min_pressao = 30.0;
volatile float lim_max_pressao = 70.0;
volatile float lim_min_umi = 30.0;
volatile float lim_max_umi = 70.0;

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

    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA_DISP);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL_DISP);                                        // Pull up the clock line
    ssd1306_t ssd;                                                     // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT_DISP); // Inicializa o display
    ssd1306_config(&ssd);                                              // Configura o display
    ssd1306_send_data(&ssd);                                           // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
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

    // Estrutura para armazenar os dados do sensor
    AHT20_Data data;
    int32_t raw_temp_bmp;
    int32_t raw_pressure;

    char str_pa[5];  // Buffer para armazenar a string
    char str_alt[5];  // Buffer para armazenar a string  
    char str_tmp2[5];  // Buffer para armazenar a string
    char str_umi[5];  // Buffer para armazenar a string      

    bool cor = true;
    while (1)
    {
        // Leitura do BMP280
        bmp280_read_raw(I2C_PORT, &raw_temp_bmp, &raw_pressure);
        int32_t temperature = bmp280_convert_temp(raw_temp_bmp, &params);
        int32_t pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);

        printf("Pressao = %.3f kPa\n", pressure / 1000.0);
        printf("Temperatura BMP: = %.2f C\n", temperature / 100.0);

        // Leitura do AHT20
        if (aht20_read(I2C_PORT, &data))
        {
            printf("Temperatura AHT: %.2f C\n", data.temperature);
            printf("Umidade: %.2f %%\n\n\n", data.humidity);
        }
        else
        {
            printf("Erro na leitura do AHT10!\n\n\n");
        }


        sprintf(str_pa, "%.1f kPa", pressure / 1000.0);  // Converte o inteiro em string
        sprintf(str_tmp2, "%.1fC", data.temperature);  // Converte o inteiro em string
        sprintf(str_umi, "%.1f%%", data.humidity);  // Converte o inteiro em string        
    
        //  Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, !cor);                           // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);       // Desenha um retângulo
        ssd1306_line(&ssd, 3, 13, 123, 13, cor);   
        ssd1306_line(&ssd, 3, 25, 123, 25, cor);            // Desenha uma linha
        ssd1306_line(&ssd, 3, 37, 123, 37, cor);            // Desenha uma linha
        ssd1306_line(&ssd, 3, 49, 123, 49, cor);    
        ssd1306_line(&ssd, 3, 61, 123, 61, cor);    
        ssd1306_draw_string(&ssd, "SKYTRACE", 32, 6);  // Desenha uma string
        ssd1306_draw_string(&ssd, "WIFI STATUS: ", 7, 16);   // Desenha uma string
        ssd1306_draw_string(&ssd, "PRESS: ", 7, 28); // Desenha uma string
        ssd1306_draw_string(&ssd, str_pa, 60, 28);  
        ssd1306_draw_string(&ssd, "TEMP: ", 7, 40); 
        ssd1306_draw_string(&ssd, str_tmp2, 60, 40);
        ssd1306_draw_string(&ssd, "HUM:  ", 7, 52);
        ssd1306_draw_string(&ssd, str_umi, 60, 52);
        ssd1306_send_data(&ssd);                            // Atualiza o display

        sleep_ms(500);
    }

    return 0;
}