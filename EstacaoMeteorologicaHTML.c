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
#include "aht20.h"
#include "bmp280.h"

#define WIFI_SSID "PRF"   // Alterar para o SSID da rede
#define WIFI_PASSWORD "@hfs0800" // Alterar para a senha da rede

//Definição de pinagem
#define I2C_PORT_SENSOR i2c0
#define I2C_PORT_DISPLAY i2c1
#define I2C_SDA_SENSOR 0    // Pino SDA - Dados
#define I2C_SCL_SENSOR 1    // Pino SCL - Clock
#define I2C_SDA_DISPLAY 14    // Pino SDA - Dados
#define I2C_SCL_DISPLAY 15    // Pino SCL - Clock
#define WS2812_PIN 7  // Pino do WS2812
#define BUZZER_PIN 21 // Pino do buzzer
#define BOTAO_A 5     // Pino do botao A
#define LED_RED 13    // Pino do LED vermelho
#define LED_GREEN 12  // Pino do LED verde'
#define IS_RGBW false // Maquina PIO para RGBW

uint volatile numero = 0;            // Variável para inicializar o numero com 0 (WS2812B)
float temperatura = 0, umidade = 0, pressao = 0;
uint8_t limite_temp_min = 0, limite_temp_max = 40;
uint8_t limite_umi_min = 40, limite_umi_max = 80;
uint8_t limite_pres_min = 900, limite_pres_max = 1020;
uint buzzer_slice;
ssd1306_t ssd;
AHT20_Data dados_aht;
struct bmp280_calib_param bmp_params;

const char HTML_BODY[] = 
"<!DOCTYPE html><html lang='pt-BR'><head><meta charset='UTF-8'/>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'/>"
"<title>Estação Meteorológica</title>"
"<style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:sans-serif;background:#e0f7fa;color:#01579b;min-height:100vh}"
"header{background:#0077b6;padding:10px;text-align:center;color:white;font-size:1.5rem}"
".sensor{margin:20px auto;text-align:center}.sensor h2{margin:10px 0}.sensor span{font-weight:bold;font-size:1.3rem}"
".graph{width:90%;margin:20px auto;display:flex;flex-wrap:wrap;justify-content:center;gap:20px}"
".graph canvas{width:100%;max-width:500px;height:250px;border:1px solid #ccc;border-radius:10px}"
".limits{margin:30px auto;text-align:center}.limits input{margin:5px;padding:5px;width:80px}"
"button{margin-top:10px;padding:10px 20px;background:#0077b6;border:none;color:white;border-radius:5px;cursor:pointer}"
"button:hover{background:#005f87}"
".row{display:flex;flex-wrap:wrap;justify-content:center;gap:20px}"
".row canvas{flex:1 1 45%;}"
".full-width{width:100%;max-width:600px;margin:auto}"
"</style></head><body>"
"<header>Estação Meteorológica IoT</header>"
"<div class='sensor'>"
"<h2>Temperatura: <span id='temp'>--</span> °C</h2>"
"<h2>Umidade: <span id='umi'>--</span> %</h2>"
"<h2>Pressão: <span id='pres'>--</span> hPa</h2>"
"</div>"
"<div class='limits'>"
"<h3>Definir Limites</h3>"
"<label>Temp Min: <input type='number' id='tempMin' value='18'></label>"
"<label>Temp Max: <input type='number' id='tempMax' value='32'></label><br>"
"<label>Umi Min: <input type='number' id='umiMin' value='40'></label>"
"<label>Umi Max: <input type='number' id='umiMax' value='80'></label><br>"
"<label>Pres Min: <input type='number' id='presMin' value='900'></label>"
"<label>Pres Max: <input type='number' id='presMax' value='1020'></label><br>"
"<button onclick='enviarLimites()'>Salvar Limites</button>"
"</div>"
"<div class='graph'>"
"<div class='row'>"
"<canvas id='chartTemp'></canvas>"
"<canvas id='chartUmi'></canvas>"
"</div>"
"<div class='full-width'>"
"<canvas id='chartPres'></canvas>"
"</div>"
"</div>"
"<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>"
"<script>"
"function enviarLimites(){"
"const body=\"tempMin=\"+tempMin.value+\"&tempMax=\"+tempMax.value+\"&umiMin=\"+umiMin.value+\"&umiMax=\"+umiMax.value+\"&presMin=\"+presMin.value+\"&presMax=\"+presMax.value;"
"fetch('/set-limits',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});}"
"function atualizar(){fetch('/data').then(res=>res.json()).then(data=>{"
"document.getElementById('temp').textContent=data.temp.toFixed(2);"
"document.getElementById('umi').textContent=data.umi.toFixed(2);"
"document.getElementById('pres').textContent=data.pres.toFixed(2);"
"chartTemp.data.labels.push('');chartTemp.data.datasets[0].data.push(data.temp);"
"chartUmi.data.labels.push('');chartUmi.data.datasets[0].data.push(data.umi);"
"chartPres.data.labels.push('');chartPres.data.datasets[0].data.push(data.pres);"
"if(chartTemp.data.labels.length>20){chartTemp.data.labels.shift();chartTemp.data.datasets[0].data.shift();chartUmi.data.labels.shift();chartUmi.data.datasets[0].data.shift();chartPres.data.labels.shift();chartPres.data.datasets[0].data.shift();}"
"chartTemp.update();chartUmi.update();chartPres.update();}).catch(console.error);}"
"const ctxT=document.getElementById('chartTemp').getContext('2d');"
"const ctxU=document.getElementById('chartUmi').getContext('2d');"
"const ctxP=document.getElementById('chartPres').getContext('2d');"
"const chartTemp=new Chart(ctxT,{type:'line',data:{labels:[],datasets:[{label:'Temperatura (°C)',data:[],borderColor:'red'}]},options:{responsive:true,animation:false}});"
"const chartUmi=new Chart(ctxU,{type:'line',data:{labels:[],datasets:[{label:'Umidade (%)',data:[],borderColor:'blue'}]},options:{responsive:true,animation:false}});"
"const chartPres=new Chart(ctxP,{type:'line',data:{labels:[],datasets:[{label:'Pressão (hPa)',data:[],borderColor:'green'}]},options:{responsive:true,animation:false}});"
"setInterval(atualizar,2000);atualizar();"
"</script></body></html>";

// Prototipagem
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

//////////////////////////////////////////BASE PRONTA//////////////////////////////////////////////////////////////////////
// Display SSD1306
ssd1306_t ssd;

// Função para modularizar a inicialização do hardware
void inicializar_componentes(){
    stdio_init_all();
    gpio_init(LED_RED); gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_init(LED_GREEN); gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_init(BOTAO_A); gpio_set_dir(BOTAO_A, GPIO_IN); gpio_pull_up(BOTAO_A);

    //Configura o I2C na porta i2c1 para o display
    i2c_init(I2C_PORT_DISPLAY, 400 * 1000);
    gpio_set_function(I2C_SDA_DISPLAY, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISPLAY, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISPLAY); gpio_pull_up(I2C_SCL_DISPLAY);
    ssd1306_init(&ssd, 128, 64, false, 0x3C, I2C_PORT_DISPLAY);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    //Configura o I2C na porta i2c0 para os sensores
    i2c_init(I2C_PORT_SENSOR, 400 * 1000);
    gpio_set_function(I2C_SDA_SENSOR, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_SENSOR, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_SENSOR); gpio_pull_up(I2C_SCL_SENSOR);
    //bmp280
    bmp280_init(I2C_PORT_SENSOR);
    bmp280_get_calib_params(I2C_PORT_SENSOR, &bmp_params);
    //aht20
    aht20_reset(I2C_PORT_SENSOR);
    aht20_init(I2C_PORT_SENSOR);

    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    buzzer_slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_clkdiv(buzzer_slice, 125.0f);
    pwm_set_wrap(buzzer_slice, 999);
    pwm_set_gpio_level(BUZZER_PIN, 300);
    pwm_set_enabled(buzzer_slice, false);
}

// WebServer: Início no main()
void iniciar_webserver(){
    if (cyw43_arch_init())
        return; // Inicia o Wi-Fi
    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)){ // Conecta ao Wi-Fi - loop
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
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err){
    tcp_recv(newpcb, http_recv); // Recebe dados da conexao
    return ERR_OK;
}

// Requisição HTTP
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err){
    if(!p){
        tcp_close(tpcb);
        return ERR_OK;
    }
    char *req = (char *)p->payload;
    const char *header_html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
    const char *header_json = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n";

    if(strncmp(req, "GET / ", 6) == 0){
        tcp_write(tpcb, header_html, strlen(header_html), TCP_WRITE_FLAG_COPY);
        tcp_write(tpcb, HTML_BODY, strlen(HTML_BODY), TCP_WRITE_FLAG_COPY);
    }else if(strncmp(req, "GET /data", 9) == 0) {
        char json[128];
        snprintf(json, sizeof(json), "{\"temp\":%.2f,\"umi\":%.2f,\"pres\":%.2f}", temperatura / 100.0, umidade, pressao);
        tcp_write(tpcb, header_json, strlen(header_json), TCP_WRITE_FLAG_COPY);
        tcp_write(tpcb, json, strlen(json), TCP_WRITE_FLAG_COPY);
    }else if(strstr(req, "POST /set-limits") != NULL){
        char *body = strstr(req, "\r\n\r\n");
        if(body){
            body += 4;
            sscanf(body, "tempMin=%hhu&tempMax=%hhu&umiMin=%hhu&umiMax=%hhu&presMin=%hhu&presMax=%hhu",
                &limite_temp_min, &limite_temp_max,
                &limite_umi_min, &limite_umi_max,
                &limite_pres_min, &limite_pres_max);
        }
        const char *ok = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nLimites atualizados.";
        tcp_write(tpcb, ok, strlen(ok), TCP_WRITE_FLAG_COPY);
    }else{
        const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nRecurso não encontrado.";
        tcp_write(tpcb, not_found, strlen(not_found), TCP_WRITE_FLAG_COPY);
    }
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    tcp_close(tpcb);
    return ERR_OK;
}

// Debounce do botão (evita leituras falsas)
bool debounce_botao(uint gpio){
    static uint32_t ultimo_tempo = 0;
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    // Verifica se o botão foi pressionado e se passaram 200ms
    if (gpio_get(gpio) == 0 && (tempo_atual - ultimo_tempo) > 200){ // 200ms de debounce
        ultimo_tempo = tempo_atual;
        return true;
    }
    return false;
}

// Função de interrupção com Debouncing
void gpio_irq_handler(uint gpio, uint32_t events){
    uint32_t current_time = to_ms_since_boot(get_absolute_time()); // Variavel para armazenar o tempo atual
    // Caso o botão A seja pressionado
    if(gpio == BOTAO_A && debounce_botao(BOTAO_A)){

    }
}

int main(){
    inicializar_componentes(); // Inicia os componentes
    iniciar_webserver();       // Inicia o webserver
    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    char ip_str[24];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    printf("IP: %s\n", ip_str); // Exibe o IP no serial monitor

    //Inicia a interrupção do botão A
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    //Estrutura para armazenar os dados do sensor
    AHT20_Data data;
    int32_t raw_temp_bmp;
    int32_t raw_pressure;

    char str_tmp1[5];  // Buffer para armazenar a string
    char str_tmp2[5];  // Buffer para armazenar a string
    char str_umi[5];  // Buffer para armazenar a string    

    while (true){
        //Leitura do BMP280
        bmp280_read_raw(I2C_PORT_SENSOR, &raw_temp_bmp, &raw_pressure);
        temperatura = bmp280_convert_temp(raw_temp_bmp, &bmp_params);
        pressao = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &bmp_params);
        //Leitura do AHT20
        if (aht20_read(I2C_PORT_SENSOR, &data)){
            printf("Temperatura AHT: %.2f C\n", data.temperature);
            printf("Umidade: %.2f %%\n\n\n", data.humidity);
            umidade = data.humidity;
        }
        else{
            printf("Erro na leitura do AHT20!\n\n\n");
        }
        sprintf(str_tmp1, "%.1fC", temperatura / 100.0);  // Converte o inteiro em string
        sprintf(str_tmp2, "%.1fC", data.temperature);  // Converte o inteiro em string
        sprintf(str_umi, "%.1f%%", data.humidity);  // Converte o inteiro em string        
    
        //  Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, false);                           // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);       // Desenha um retângulo
        ssd1306_line(&ssd, 3, 25, 123, 25, true);            // Desenha uma linha
        ssd1306_line(&ssd, 3, 37, 123, 37, true);            // Desenha uma linha
        ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6);  // Desenha uma string
        ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16);   // Desenha uma string
        ssd1306_draw_string(&ssd, "BMP280  AHT20", 10, 28); // Desenha uma string
        ssd1306_line(&ssd, 63, 25, 63, 60, true);            // Desenha uma linha vertical
        ssd1306_draw_string(&ssd, str_tmp1, 14, 41);             // Desenha uma string
        ssd1306_draw_string(&ssd, str_tmp2, 73, 41);             // Desenha uma string
        ssd1306_draw_string(&ssd, str_umi, 73, 52);            // Desenha uma string
        ssd1306_send_data(&ssd);                            // Atualiza o display

        sleep_ms(500);
    }
    return 0;
}