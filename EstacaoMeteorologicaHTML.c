#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "ws2812.pio.h"
#include "inc/matriz_leds.h"

#define WIFI_SSID ""   // Alterar para o SSID da rede
#define WIFI_PASSWORD "" // Alterar para a senha da rede

// Definição de GPIOs
#define I2C_SDA 14    // Pino SDA - Dados
#define I2C_SCL 15    // Pino SCL - Clock
#define WS2812_PIN 7  // Pino do WS2812
#define BUZZER_PIN 21 // Pino do buzzer
#define BOTAO_A 5     // Pino do botao A
#define LED_RED 13    // Pino do LED vermelho
#define LED_GREEN 12  // Pino do LED verde'
#define IS_RGBW false // Maquina PIO para RGBW

uint volatile numero = 0;            // Variável para inicializar o numero com 0 (WS2812B)
uint buzzer_slice;                   // Slice para o buzzer

const char HTML_BODY[] =
"<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'/>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'/>"
"<title>Water Level Monitor</title>"
"<style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:sans-serif;background:#e0f7fa;color:#01579b;min-height:100vh}"
"header{background:#1744cb;padding:10px 20px;display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap}"
"header img{height:50px}.hdr-txt{display:flex;gap:20px;font-weight:bold;font-size:1.3rem;color:#fff}"
"h1{font-size:2rem;margin:20px auto;text-align:center}.main{display:flex;gap:50px;flex-wrap:wrap;justify-content:center;padding:20px;max-width:1000px;margin:auto}"
".tank,.pump{display:flex;flex-direction:column;align-items:center;max-width:320px;width:100%}"
".tank-box{width:100%;aspect-ratio:1;border:4px solid #0288d1;border-radius:16px;background:#b3e5fc;position:relative;margin-bottom:15px;overflow:hidden}"
".wave-wrapper{position:absolute;bottom:0;left:0;width:100%;height:100%;overflow:hidden}"
".wave-container{width:100%;height:0;position:absolute;bottom:0;left:0;overflow:hidden;transition:height 0.5s ease-in-out}"
".wave-container svg{width:100%;height:100%;position:absolute;bottom:0}"
".w1{fill:#0288d1;opacity:1}.w2{fill:#4fc3f7;opacity:0.6}"
".waves1,.waves2{display:flex;width:2400px;height:100%}"
".waves1{animation:moveWaves 3s linear infinite}.waves2{animation:moveWaves 6s linear infinite}"
"@keyframes moveWaves{0%{transform:translateX(0)}100%{transform:translateX(-1200px)}}"
".tank p,.pump p{margin:5px 0;font-size:1.1rem;text-align:center}"
".pump-box{width:100%;aspect-ratio:1;border-radius:16px;background:white;border:6px solid red;display:flex;justify-content:center;align-items:center;margin-bottom:10px;transition:border-color 0.3s}"
".pump-box.on{border-color:green}.pump-box.on img{animation:shake 0.2s infinite}"
".pump-box img{width:90%;object-fit:contain}.status.on{color:green;font-weight:bold}.status.off{color:red;font-weight:bold}"
"button{padding:10px 20px;background:#0288d1;border:none;color:white;border-radius:8px;font-size:1rem;cursor:pointer;margin-top:10px}"
"button:hover{background:#0277bd}#last-update{margin-top:30px;font-size:1.2rem;font-weight:bold;text-align:center}"
"input[type=number]{padding:5px;margin:5px;border-radius:5px;border:1px solid #0288d1;width:80px;text-align:center}"
"#limits{display:flex;justify-content:center;gap:15px;align-items:center;margin-top:30px;flex-wrap:wrap}"
"#limits-container{text-align:center;margin-top:20px}"
"#submit-limits{margin-top:10px}"
"@keyframes shake{0%,100%{transform:translate(1px,-2px)}25%{transform:translate(-2px,1px)}50%{transform:translate(2px,-1px)}75%{transform:translate(-1px,2px)}}"
"@media(max-width:768px){.main{flex-direction:column;align-items:center}.hdr-txt{justify-content:center;width:100%;font-size:1.1rem;margin-top:10px}header{flex-direction:column;align-items:center}}"
"</style></head><body>"
"<header><img src='https://i.imgur.com/wVCmCfn.png' alt='RESTIC Logo'/>"
"<div class='hdr-txt'><span>RESTIC 37</span><span>CEPEDI</span></div></header>"
"<h1>Water Level Monitoring System</h1><div class='main'>"
"<div class='tank'><div class='tank-box'>"
"<div class='wave-wrapper'><div class='wave-container' id='wave'>"
"<svg viewBox='0 0 1200 200' preserveAspectRatio='xMidYMax slice'>"
"<g class='waves1'><path class='w1' d='M0,100 C300,50 900,150 1200,100 V200 H0 Z'/>"
"<path class='w1' d='M1200,100 C1500,50 2100,150 2400,100 V200 H1200 Z'/></g>"
"<g class='waves2'><path class='w2' d='M0,100 C300,50 900,150 1200,100 V200 H0 Z'/>"
"<path class='w2' d='M1200,100 C1500,50 2100,150 2400,100 V200 H1200 Z'/></g>"
"</svg></div></div></div>"
"<p><strong>Level:</strong> <span id='level'>--%</span></p>"
"<p><strong>Volume:</strong> <span id='volume'>--L</span></p></div>"
"<div class='pump'><div id='pump-box' class='pump-box'><img src='https://i.imgur.com/sucmfnd.png' alt='Water Pump'></div>"
"<p><strong>Pump:</strong> <span id='pump-status' class='status'>--</span></p>"
"<button id='toggle-pump'>Toggle Pump</button></div></div>"
"</div>"

"<div id='limits-container'>"
"<div id='limits'>"
"<label for='min-value'><strong>Min (%):</strong></label><input type='number' id='min-value' value='20' min='0' max='100'>"
"<label for='max-value'><strong>Max (%):</strong></label><input type='number' id='max-value' value='80' min='0' max='100'>"
"</div>"
"<button id='submit-limits'>Alterar Limites</button>"
"</div>"

"<script>"
"function togglePump(){fetch('/toggle',{method:'POST'})}"
"function atualizar(){fetch('/data').then(res=>res.json()).then(data=>{"
"const level = data.lvl;"
"document.getElementById('wave').style.height = `${level*2}%`;"
"document.getElementById('level').textContent=`${level}%`;"
"document.getElementById('volume').textContent=`${(level*0.04).toFixed(2)}L`;"
"const pump=document.getElementById('pump-status'),box=document.getElementById('pump-box'),isOn=data.pump;"
"pump.textContent=isOn?'On':'Off';pump.className='status '+(isOn?'on':'off');"
"box.className='pump-box '+(isOn?'on':'');"
"}).catch(e=>console.error('Erro ao atualizar:',e))}"
"document.getElementById('toggle-pump').addEventListener('click',togglePump);"
"document.getElementById('submit-limits').addEventListener('click',()=>{"
"const min=parseInt(document.getElementById('min-value').value);"
"const max=parseInt(document.getElementById('max-value').value);"
"if(min>=0&&max<=100&&min<=max){"
"fetch('/set-limits',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:`min=${min}&max=${max}`});"
"}else{alert('Valores inválidos! Certifique-se de que 0 ≤ min ≤ max ≤ 100.');}"
"});"
"setInterval(atualizar,1000);atualizar();"
"</script></body></html>";

// Prototipagem
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

//////////////////////////////////////////BASE PRONTA//////////////////////////////////////////////////////////////////////
// Display SSD1306
ssd1306_t ssd;

// Função para modularizar a inicialização do hardware
void inicializar_componentes()
{
    stdio_init_all();

    // Inicializa LED Vermelho
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_put(LED_RED, 0);

    // Inicializa LED Verde
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_put(LED_GREEN, 0);

    // Inicializa o pio
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    // Inicializa botao A
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);

    // Inicializa I2C para o display SSD1306
    i2c_init(i2c1, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Dados
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Clock
    // Define como resistor de pull-up interno
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa display
    ssd1306_init(&ssd, 128, 64, false, 0x3C, i2c1);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicializa buzzer
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    buzzer_slice = pwm_gpio_to_slice_num(BUZZER_PIN);              // Slice para o buzzer
    float clkdiv = 125.0f;                                         // Clock divisor
    uint16_t wrap = (uint16_t)((125000000 / (clkdiv * 1000)) - 1); // Valor do Wrap
    pwm_set_clkdiv(buzzer_slice, clkdiv);                          // Define o clock
    pwm_set_wrap(buzzer_slice, wrap);                              // Define o wrap
    pwm_set_gpio_level(BUZZER_PIN, wrap * 0.3f);                   // Define duty
    pwm_set_enabled(buzzer_slice, false);                          // Começa desligado
}

// WebServer: Início no main()
void iniciar_webserver()
{
    if (cyw43_arch_init())
        return; // Inicia o Wi-Fi
    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    { // Conecta ao Wi-Fi - loop
        printf("Falha ao conectar!\n");
        sleep_ms(3000);
    }
    printf("Conectado! IP: %s\n", ipaddr_ntoa(&netif_default->ip_addr)); // Conectado, e exibe o IP da rede no serial monitor

    struct tcp_pcb *server = tcp_new();    // Cria o servidor
    tcp_bind(server, IP_ADDR_ANY, 80);     // Binda na porta 80
    server = tcp_listen(server);           // Inicia o servidor
    tcp_accept(server, tcp_server_accept); // Aceita conexoes
}

// Aceita conexão TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, http_recv); // Recebe dados da conexao
    return ERR_OK;
}

// // Requisição HTTP
// static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
// {
//     if (!p)//Se o pacote estiver vazio
//     {
//         tcp_close(tpcb);    //Fecha a conexao
//         return ERR_OK;
//     }

//     char *req = (char *)p->payload; //Requisicao HTTP
    
//     const char *header_html =
//         "HTTP/1.1 200 OK\r\n"
//         "Content-Type: text/html\r\n"
//         "Connection: close\r\n"
//         "\r\n";

//     const char *header_json =
//         "HTTP/1.1 200 OK\r\n"
//         "Content-Type: application/json\r\n"
//         "Access-Control-Allow-Origin: *\r\n"
//         "Connection: close\r\n"
//         "\r\n";

//     if (p->len >= 6 && strncmp(req, "GET / ", 6) == 0)
//     {   //Requisicao GET com o header HTML
//         tcp_write(tpcb, header_html, strlen(header_html), TCP_WRITE_FLAG_COPY);
//         tcp_write(tpcb, HTML_BODY, strlen(HTML_BODY), TCP_WRITE_FLAG_COPY);
//     }
//     else if (p->len >= 9 && strncmp(req, "GET /data", 9) == 0)
//     {   //Requisicao GET com o header JSON
//         char json[64];
//         snprintf(json, sizeof(json), "{\"lvl\":%d,\"pump\":%s}", volume_litros, bomba_ligada ? "true" : "false");
//         tcp_write(tpcb, header_json, strlen(header_json), TCP_WRITE_FLAG_COPY);
//         tcp_write(tpcb, json, strlen(json), TCP_WRITE_FLAG_COPY);
//     }
//     else if (strstr(req, "POST /toggle") != NULL)
//     {   //Requisicao POST
//         acionar_bomba = true;
//         char json[64];
//         snprintf(json, sizeof(json), "{\"lvl\":%d,\"pump\":%s}", volume_litros, bomba_ligada ? "true" : "false");
//         tcp_write(tpcb, header_json, strlen(header_json), TCP_WRITE_FLAG_COPY);
//         tcp_write(tpcb, json, strlen(json), TCP_WRITE_FLAG_COPY);
//     }
//     else if (strstr(req, "POST /set-limits") != NULL)
//     {
//         // Procura os valores de min e max no corpo da requisição
//         char *body = strstr(req, "\r\n\r\n");
//         if (body) {
//             body += 4; // Pula os \r\n\r\n

//             int min = -1, max = -1;
//             sscanf(body, "min=%d&max=%d", &min, &max);
//             max /= 10;
//             if (min >= 0 && max <= 100 && min <= max) {
//                 limite_minimo = (uint8_t)min;
//                 limite_maximo = (uint8_t)max;
//             }
//         }

//         const char *ok_response =
//             "HTTP/1.1 200 OK\r\n"
//             "Content-Type: text/plain\r\n"
//             "Access-Control-Allow-Origin: *\r\n"
//             "Connection: close\r\n"
//             "\r\n"
//             "Limites atualizados.";
//         tcp_write(tpcb, ok_response, strlen(ok_response), TCP_WRITE_FLAG_COPY);
//     }
//     else
//     {
//         const char *not_found =
//             "HTTP/1.1 404 Not Found\r\n"
//             "Content-Type: text/plain\r\n"
//             "Connection: close\r\n"
//             "\r\n"
//             "Recurso não encontrado.";
//         tcp_write(tpcb, not_found, strlen(not_found), TCP_WRITE_FLAG_COPY);
//     }

//     tcp_recved(tpcb, p->tot_len);
//     pbuf_free(p);
//     tcp_close(tpcb); // Fecha a conexão após envio da resposta
//     return ERR_OK;
// }

// Debounce do botão (evita leituras falsas)
bool debounce_botao(uint gpio)
{
    static uint32_t ultimo_tempo = 0;
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    // Verifica se o botão foi pressionado e se passaram 200ms
    if (gpio_get(gpio) == 0 && (tempo_atual - ultimo_tempo) > 200)
    { // 200ms de debounce
        ultimo_tempo = tempo_atual;
        return true;
    }
    return false;
}

// Função de interrupção com Debouncing
void gpio_irq_handler(uint gpio, uint32_t events)
{
    uint32_t current_time = to_ms_since_boot(get_absolute_time()); // Variavel para armazenar o tempo atual
    // Caso o botão A seja pressionado
    if (gpio == BOTAO_A && debounce_botao(BOTAO_A))
    {

    }
}

int main()
{
    inicializar_componentes(); // Inicia os componentes
    iniciar_webserver();       // Inicia o webserver
    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    char ip_str[24];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    printf("IP: %s\n", ip_str); // Exibe o IP no serial monitor

    // Inicia a interrupção do botão A
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    while (true)
    {
        sleep_ms(1000);
    }
    return 0;
}