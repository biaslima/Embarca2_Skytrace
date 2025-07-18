#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "wifi_secrets.h"

#include "webserver.h"

extern volatile float lim_min_temp;
extern volatile float lim_max_temp;
extern volatile float lim_min_pressao;
extern volatile float lim_max_pressao;
extern volatile float lim_min_umi;
extern volatile float lim_max_umi;

// Variáveis para armazenar os valores atuais dos sensores
extern float temperatura_atual;
extern float pressao_atual;
extern float umidade_atual;

// Conteúdo da página HTML
const char HTML_BODY[] =
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"<meta charset='UTF-8'>\n"
"<title>Sensores</title>\n"
"<meta name='viewport' content='width=device-width, initial-scale=1'>\n"
"<style>\n"
"body { font-family:sans-serif; background:linear-gradient(135deg,rgb(139, 193, 255) 0%, #ffbb00 100%); text-align:center; }\n"
".c { background:#fff; padding:10px; margin:auto; max-width:700px; border-radius:8px; }\n"
"h1 { color:#004ea2; }\n"
"svg { width:100%; height:120px; background:#eee; margin-bottom:8px; }\n"
"form { margin:10px 0; font-size:14px; }\n"
"label { display:block; margin-top:6px; }\n"
"input[type='number'],input[type='submit'] { width:90%; padding:6px; margin-top:4px;  }\n"
"input[type='submit'] { background:#004ea2; color:white; border:none; border-radius:20px; cursor:pointer; margin-top: 10px; width: 60%; }\n"
"</style>\n"
"</head>\n"
"<body>\n"
"<div class='c'>\n"
"<h1>SKYTRACE</h1>\n"
"<h3>Estação Metereológica</h3>\n"
"<p>Temp: <span id='t'>--</span>°C</p><svg id='gt'></svg>\n"
"<p>Pressão: <span id='p'>--</span>kPa</p><svg id='gp'></svg>\n"
"<p>Umidade: <span id='u'>--</span>%</p><svg id='gu'></svg>\n"

"<form action='/limites_temp' method='get'>\n"
"<h3>Limites de Temperatura</h3>\n"
"<label>Min (°C):</label>\n"
"<input type='number' name='min_temp' step='0.1' required>\n"
"<label>Max (°C):</label>\n"
"<input type='number' name='max_temp' step='0.1' required>\n"
"<input type='submit' value='Atualizar'>\n"
"</form>\n"

"<form action='/limites_pressao' method='get'>\n"
"<h3>Limites de Pressão</h3>\n"
"<label>Min (kPa):</label>\n"
"<input type='number' name='min_pressao' step='0.1' required>\n"
"<label>Max (kPa):</label>\n"
"<input type='number' name='max_pressao' step='0.1' required>\n"
"<input type='submit' value='Atualizar'>\n"
"</form>\n"

"<form action='/limites_umidade' method='get'>\n"
"<h3>Limites de Umidade</h3>\n"
"<label>Min (%):</label>\n"
"<input type='number' name='min_umi' step='0.1' required>\n"
"<label>Max (%):</label>\n"
"<input type='number' name='max_umi' step='0.1' required>\n"
"<input type='submit' value='Atualizar'>\n"
"</form>\n"

"<script>\n"
"let ht=[],hp=[],hu=[],n=30;\n"
"function d(id,v,min,max,c){\n"
"let s=document.getElementById(id),w=s.clientWidth,h=s.clientHeight;\n"
"let pts=v.map((x,i)=>(i*(w/n))+','+(h-(x-min)/(max-min)*h));\n"
"s.innerHTML='<polyline fill=\"none\" stroke=\"'+c+'\" stroke-width=\"2\" points=\"'+pts.join(' ')+'\"/>';\n"
"}\n"
"function up(){\n"
"fetch('/estado').then(r=>r.json()).then(dv=>{\n"
"document.getElementById('t').innerText=dv.temperatura.toFixed(1);\n"
"document.getElementById('p').innerText=dv.pressao.toFixed(1);\n"
"document.getElementById('u').innerText=dv.umidade.toFixed(1);\n"
"ht.push(dv.temperatura); if(ht.length>n)ht.shift();\n"
"hp.push(dv.pressao); if(hp.length>n)hp.shift();\n"
"hu.push(dv.umidade); if(hu.length>n)hu.shift();\n"
"d('gt',ht,0,50,'red');\n"
"d('gp',hp,90,110,'blue');\n"
"d('gu',hu,0,100,'green');\n"
"});}\n"
"setInterval(up,2000);up();\n"
"</script>\n"

"</div>\n"
"</body>\n"
"</html>\n";

// Estrutura para manter o estado da resposta HTTP
struct http_state {
    char response[4096];
    size_t len;
    size_t sent;
};

// Callback chamado quando os dados são enviados com sucesso
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len) {
        tcp_close(tpcb);
        free(hs);
    }
    return ERR_OK;
}

// Função principal que processa as requisições HTTP
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    hs->sent = 0;

    // Processa limites de temperatura
    if (strstr(req, "GET /limites_temp")) {
        char *min_str = strstr(req, "min_temp=");
        char *max_str = strstr(req, "max_temp=");
        if (min_str && max_str) {
            sscanf(min_str, "min_temp=%f", &lim_min_temp);
            sscanf(max_str, "max_temp=%f", &lim_max_temp);
            printf("Limites de temperatura atualizados: Min=%.1f, Max=%.1f\n", lim_min_temp, lim_max_temp);
        }
        const char *redir_hdr = "HTTP/1.1 302 Found\r\nLocation: /\r\n\r\n";
        hs->len = snprintf(hs->response, sizeof(hs->response), "%s", redir_hdr);
    }
    // Processa limites de pressão
    else if (strstr(req, "GET /limites_pressao")) {
        char *min_str = strstr(req, "min_pressao=");
        char *max_str = strstr(req, "max_pressao=");
        if (min_str && max_str) {
            sscanf(min_str, "min_pressao=%f", &lim_min_pressao);
            sscanf(max_str, "max_pressao=%f", &lim_max_pressao);
            printf("Limites de pressão atualizados: Min=%.1f, Max=%.1f\n", lim_min_pressao, lim_max_pressao);
        }
        const char *redir_hdr = "HTTP/1.1 302 Found\r\nLocation: /\r\n\r\n";
        hs->len = snprintf(hs->response, sizeof(hs->response), "%s", redir_hdr);
    }
    // Processa limites de umidade
    else if (strstr(req, "GET /limites_umidade")) {
        char *min_str = strstr(req, "min_umi=");
        char *max_str = strstr(req, "max_umi=");
        if (min_str && max_str) {
            sscanf(min_str, "min_umi=%f", &lim_min_umi);
            sscanf(max_str, "max_umi=%f", &lim_max_umi);
            printf("Limites de umidade atualizados: Min=%.1f, Max=%.1f\n", lim_min_umi, lim_max_umi);
        }
        const char *redir_hdr = "HTTP/1.1 302 Found\r\nLocation: /\r\n\r\n";
        hs->len = snprintf(hs->response, sizeof(hs->response), "%s", redir_hdr);
    }
    // Retorna o estado atual dos sensores em JSON
    else if (strstr(req, "GET /estado")) {
        char json_payload[256];
        int json_len = snprintf(json_payload, sizeof(json_payload),
                                "{\"temperatura\":%.1f,\"pressao\":%.1f,\"umidade\":%.1f}",
                                temperatura_atual, pressao_atual, umidade_atual);

        hs->len = snprintf(hs->response, sizeof(hs->response),
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: %d\r\n"
                          "Connection: close\r\n\r\n%s",
                          json_len, json_payload);
    }
    // Página principal
    else {
        hs->len = snprintf(hs->response, sizeof(hs->response),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: %d\r\n"
                        "Connection: close\r\n\r\n%s",
                        (int)strlen(HTML_BODY), HTML_BODY);
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);
    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    pbuf_free(p);
    return ERR_OK;
}

// Callback para aceitar novas conexões
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

// Função para iniciar o listener do servidor
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP iniciado na porta 80\n");
}

// Função de inicialização principal do webserver
bool webserver_init(void) {
    if (cyw43_arch_init()) {
        printf("Falha para iniciar o cyw43\n");
        return false;
    }

    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi: %s\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 15000)) {
        printf("Falha para conectar ao Wi-Fi\n");
        cyw43_arch_deinit();
        return false;
    }
    
    printf("Conectado com sucesso!\n");
    start_http_server();
    return true;
}