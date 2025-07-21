#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "wifi_secrets.h"
#include "webserver.h"

extern volatile float lim_min_temp, lim_max_temp, lim_min_pressao, lim_max_pressao, lim_min_umi, lim_max_umi;
extern float temperatura_atual, pressao_atual, umidade_atual;
float offset_temp = 0.0, offset_pressao = 0.0, offset_umidade = 0.0;

const char HTML_MAIN[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<style>"
"body{font-family:'Poppins',Tahoma,Geneva,Verdana,sans-serif;margin:0;"
"height:100vh;display:flex;align-items:center;justify-content:center;"
"background:linear-gradient(135deg,rgba(0,82,177,1) 0%,#ffe600ff 100%);}"
".box{background:white;padding:18px 20px 16px 20px;border-radius:16px;margin:0 auto;max-width:360px;"
"box-shadow:0 2px 8px #0002;text-align:center;}"
"h1{color:#004ea2;margin-bottom:10px;margin-top:0;font-size:40px;font-weight:700;letter-spacing:1px;}"
".val{font-size:17px;margin:12px 0 2px 0;color:#333;line-height:1.6;}"
"svg{width:96%;max-width:340px;height:54px;background:#f8f8f8;margin:4px 0 8px 0;"
"border-radius:8px;border:1px solid #ddd;display:block;mx-auto:1;}"
"a{display:inline-block;background:#004ea2;color:white;"
"padding:9px 23px;margin:12px 0 0 0;text-decoration:none;border-radius:7px;font-size:15px;"
"box-shadow:0 1px 2px #0001;transition:background 0.15s;}"
"a:hover{background:#0361c4;}"
"</style></head><body>"
"<div class='box'><h1>SKYTRACE</h1>"
"<div class='val'>Temp: <span id='t'>--</span>°C</div><svg id='gt'></svg>"
"<div class='val'>Pressão: <span id='p'>--</span>kPa</div><svg id='gp'></svg>"
"<div class='val'>Umidade: <span id='u'>--</span>%</div><svg id='gu'></svg>"
"<a href='/cfg'>Configurações</a></div>"
"<script>"
"let dt=[],dp=[],du=[],mx=15;"
"function draw(id,data,color,unit){"
"let svg=document.getElementById(id),w=svg.clientWidth,h=svg.clientHeight;"
"if(data.length<2){svg.innerHTML='';return;}"
"let min=Math.min(...data),max=Math.max(...data);if(min==max){min-=0.5;max+=0.5;}"
"let pts=data.map((v,i)=>{"
"let x=i*w/(mx-1),y=h-((v-min)/(max-min))*h*0.8-h*0.1;"
"return x+','+y;"
"}).join(' ');"
"svg.innerHTML='<polyline fill=\"none\" stroke=\"'+color+'\" stroke-width=\"2\" points=\"'+pts+'\"/>'+"
"'<text x=\"8\" y=\"13\" font-size=\"11\" fill=\"#666\">'+max.toFixed(1)+unit+'</text>'+"
"'<text x=\"8\" y=\"'+h+'\" font-size=\"11\" fill=\"#666\">'+min.toFixed(1)+unit+'</text>';"
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
"draw('gp',dp,'#27ae60','kPa');"
"draw('gu',du,'#3498db','%');"
"}).catch(e=>console.log('Erro:',e));"
"}"
"setInterval(up,3000);up();"
"</script></body></html>";

// Página de configuração 
const char HTML_CFG[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<style>"
"body{font-family:'Poppins',Arial,sans-serif;margin:0;background:rgba(0,82,177,1);display:flex;"
"align-items:center;justify-content:center;min-height:100vh;}"
".cfg{background:white;padding:23px 18px;border-radius:16px;box-shadow:0 2px 10px #0002;max-width:500px;width:99%;text-align:center;}"
"h2{margin:0 0 13px 0;color:#004ea2;font-size:19px;letter-spacing:1px;}"
".current{font-size:13px;color:#666;margin-bottom:7px;}"
"form{background:#fbfcff;padding:11px 0;border-radius:11px;margin:10px 0;text-align:center;box-shadow:0 0.5px 2px #ccc1;}"
"input{margin:3px 3px 3px 6px;padding:6px 4px;width:70px;border:1.1px solid #ccd7ee;border-radius:6px;}"
"button{background:#004ea2;color:white;padding:7px 14px;border:none;border-radius:7px;font-size:14px;font-family:inherit;"
"box-shadow:0 1px 2px #0001;margin-left:8px}"
"button:hover{background:#0361c4;}"
"a{display:inline-block;background:#666;color:white;padding:8px 19px;margin:12px 0 0 0;text-decoration:none;border-radius:7px;font-size:15px;box-shadow:0 1px 2px #0001;}"
"a:hover{background:#333;}"
"</style></head><body>"
"<div class='cfg'>"
"<h2>Configuração</h2>"
"<div class='current'>Valores atuais dos offsets:</div>"
"<div class='current' id='offsets'>Carregando...</div>"
"<form method='GET' action='/st'><b>Temp:</b> Min:<input name='tm' type='number' step='0.1' required> Max:<input name='tx' type='number' step='0.1' required><button type='submit'>OK</button></form>"
"<form method='GET' action='/sp'><b>Pressão:</b> Min:<input name='pm' type='number' step='0.1' required> Max:<input name='px' type='number' step='0.1' required><button type='submit'>OK</button></form>"
"<form method='GET' action='/su'><b>Umidade:</b> Min:<input name='um' type='number' step='0.1' required> Max:<input name='ux' type='number' step='0.1' required><button type='submit'>OK</button></form>"
"<form method='GET' action='/ot'><b>Offset Temp:</b><input name='o' type='number' step='0.1' required placeholder='Ex: 2.5'><button type='submit'>Aplicar</button></form>"
"<form method='GET' action='/op'><b>Offset Pressão:</b><input name='o' type='number' step='0.1' required placeholder='Ex: 1.2'><button type='submit'>Aplicar</button></form>"
"<form method='GET' action='/ou'><b>Offset Umidade:</b><input name='o' type='number' step='0.1' required placeholder='Ex: -3.0'><button type='submit'>Aplicar</button></form>"
"<form method='GET' action='/reset'><button type='submit'>Resetar configurações</button></form>"
"<a href='/'>Voltar</a>"
"<script>"
"fetch('/offsets').then(r=>r.json()).then(x=>{"
"document.getElementById('offsets').innerHTML='Temp: '+x.ot+'°C | Pressão: '+x.op+'kPa | Umidade: '+x.ou+'%';"
"}).catch(e=>console.log('Erro:',e));"
"</script>"
"</div></body></html>";

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
    printf("DEBUG: Procurando parametro '%s' na requisicao\n", param);
    printf("DEBUG: Requisicao completa: %.200s\n", req);
    
    char *p = strstr(req, param);
    if (!p) {
        printf("DEBUG: Parametro '%s' nao encontrado\n", param);
        return 0;
    }
    
    char *equals = strchr(p, '=');
    if (!equals) {
        printf("DEBUG: '=' nao encontrado apos '%s'\n", param);
        return 0;
    }
    
    float val = 0;
    int parsed = sscanf(equals + 1, "%f", &val);
    
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
    size_t header_len = 150; 
    size_t total_len = header_len + content_len;
    
    hs->data = malloc(total_len);
    if (!hs->data) {
        free(hs);
        tcp_close(tpcb);
        return;
    }
    
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
    if (strstr(req, "GET /d")) {  
        char json[128];
        float temp_final = temperatura_atual + offset_temp;
        float press_final = pressao_atual + offset_pressao;
        float umi_final = umidade_atual + offset_umidade;
        
        snprintf(json, sizeof(json), 
                "{\"t\":%.1f,\"p\":%.1f,\"u\":%.1f}",
                temp_final, press_final, umi_final);
        
        // Debug: imprimir no console
        printf("Enviando dados: Temp=%.1f (%.1f+%.1f), Press=%.1f (%.1f+%.1f), Umi=%.1f (%.1f+%.1f)\n",
               temp_final, temperatura_atual, offset_temp,
               press_final, pressao_atual, offset_pressao,
               umi_final, umidade_atual, offset_umidade);
               
        send_response(tpcb, json, "application/json");
    }
    else if (strstr(req, "GET /offsets")) {  
        char json[128];
        snprintf(json, sizeof(json), 
                "{\"ot\":%.1f,\"op\":%.1f,\"ou\":%.1f}",
                offset_temp, offset_pressao, offset_umidade);
        send_response(tpcb, json, "application/json");
    }
    else if (strstr(req, "GET /cfg")) {  // Página configuração
        send_response(tpcb, HTML_CFG, "text/html");
    }
    else if (strstr(req, "GET /st")) {  // Set temperatura
        lim_min_temp = get_param(req, "tm");
        lim_max_temp = get_param(req, "tx");
        printf("Limites temperatura definidos: %.1f - %.1f\n", lim_min_temp, lim_max_temp);
        send_redirect(tpcb);
    }
    else if (strstr(req, "GET /sp")) {  // Set pressão
        lim_min_pressao = get_param(req, "pm");
        lim_max_pressao = get_param(req, "px");
        printf("Limites pressão definidos: %.1f - %.1f\n", lim_min_pressao, lim_max_pressao);
        send_redirect(tpcb);
    }
    else if (strstr(req, "GET /su")) {  // Set umidade
        lim_min_umi = get_param(req, "um");
        lim_max_umi = get_param(req, "ux");
        printf("Limites umidade definidos: %.1f - %.1f\n", lim_min_umi, lim_max_umi);
        send_redirect(tpcb);
    }
    else if (strstr(req, "GET /ot")) {  // Offset temperatura
        printf("DEBUG: Requisicao de offset temperatura recebida\n");
        float new_offset = get_param(req, "o");
        offset_temp = new_offset;
        printf("Offset temperatura definido: %.2f\n", offset_temp);
        send_redirect(tpcb);
    }
    else if (strstr(req, "GET /op")) {  // Offset pressão
        printf("DEBUG: Requisicao de offset pressao recebida\n");
        float new_offset = get_param(req, "o");
        offset_pressao = new_offset;
        printf("Offset pressao definido: %.2f\n", offset_pressao);
        send_redirect(tpcb);
    }
    else if (strstr(req, "GET /ou")) {  // Offset umidade
        printf("DEBUG: Requisicao de offset umidade recebida\n");
        float new_offset = get_param(req, "o");
        offset_umidade = new_offset;
        printf("Offset umidade definido: %.2f\n", offset_umidade);
        send_redirect(tpcb);
    } 
    else if (strstr(req, "GET /reset")) {  // Reset configurações
        lim_min_temp = -10.0;
        lim_max_temp = 60.0;
        lim_min_pressao = 90.0;
        lim_max_pressao = 107.0;
        lim_min_umi = 0.0;
        lim_max_umi = 100.0;
        offset_temp = 0.0;
        offset_pressao = 0.0;
        offset_umidade = 0.0;
        printf("Configurações resetadas\n");
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

void get_offsets(float *temp, float *press, float *hum) {
    *temp = offset_temp;
    *press = offset_pressao;
    *hum = offset_umidade;
}