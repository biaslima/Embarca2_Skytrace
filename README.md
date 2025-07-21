# 🌤️ SkyTrace — Estação Meteorológica IoT com Interface Web

Projeto desenvolvido individualmente como parte da Tarefa 2 de Sensores e Atuadores IoT (EmbarcaTech 2).  
A SkyTrace é uma estação meteorológica embarcada, baseada na BitDogLab (RP2040), com monitoramento contínuo de temperatura, umidade e pressão, exibição local via display OLED e acesso remoto por uma interface web responsiva.

## ⚙️ Funcionalidades

- Leitura contínua dos sensores:
  - **AHT20** (temperatura e umidade)
  - **BMP280** (pressão e temperatura)
- Exibição local no **display OLED SSD1306**
- Interface Web responsiva:
  - Exibe os dados em **tempo real com gráficos**
  - Permite configuração de **limites e offsets**
  - Implementada em **HTML, CSS e JavaScript puro (AJAX)**
- Alertas automáticos com:
  - **LED RGB** (um por tipo de alerta)
  - **Matriz de LEDs WS2812** (padrões coloridos)
  - **Buzzer**
- Botões físicos:
  - **Botão A**: Reset de configurações
  - **Botão B**: Exibe limites atuais no display

## 🔧 Tecnologias e Recursos Utilizados

- 📦 **RP2040 (BitDogLab)**
- 📶 **Conexão Wi-Fi com CYW43**
- 📟 **Display OLED SSD1306 (I2C)**
- 🌡️ **Sensores AHT20 e BMP280 (I2C)**
- 💡 **Matriz de LEDs WS2812 (5x5)**
- 🎯 **Botões com interrupção e debounce**
- 🧠 **Servidor HTTP leve com lwIP**
- 🌐 **Frontend HTML/CSS/JS sem frameworks**

## 📝 Como usar

1. Clone o repositório:
   ```bash
   git clone https://github.com/seu-usuario/skytrace.git
   cd skytrace
Configure sua rede Wi-Fi diretamente no arquivo webserver.c.
Edite as linhas:

#define WIFI_SSID "NomeDaRede"
#define WIFI_PASS "SenhaDaRede"
Compile e grave o código na BitDogLab com a SDK do Pico (pico-sdk).

Ao inicializar, o IP local será exibido no display OLED.
Acesse esse IP no navegador de qualquer dispositivo na mesma rede.

👤 Autor
Anna Beatriz Silva Lima
Projeto individual — EmbarcaTech 2 — CEPEDI



