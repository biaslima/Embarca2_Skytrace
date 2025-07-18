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

// HTML ultra compacto com gráficos SVG simples
const char HTML_MAIN[] = 
"<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<style>body{font-family:Arial;text-align:center;margin:20px;background:#f0f8ff;}"
".box{background:white;padding:15px;border-radius:8px;margin:10px auto;max-width:400px;}"
"h1{color:#004ea2;margin:0;}.val{font-size:18px;margin:10px 0;color:#333;}"
"svg{width:100%;height:60px;background:#f8f8f8;margin:5px 0;border-radius:3px;}"
"a{display:inline-block;background:#004ea2;color:white;padding:8px 15px;margin:5px;text-decoration:none;border-radius:5px;}"
"</style></head><body><div class='box'><h1>SKYTRACE</h1>"
"<div class='val'>Temp: <span id='t'>--</span>°C</div><svg id='gt'></svg>"
"<div class='val'>Pressão: <span id='p'>--</span>kPa</div><svg id='gp'></svg>"
"<div class='val'>Umidade: <span id='u'>--</span>%</div><svg id='gu'></svg>"
"<a href='/cfg'>Configurar</a></div>"
"<script>let ht=[],hp=[],hu=[],n=20;function plot(id,v,c){let s=document.getElementById(id),w=s.clientWidth,h=s.clientHeight;"
"if(v.length<2)return;let min=Math.min(...v),max=Math.max(...v);if(min==max){min-=1;max+=1;}"
"let pts=v.map((x,i)=>i*(w/(n-1))+','+(h-((x-min)/(max-min))*h*0.8+h*0.1));"
"s.innerHTML=`<polyline fill='none' stroke='${c}' stroke-width='2' points='${pts.join(' ')}'/>`;"
"}function up(){fetch('/d').then(r=>r.json()).then(x=>{document.getElementById('t').innerText=x.t;"
"document.getElementById('p').innerText=x.p;document.getElementById('u').innerText=x.u;"
"ht.push(+x.t);if(ht.length>n)ht.shift();hp.push(+x.p);if(hp.length>n)hp.shift();"
"hu.push(+x.u);if(hu.length>n)hu.shift();plot('gt',ht,'red');plot('gp',hp,'blue');plot('gu',hu,'green');});"
"}setInterval(up,3000);up();</script></body></html>";

// Página de configuração - carregada sob demanda
const char HTML_CFG[] = 
"<html><head><meta charset='UTF-8'><style>body{font-family:Arial;margin:20px;background:#f0f8ff;}"
"form{background:white;padding:10px;margin:10px 0;border-radius:5px;}"
"input{margin:5px;padding:5px;width:80px;}button{background:#004ea2;color:white;padding:5px 10px;border:none;border-radius:3px;}"
"</style></head><body><h2>Configuração</h2>"
"<form action='/st'><b>Temp:</b> Min:<input name='tm' type='number' step='0.1'> Max:<input name='tx' type='number' step='0.1'><button>OK</button></form>"
"<form action='/sp'><b>Pressão:</b> Min:<input name='pm' type='number' step='0.1'> Max:<input name='px' type='number' step='0.1'><button>OK</button></form>"
"<form action='/su'><b>Umidade:</b> Min:<input name='um' type='number' step='0.1'> Max:<input name='ux' type='number' step='0.1'><button>OK</button></form>"
"<form action='/ot'><b>Offset Temp:</b><input name='o' type='number' step='0.1'><button>OK</button></form>"
"<form action='/op'><b>Offset Pressão:</b><input name='o' type='number' step='0.1'><button>OK</button></form>"
"<form action='/ou'><b>Offset Umidade:</b><input name='o' type='number' step='0.1'><button>OK</button></form>"
"<a href='/'>Voltar</a></body></html>";

struct http_state {
    const char* data;
    size_t len;
    size_t sent;
};

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len) {
        tcp_close(tpcb);
        free(hs);
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
    if (!hs) return;
    
    static char header[256];
    hs->len = snprintf(header, sizeof(header), 
                      "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n",
                      type, (int)strlen(content));
    
    // Combinar header e content em uma única resposta
    static char response[2048];
    strcpy(response, header);
    strcat(response, content);
    
    hs->data = response;
    hs->len = strlen(response);
    hs->sent = 0;
    
    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);
    tcp_write(tpcb, hs->data, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
}

static void send_redirect(struct tcp_pcb *tpcb) {
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs) return;
    
    static char redirect[] = "HTTP/1.1 302 Found\r\nLocation: /\r\n\r\n";
    hs->data = redirect;
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
    
    if (strstr(req, "GET /d")) {  // Dados JSON
        static char json[64];
        snprintf(json, sizeof(json), "{\"t\":%.1f,\"p\":%.1f,\"u\":%.1f}",
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
    else {  // Página principal
        send_response(tpcb, HTML_MAIN, "text/html");
    }

    pbuf_free(p);
    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

bool webserver_init(void) {
    if (cyw43_arch_init()) return false;
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 15000)) {
        cyw43_arch_deinit();
        return false;
    }
    
    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    return true;
}