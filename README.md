# Utilização dos sensores AHT10, AHT20 e BMP280 na Bitdoglab.

## Por: Wilton Lacerda Silva

Este projeto utiliza o microcontrolador **RP2040** (Raspberry Pi Pico), sensores digitais de umidade/temperatura (**AHT20**), pressão/temperatura/altitude (**BMP280**) e um display OLED I2C (**SSD1306**).

## Funcionalidades

- Leitura simultânea de temperatura e umidade (AHT10 ou AHT20)
- Leitura de temperatura, pressão e cálculo de altitude (BMP280)
- Exibição das informações em tempo real no display OLED SSD1306
- Comunicação via barramento I2C com múltiplos dispositivos em diferentes barramentos (i2c0 e i2c1)
- Saída dos dados para o terminal serial (debug/log)
- Função BOOTSEL no botão B (GPIO 6) para facilitar a regravação do firmware

---

## Hardware Necessário

- Raspberry Pi Pico (RP2040)
- Sensor AHT20 (ou AHT10) — I2C
- Sensor BMP280 — I2C
- Display OLED SSD1306 (128x64) — I2C
- Cabos de conexão

---

## Conexões (GPIO)

| Dispositivo       | Barramento | SDA   | SCL   | Endereço I2C padrão |
|-------------------|------------|-------|-------|---------------------|
| AHT20 + BMP280    | i2c0       | 0     | 1     | 0x38 (AHT20), 0x76/0x77 (BMP280) |
| SSD1306 Display   | i2c1       | 14    | 15    | 0x3C                |
| Botão B (BOOTSEL) | -          | 6     | -     | -                   |

---

## Como Funciona

1. O programa inicializa os dois barramentos I2C (`i2c0` para sensores e `i2c1` para display).
2. Inicializa e lê periodicamente os sensores AHT20 (umidade/temperatura) e BMP280 (pressão/temperatura).
3. Calcula a altitude com base na pressão atmosférica.
4. Mostra as informações em tempo real no display SSD1306.
5. Exibe também os valores no terminal serial para depuração/registro.

---

## Dependências e Compilação

- SDK do Raspberry Pi Pico  
  [https://github.com/raspberrypi/pico-sdk](https://github.com/raspberrypi/pico-sdk)
- Bibliotecas customizadas (devem estar no projeto):
    - `aht20.h`
    - `bmp280.h`
    - `ssd1306.h`
    - `font.h`

**Compile com a extensão do Raspberry Pi Pico no VS Code.**

---

## Exemplo de Uso

1. Programe o Raspberry Pi Pico com o firmware compilado.
2. Conecte os sensores e o display conforme a tabela de pinos acima.
3. Abra um terminal serial para acompanhar a saída de dados.
4. O display mostrará os valores de temperatura, umidade, pressão e altitude, além de identificações do projeto.

---

## Observações

- O botão B (GPIO6) pode ser usado para entrar no modo BOOTSEL e facilitar a regravação do firmware.
- O cálculo da altitude considera pressão ao nível do mar fixa (`101325 Pa`). Ajuste conforme necessário para maior precisão.

---
