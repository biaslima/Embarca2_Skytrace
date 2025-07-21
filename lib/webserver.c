#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "wifi_secrets.h"
#include "webserver.h"

extern volatile float lim_min_temp, lim_max_temp, lim_min_pressao, lim_max_pressao, lim_min_umi, lim_max_umi;
extern float temperatura_atual, pressao_atual, umidade_atual;
volatile float offset_temp = 0, offset_pressao = 0, offset_umidade = 0;

// HTML com gráficos SVG otimizados para Pico W
const char HTML_MAIN[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<style>"
"body{font-family:Arial;margin:10px;background:#f0f8ff;}"
".box{background:white;padding:10px;border-radius:5px;margin:5px auto;max-width:350px;}"
"h1{color:#004ea2;margin:0;font-size:20px;}"
".val{font-size:16px;margin:8px 0;color:#333;}"
"svg{width:100%;height:40px;background:#f8f8f8;margin:3px 0;border-radius:3px;border:1px solid #ddd;}"
"a{display:inline-block;background:#004ea2;color:white;padding:8px 12px;margin:5px;text-decoration:none;border-radius:3px;font-size:14px;}"
"</style></head><body>"
"<div class='box'><h1>SKYTRACE</h1>"
"<div class='val'>Temp: <span id='t'>--</span>°C</div><svg id='gt'></svg>"
"<div class='val'>Pressão: <span id='p'>--</span>kPa</div><svg id='gp'></svg>"
"<div class='val'>Umidade: <span id='u'>--</span>%</div><svg id='gu'></svg>"
"<a href='/cfg'>Config</a></div>"
"<script>"
"let dt=[],dp=[],du=[],mx=15;"
"function draw(id,data,color,unit){"
"let svg=document.getElementById(id),w=svg.clientWidth,h=svg.clientHeight;"
"if(data.length<2){svg.innerHTML='';return;}"
"let min=Math.min(...data),max=Math.max(...data);"
"if(min==max){min-=0.5;max+=0.5;}"
"let pts=data.map((v,i)=>{"
"let x=i*w/(mx-1),y=h-((v-min)/(max-min))*h*0.8-h*0.1;"
"return x+','+y;"
"}).join(' ');"
"svg.innerHTML='<polyline fill=\"none\" stroke=\"'+color+'\" stroke-width=\"2\" points=\"'+pts+'\"/>'+"
"'<text x=\"5\" y=\"12\" font-size=\"10\" fill=\"#666\">'+max.toFixed(1)+unit+'</text>'+"
"'<text x=\"5\" y=\"'+h+'\" font-size=\"10\" fill=\"#666\">'+min.toFixed(1)+unit+'</text>';"
"}"
"function up(){"
"fetch('/d').then(r=>r.json()).then(x=>{"
"document.getElementById('t').innerText=x.t;"
"document.getElementById('p').innerText=x.p;"
"document.getElementById('u').innerText=x.u;"
"dt.push(+x.t);if(dt.length>mx)dt.shift();"
"dp.push(+x.p);if(dp.length>mx)dp.shift();"
"du.push(+x.u);if(du.length>mx)du.shift();"
"draw('gt',dt,'#e74c3c','°C');"
"draw('gp',dp,'#3498db','kPa');"
"draw('gu',du,'#27ae60','%');"
"}).catch(e=>console.log('Erro:',e));"
"}"
"setInterval(up,3000);up();"
"</script></body></html>";

// Página de configuração simplificada
const char HTML_CFG[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<style>"
"body{font-family:Arial;margin:10px;background:#f0f8ff;}"
"form{background:white;padding:8px;margin:8px 0;border-radius:5px;}"
"input{margin:3px;padding:5px;width:70px;}"
"button{background:#004ea2;color:white;padding:5px 8px;border:none;border-radius:3px;}"
"a{display:inline-block;background:#666;color:white;padding:8px 12px;margin:5px;text-decoration:none;border-radius:3px;}"
"</style></head><body>"
"<h2>Configuração</h2>"
"<form action='/st'><b>Temp:</b> Min:<input name='tm' type='number' step='0.1'> Max:<input name='tx' type='number' step='0.1'><button>OK</button></form>"
"<form action='/sp'><b>Pressão:</b> Min:<input name='pm' type='number' step='0.1'> Max:<input name='px' type='number' step='0.1'><button>OK</button></form>"
"<form action='/su'><b>Umidade:</b> Min:<input name='um' type='number' step='0.1'> Max:<input name='ux' type='number' step='0.1'><button>OK</button></form>"
"<form action='/ot'><b>Offset Temp:</b><input name='o' type='number' step='0.1'><button>OK</button></form>"
"<form action='/op'><b>Offset Pressão:</b><input name='o' type='number' step='0.1'><button>OK</button></form>"
"<form action='/ou'><b>Offset Umidade:</b><input name='o' type='number' step='0.1'><button>OK</button></form>"
"<form action='/reset'><button type='submit'>Resetar configurações</button></form>"
"<a href='/'>Voltar</a></body></html>";

struct http_state {
    char *data;
    size_t len;
    size_t sent;
};

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct http_state *hs = (struct http_state *)arg;
    if (!hs) return ERR_OK;
    
    hs->sent += len;
    if (hs->sent >= hs->len) {
        if (hs->data) free(hs->data);
        free(hs);
        tcp_close(tpcb);
    }
    return ERR_OK;
}

static float get_param(const char *req, const char *param) {
    char *p = strstr(req, param);
    if (!p) return 0;
    float val = 0;
    sscanf(p + strlen(param) + 1, "%f", &val);
    return val;
}

static void send_response(struct tcp_pcb *tpcb, const char *content, const char *type) {
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs) {
        tcp_close(tpcb);
        return;
    }
    
    // Calcular tamanho total necessário
    size_t content_len = strlen(content);
    size_t header_len = 150; // Tamanho estimado do header
    size_t total_len = header_len + content_len;
    
    hs->data = malloc(total_len);
    if (!hs->data) {
        free(hs);
        tcp_close(tpcb);
        return;
    }
    
    // Criar resposta completa
    hs->len = snprintf(hs->data, total_len,
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: %s\r\n"
                      "Content-Length: %d\r\n"
                      "Connection: close\r\n"
                      "\r\n%s",
                      type, (int)content_len, content);
    
    hs->sent = 0;
    
    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);
    tcp_write(tpcb, hs->data, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
}

static void send_redirect(struct tcp_pcb *tpcb) {
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs) {
        tcp_close(tpcb);
        return;
    }
    
    const char redirect[] = "HTTP/1.1 302 Found\r\nLocation: /\r\nConnection: close\r\n\r\n";
    hs->data = malloc(strlen(redirect) + 1);
    if (!hs->data) {
        free(hs);
        tcp_close(tpcb);
        return;
    }
    
    strcpy(hs->data, redirect);
    hs->len = strlen(redirect);
    hs->sent = 0;
    
    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);
    tcp_write(tpcb, hs->data, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
}

static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    
    // Processar requisição
    if (strstr(req, "GET /d")) {  // Dados JSON
        char json[128];
        snprintf(json, sizeof(json), 
                "{\"t\":%.1f,\"p\":%.1f,\"u\":%.1f}",
                temperatura_atual + offset_temp,
                pressao_atual + offset_pressao,
                umidade_atual + offset_umidade);
        send_response(tpcb, json, "application/json");
    }
    else if (strstr(req, "GET /cfg")) {  // Página configuração
        send_response(tpcb, HTML_CFG, "text/html");
    }
    else if (strstr(req, "GET /st")) {  // Set temperatura
        lim_min_temp = get_param(req, "tm");
        lim_max_temp = get_param(req, "tx");
        send_redirect(tpcb);
    }
    else if (strstr(req, "GET /sp")) {  // Set pressão
        lim_min_pressao = get_param(req, "pm");
        lim_max_pressao = get_param(req, "px");
        send_redirect(tpcb);
    }
    else if (strstr(req, "GET /su")) {  // Set umidade
        lim_min_umi = get_param(req, "um");
        lim_max_umi = get_param(req, "ux");
        send_redirect(tpcb);
    }
    else if (strstr(req, "GET /ot")) {  // Offset temperatura
        offset_temp = get_param(req, "o");
        send_redirect(tpcb);
    }
    else if (strstr(req, "GET /op")) {  // Offset pressão
        offset_pressao = get_param(req, "o");
        send_redirect(tpcb);
    }
    else if (strstr(req, "GET /ou")) {  // Offset umidade
        offset_umidade = get_param(req, "o");
        send_redirect(tpcb);
    } 
    else if (strstr(req, "GET /reset")) {  // Offset umidade
        lim_min_temp = -10.0;
        lim_max_temp = 60.0;
        lim_min_pressao = 90.0;
        lim_max_pressao = 107.0;
        lim_min_umi = 0.0;
        lim_max_umi = 100.0;
        offset_temp = 0, offset_pressao = 0, offset_umidade = 0;
        send_redirect(tpcb);
    }
    else {  // Página principal
        send_response(tpcb, HTML_MAIN, "text/html");
    }

    pbuf_free(p);
    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    if (err != ERR_OK || newpcb == NULL) {
        return ERR_VAL;
    }
    
    tcp_recv(newpcb, http_recv);
    tcp_err(newpcb, NULL);
    return ERR_OK;
}

bool webserver_init(void) {
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar Wi-Fi\n");
        return false;
    }
    
    cyw43_arch_enable_sta_mode();
    
    printf("Conectando ao Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 15000)) {
        printf("Erro ao conectar ao Wi-Fi\n");
        cyw43_arch_deinit();
        return false;
    }
    
    printf("Wi-Fi conectado!\n");
    printf("IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
    
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return false;
    }
    
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao fazer bind na porta 80\n");
        tcp_close(pcb);
        return false;
    }
    
    pcb = tcp_listen(pcb);
    if (!pcb) {
        printf("Erro ao colocar em modo listen\n");
        return false;
    }
    
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP iniciado na porta 80\n");
    return true;
}