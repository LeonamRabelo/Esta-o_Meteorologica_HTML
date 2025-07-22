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

#define WIFI_SSID "USER"   // Alterar para o SSID da rede
#define WIFI_PASSWORD "PASSWORD" // Alterar para a senha da rede

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
#define LED_BLUE 12   // Pino do LED azul
#define LED_GREEN 11  // Pino do LED verde'
#define IS_RGBW false // Maquina PIO para RGBW

float temperatura = 0, umidade = 0, pressao = 0;            //Variáveis para armazenar os valores de temperatura, umidade e pressão
float limite_temp_min = 10.0, limite_temp_max = 40.0;       //Variáveis para armazenar os limites de temperatura
float limite_umi_min = 30.0, limite_umi_max = 80.0;         //Variáveis para armazenar os limites de umidade
float limite_pres_min = 900.0, limite_pres_max = 1020.0;    //Variáveis para armazenar os limites de pressão
float offSet_temp = 0.0f;                                   //Variável para armazenar o offset da temperatura
float offSet_umid = 0.0f;                                   //Variável para armazenar o offset da umidade
float offSet_pres = 0.0f;                                   //Variável para armazenar o offset da pressão

uint buzzer_slice;  //Variável para armazenar o slice do buzzer
ssd1306_t ssd;  //Estrutura para armazenar os dados do display
AHT20_Data dados_aht;   //Estrutura para armazenar os dados do sensor
struct bmp280_calib_param bmp_params;   //Estrutura para armazenar os parâmetros do sensor

//Definição do HTML
const char HTML_TEMPLATE[] =
"<!DOCTYPE html><html lang='pt-BR'><head><meta charset='UTF-8'/>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'/>"
"<title>Estação Meteorológica</title>"
"<style>"
"body{font-family:sans-serif;background:#e0f7fa;color:#01579b;margin:0;padding:0}"
"header{text-align:center;background:#01579b;color:white;padding:15px;font-size:1.8rem}"
".sensor{text-align:center;margin:15px 0}"
".sensor h2{margin:10px 0;font-size:1.2rem}"
".limits{text-align:center;margin:20px auto}"
".limits input{width:80px;padding:5px;margin:5px}"
".limits label{margin:5px;display:inline-block}"
"button{padding:10px 20px;background:#0077b6;border:none;color:white;border-radius:5px;cursor:pointer}"
"button:hover{background:#005f87}"
".graphs{display:flex;flex-wrap:wrap;justify-content:center;gap:20px;margin:20px}"
".graph-half{flex:1 1 300px;max-width:400px}"
".graph-full{width:100%;max-width:600px;margin:auto}"
"canvas{width:100%;height:250px;border:1px solid #ccc;border-radius:10px}"
"</style></head><body>"

"<header>Estação Meteorológica</header>"

"<div class='sensor'>"
"<h2>Temperatura: <span id='temp'>--</span>°C</h2>"
"<h2>Umidade: <span id='umi'>--</span>%%</h2>"
"<h2>Pressão: <span id='pres'>--</span>hPa</h2>"
"</div>"

"<div class='limits'>"
"<label>Temp Min: <input type='number' id='tempMin' value='%.1f'></label>"
"<label>Temp Max: <input type='number' id='tempMax' value='%.1f'></label><br>"
"<label>Umi Min: <input type='number' id='umiMin' value='%.1f'></label>"
"<label>Umi Max: <input type='number' id='umiMax' value='%.1f'></label><br>"
"<label>Pres Min: <input type='number' id='presMin' value='%.1f'></label>"
"<label>Pres Max: <input type='number' id='presMax' value='%.1f'></label><br>"
"<label>Offset Temp: <input type='number' id='offSetTemp' value='%.1f'></label>"
"<label>Offset Umi: <input type='number' id='offSetUmi' value='%.1f'></label>"
"<label>Offset Pres: <input type='number' id='offSetPres' value='%.1f'></label><br>"
"<button onclick='enviarLimites()'>Salvar</button>"
"</div>"

"<div class='graphs'>"
"<div class='graph-half'><canvas id='chartTemp'></canvas></div>"
"<div class='graph-half'><canvas id='chartUmi'></canvas></div>"
"<div class='graph-half'><canvas id='chartPres'></canvas></div>"
"</div>"

"<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>"
"<script>"
"function enviarLimites(){"
"const body=`tempMin=${tempMin.value}&tempMax=${tempMax.value}`+"
"`&umiMin=${umiMin.value}&umiMax=${umiMax.value}`+"
"`&presMin=${presMin.value}&presMax=${presMax.value}`+"
"`&offSetTemp=${offSetTemp.value}&offSetUmi=${offSetUmi.value}&offSetPres=${offSetPres.value}`;"
"fetch('/set-limits',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});}"

"function atualizar(){fetch('/data').then(res=>res.json()).then(data=>{"
"document.getElementById('temp').textContent=data.temp.toFixed(2);"
"document.getElementById('umi').textContent=data.umi.toFixed(2);"
"document.getElementById('pres').textContent=data.pres.toFixed(2);"

"chartTemp.data.labels.push(''); chartTemp.data.datasets[0].data.push(data.temp);"
"chartUmi.data.labels.push(''); chartUmi.data.datasets[0].data.push(data.umi);"
"chartPres.data.labels.push(''); chartPres.data.datasets[0].data.push(data.pres);"

"if(chartTemp.data.labels.length>20){"
"chartTemp.data.labels.shift(); chartTemp.data.datasets[0].data.shift();"
"chartUmi.data.labels.shift(); chartUmi.data.datasets[0].data.shift();"
"chartPres.data.labels.shift(); chartPres.data.datasets[0].data.shift();}"

"chartTemp.update(); chartUmi.update(); chartPres.update();}).catch(console.error);}"

"const chartTemp=new Chart(document.getElementById('chartTemp'),{type:'line',data:{labels:[],datasets:[{label:'Temperatura (°C)',data:[],borderColor:'red'}]},options:{responsive:true,animation:false}});"
"const chartUmi=new Chart(document.getElementById('chartUmi'),{type:'line',data:{labels:[],datasets:[{label:'Umidade (%)',data:[],borderColor:'blue'}]},options:{responsive:true,animation:false}});"
"const chartPres=new Chart(document.getElementById('chartPres'),{type:'line',data:{labels:[],datasets:[{label:'Pressão (hPa)',data:[],borderColor:'green'}]},options:{responsive:true,animation:false}});"
"setInterval(atualizar,2000); atualizar();"
"</script>"

"</body></html>";

// Prototipagem
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
void set_one_led(uint8_t r, uint8_t g, uint8_t b, int numero);

// Função para modularizar a inicialização do hardware
void inicializar_componentes(){
    stdio_init_all();

    //Inicializa o pio
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    //Configura os leds
    gpio_init(LED_RED); gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_init(LED_BLUE); gpio_set_dir(LED_BLUE, GPIO_OUT);
    gpio_init(LED_GREEN); gpio_set_dir(LED_GREEN, GPIO_OUT);

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

    //Configura o buzzer com PWM
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    buzzer_slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_clkdiv(buzzer_slice, 125.0f);
    pwm_set_wrap(buzzer_slice, 999);
    pwm_set_gpio_level(BUZZER_PIN, 300);
    pwm_set_enabled(buzzer_slice, false);
}

//WebServer: Início no main()
void iniciar_webserver(){
    if (cyw43_arch_init())
        return; // Inicia o Wi-Fi
    cyw43_arch_enable_sta_mode();

    gpio_put(LED_BLUE, 1);                                // Liga o LED azul para indicar que estamos conectando
    ssd1306_fill(&ssd, false);                            // Limpa o display
    ssd1306_draw_string(&ssd, "CONECTANDO...", 15, 20);   // Desenha uma string
    ssd1306_send_data(&ssd);                              // Envia os dados
    printf("Conectando ao Wi-Fi...\n");                    // Exibe uma mensagem no serial monitor
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)){ // Conecta ao Wi-Fi - loop
        printf("Falha ao conectar!\n");
        sleep_ms(3000);
    }
    printf("Conectado! IP: %s\n", ipaddr_ntoa(&netif_default->ip_addr)); // Conectado, e exibe o IP da rede no serial monitor
    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "Conexao feita", 10, 20);   // Desenha uma string
    ssd1306_send_data(&ssd);                              // Envia os dados
    gpio_put(LED_BLUE, 0);                                // Desliga o LED azul
    sleep_ms(1000);
    
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
#define MAX_REQ_LEN 2048    // Tamanho maximo da requisição
static char req_buffer[MAX_REQ_LEN];    // Buffer para armazenar a requisição
static int req_offset = 0;              //Offset da requisição

//Tratamento das requisições
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err){
    if (!p){    // Se o buffer estiver vazio
        tcp_close(tpcb);    // Fecha a conexao
        req_offset = 0;
        return ERR_OK;
    }
    //Acumula a requisição
    if(req_offset + p->len < MAX_REQ_LEN){     // Se o buffer nao estiver cheio
        memcpy(req_buffer + req_offset, p->payload, p->len);    // Copia o payload
        req_offset += p->len;
        req_buffer[req_offset] = '\0';
    }else{  // Se o buffer estiver cheio
        printf("Requisição muito grande!\n");
        tcp_close(tpcb);    // Fecha a conexao
        pbuf_free(p);
        req_offset = 0;
        return ERR_MEM;
    }

    //Verifica se chegou o corpo (\r\n\r\n)
    char *body = strstr(req_buffer, "\r\n\r\n");
    if(!body){
        tcp_recved(tpcb, p->tot_len);
        pbuf_free(p);
        return ERR_OK; //ainda não terminou
    }

    body += 4; //aponta pro início do corpo
    //printf(">>> BODY BRUTO:\n%s\n", body);    // Exibe o corpo na serial monitor, para testes, tava retornando vazio

    //Cabeçalhos
    const char *header_html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
    const char *header_json = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n";

    if (strncmp(req_buffer, "GET / ", 6) == 0){ // Se a requisição for para o index
        char html_final[4096];  // Buffer para armazenar o HTML
        //Cria o HTML com os dados atuais e os limites de temperatura, umidade e pressão, alem dos offsets
        snprintf(html_final, sizeof(html_final), HTML_TEMPLATE,
            limite_temp_min, limite_temp_max,
            limite_umi_min, limite_umi_max,
            limite_pres_min, limite_pres_max,
            offSet_temp, offSet_umid, offSet_pres);
        //Envia o HTML
        tcp_write(tpcb, header_html, strlen(header_html), TCP_WRITE_FLAG_COPY);
        tcp_write(tpcb, html_final, strlen(html_final), TCP_WRITE_FLAG_COPY);
    }
    //Verifica se a requisição e para /data
    else if (strncmp(req_buffer, "GET /data", 9) == 0){
        char json[128];
        //Cria o JSON com os dados atuais de temperatura, umidade e pressão
        snprintf(json, sizeof(json),
            "{\"temp\":%.2f,\"umi\":%.2f,\"pres\":%.2f}",
            temperatura, umidade, pressao);
        //Envia
        tcp_write(tpcb, header_json, strlen(header_json), TCP_WRITE_FLAG_COPY);
        tcp_write(tpcb, json, strlen(json), TCP_WRITE_FLAG_COPY);
    }
    //Verifica se a requisição e para /set-limits
    else if(strstr(req_buffer, "POST /set-limits") != NULL){
    char *body = strstr(req_buffer, "\r\n\r\n");
    if(body){   //Se encontrou o corpo
        int content_length = 0; //Tamanho do corpo
        char *cl_ptr = strstr(req_buffer, "Content-Length:");   //Procura pelo cabeçalho Content-Length
        if(cl_ptr) sscanf(cl_ptr, "Content-Length: %d", &content_length);   //Se encontrou, extrai o valor

        int header_len = (body + 4) - req_buffer;   //Tamanho do cabeçalho
        int total_expected_len = header_len + content_length;   //Tamanho total da requisição
        
        if(req_offset < total_expected_len){    //Se ainda nao chegou o corpo completo
            tcp_recved(tpcb, p->tot_len);       //Aumenta o total de bytes recebidos
            pbuf_free(p);                       //Libera o buffer
            return ERR_OK;
        }

        body += 4;  //Aponta pro início do corpo

        //Sanitiza vírgulas, para evitar problemas com o JSON de envio, com uso de float
        for(char *ptr = body; *ptr; ptr++){
            if (*ptr == ',') *ptr = '.';
        }

        //Variáveis temporárias
        float tMin = limite_temp_min;
        float tMax = limite_temp_max;
        float uMin = limite_umi_min;
        float uMax = limite_umi_max;
        float pMin = limite_pres_min;
        float pMax = limite_pres_max;
        float offsetT = offSet_temp;
        float offsetU = offSet_umid;
        float offsetP = offSet_pres;

        //Parse do corpo, para extrair os limites e offset, e atualizar as variáveis globais
        char *token = strtok(body, "&");
        while(token != NULL){
            sscanf(token, "tempMin=%f", &tMin);
            sscanf(token, "tempMax=%f", &tMax);
            sscanf(token, "umiMin=%f", &uMin);
            sscanf(token, "umiMax=%f", &uMax);
            sscanf(token, "presMin=%f", &pMin);
            sscanf(token, "presMax=%f", &pMax);
            sscanf(token, "offSetTemp=%f", &offsetT);
            sscanf(token, "offSetUmi=%f", &offsetU);
            sscanf(token, "offSetPres=%f", &offsetP);
            token = strtok(NULL, "&");
        }

        //Atualiza variáveis globais
        limite_temp_min = tMin;
        limite_temp_max = tMax;
        limite_umi_min = uMin;
        limite_umi_max = uMax;
        limite_pres_min = pMin;
        limite_pres_max = pMax;
        offSet_temp = offsetT;
        offSet_umid = offsetU;
        offSet_pres = offsetP;

        //Exibe os novos limites na serial
        printf(">>> NOVOS LIMITES:\n");
        printf("Temp: %.1f - %.1f\n", tMin, tMax);
        printf("Umi:  %.1f - %.1f\n", uMin, uMax);
        printf("Pres: %.1f - %.1f\n", pMin, pMax);
        printf("OffT: %.2f\n", offsetT);
        printf("OffU: %.2f\n", offsetU);
        printf("OffP: %.2f\n", offsetP);

        const char *ok = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nLimites atualizados.";
        tcp_write(tpcb, ok, strlen(ok), TCP_WRITE_FLAG_COPY);
    }else{
        const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nRecurso não encontrado.";
        tcp_write(tpcb, not_found, strlen(not_found), TCP_WRITE_FLAG_COPY);
    }
    }
    tcp_recved(tpcb, p->tot_len);   //Aumenta o total de bytes recebidos com a requisição completa
    pbuf_free(p);
    tcp_close(tpcb);
    req_offset = 0; //limpa o buffer
    return ERR_OK;
}

//Função para tocar os beeps do buzzer usando PWM com um intervalo de pausa como parâmetro e duracao
void tocar_beeps(int qtd_beeps, int duracao_ms, int pausa_ms){
    for (int i = 0; i < qtd_beeps; i++) {
        pwm_set_gpio_level(BUZZER_PIN, 500);
        pwm_set_enabled(buzzer_slice, true);
        sleep_ms(duracao_ms);
        pwm_set_enabled(buzzer_slice, false);
        sleep_ms(pausa_ms);
    }
}

//Função para checar os alertas, utilizando matriz de leds, led rgb e buzzer
void checar_alertas(){
    //Calcula a margem de erro para os limites de temperatura, umidade e pressão em 10%
    float margem_temp = (limite_temp_max - limite_temp_min) * 0.1f;
    float margem_umi  = (limite_umi_max  - limite_umi_min)  * 0.1f;
    float margem_pres = (limite_pres_max - limite_pres_min) * 0.1f;
    
    //Calcula o estado dos alertas de acordo com os limites de temperatura, umidade e pressão
    int estado_temp = 0, estado_umi = 0, estado_pres = 0;

    //Checa os limites de temperatura
    if(temperatura < limite_temp_min || temperatura > limite_temp_max)
        estado_temp = 2;
    else if(temperatura < limite_temp_min + margem_temp || temperatura > limite_temp_max - margem_temp)
        estado_temp = 1;
    //Checa os limites de umidade
    if(umidade < limite_umi_min || umidade > limite_umi_max)
        estado_umi = 2;
    else if(umidade < limite_umi_min + margem_umi || umidade > limite_umi_max - margem_umi)
        estado_umi = 1;
    //Checa os limites de pressão
    if(pressao < limite_pres_min || pressao > limite_pres_max)
        estado_pres = 2;
    else if(pressao < limite_pres_min + margem_pres || pressao > limite_pres_max - margem_pres)
        estado_pres = 1;

    //Calcula o nivel de alerta maior entre os 3 sensores
    int nivel_alerta = estado_temp;
    if(estado_umi > nivel_alerta) nivel_alerta = estado_umi;
    if(estado_pres > nivel_alerta) nivel_alerta = estado_pres;

    switch(nivel_alerta){
        case 0: //Verde - OK, sem alerta
            gpio_put(LED_GREEN, 1);
            gpio_put(LED_RED, 0);
            gpio_put(LED_BLUE, 0);
            pwm_set_enabled(buzzer_slice, false);
            set_one_led(0, 16, 0, 0);
            break;

        case 1: // Amarelo - 2 bipes curtos
            gpio_put(LED_GREEN, 1);
            gpio_put(LED_RED, 1);
            gpio_put(LED_BLUE, 0);
            tocar_beeps(2, 200, 300);  //2 beeps curtos
            set_one_led(16, 16, 0, 1);
            break;

        case 2: // Vermelho - 4 bipes curtos
            gpio_put(LED_GREEN, 0);
            gpio_put(LED_RED, 1);
            gpio_put(LED_BLUE, 0);
            tocar_beeps(4, 150, 200);  //4 beeps rápidos
            set_one_led(32, 0, 0, 2);
            break;
    }
}

int main(){
    inicializar_componentes(); //Inicia os componentes
    iniciar_webserver();       // Inicia o webserver

    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    char ip_str[24];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    printf("IP: %s\n", ip_str); // Exibe o IP no serial monitor

    //Estrutura para armazenar os dados do sensor
    AHT20_Data data;
    int32_t raw_temp_bmp;
    int32_t raw_pressure;

    char str_tmp1[10];  // Buffer para armazenar a string
    char str_tmp2[10];  // Buffer para armazenar a string
    char str_umi[10];  // Buffer para armazenar a string  
    char str_pres[10];  // Buffer para armazenar a string  

    while(true){
        //Leitura do BMP280
        bmp280_read_raw(I2C_PORT_SENSOR, &raw_temp_bmp, &raw_pressure);
        float temperatura_bmp = bmp280_convert_temp(raw_temp_bmp, &bmp_params);
        pressao = (bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &bmp_params) / 100.0f) + offSet_pres;
        //Leitura do AHT20
        if(aht20_read(I2C_PORT_SENSOR, &data)){
            printf("Temperatura AHT: %.2f C\n", data.temperature);
            printf("Umidade: %.2f %%\n\n\n", data.humidity);
            umidade = data.humidity + offSet_umid;
        }
        else{
            printf("Erro na leitura do AHT20!\n\n\n");
        }

        //Para enviar para o html, utilizei a media da soma dos dois sensores e jogar no grafico
        temperatura = (((temperatura_bmp / 100.0f) + data.temperature) / 2.0f) + offSet_temp;
        checar_alertas();

        sprintf(str_tmp1, "%.1fC", temperatura_bmp / 100.0);  // Converte o inteiro em string
        sprintf(str_tmp2, "%.1fC", data.temperature);  // Converte o inteiro em string
        sprintf(str_umi, "%.1f%%", data.humidity);  // Converte o inteiro em string  
        sprintf(str_pres, "%.1fh", pressao);  // Converte o inteiro em string      
    
        //Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, false);                           // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);       // Desenha um retângulo
        ssd1306_line(&ssd, 3, 25, 123, 25, true);            // Desenha uma linha
        ssd1306_line(&ssd, 3, 37, 123, 37, true);            // Desenha uma linha
        ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6);  // Desenha uma string
        ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16);   // Desenha uma string
        ssd1306_draw_string(&ssd, "BMP280  AHT20", 10, 28); // Desenha uma string
        ssd1306_line(&ssd, 63, 25, 63, 60, true);            // Desenha uma linha vertical
        ssd1306_draw_string(&ssd, str_tmp1, 14, 41);             // Desenha uma string
        ssd1306_draw_string(&ssd, str_pres, 10, 52);             // Desenha uma string
        ssd1306_draw_string(&ssd, str_tmp2, 73, 41);             // Desenha uma string
        ssd1306_draw_string(&ssd, str_umi, 73, 52);            // Desenha uma string
        ssd1306_send_data(&ssd);                        // Envia os dados para o display

        sleep_ms(500);
    }
    return 0;
}