#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "pico/time.h"    // Necessário para repeating_timer
#include "hardware/pwm.h" // Necessário para PWM do buzzer

// ==============================================
// DEFINIÇÕES DE HARDWARE (Atendem aos requisitos de componentes)
// ==============================================
#define BUTTON_PIN 5      // Botão A (GPIO5) - Botoeira do pedestre
#define LED_RED 13        // Pino do LED Vermelho do semáforo
#define LED_GREEN 11      // Pino do LED Verde do semáforo
#define LED_BLUE 12       // Pino do LED Azul (não utilizado)
#define BUZZER_PIN 21     // Buzzer A (GPIO21) com transistor para controle PWM

// Frequência do alerta sonoro (acessibilidade para deficientes visuais)
#define PEDESTRIAN_ALERT_FREQ 2000 // 2kHz - frequência audível

// ==============================================
// ESTADOS DO SEMÁFORO (Atende ao requisito de máquina de estados)
// ==============================================
typedef enum {
    STATE_RED,             // Vermelho - tráfego parado
    STATE_GREEN,           // Verde - tráfego liberado
    STATE_YELLOW,          // Amarelo - atenção
    STATE_PEDESTRIAN_YELLOW // Amarelo especial para transição pedestre
} TrafficState;

// ==============================================
// VARIÁVEIS GLOBAIS (Atendem aos requisitos de temporização)
// ==============================================
volatile TrafficState current_state = STATE_RED; // Estado inicial (requisito 1)
volatile bool button_pressed_flag = false;       // Flag para debounce do botão
alarm_id_t main_timer_alarm_id = 0;             // ID do timer principal

// Controle PWM do buzzer (requisito de acessibilidade)
uint buzzer_pwm_slice_num;  // Slice PWM do RP2040
uint buzzer_pwm_chan;       // Canal PWM

// Temporizador do pedestre (requisito 4 e 5)
struct repeating_timer pedestrian_countdown_timer_obj;
volatile int pedestrian_countdown_value;  // Cronômetro regressivo
volatile bool pedestrian_walk_active = false; // Estado de travessia

// ==============================================
// CONTROLE DOS LEDS (Atende aos requisitos 1-3)
// ==============================================
void set_red() {
    gpio_put(LED_RED, 1);
    gpio_put(LED_GREEN, 0);
    gpio_put(LED_BLUE, 0);
    printf("Sinal: Vermelho\n"); // Requisito 6 - log serial
}

void set_green() {
    gpio_put(LED_RED, 0);
    gpio_put(LED_GREEN, 1);
    gpio_put(LED_BLUE, 0);
    printf("Sinal: Verde\n"); // Requisito 6
}

void set_yellow() {
    gpio_put(LED_RED, 1); // Vermelho + Verde = Amarelo (requisito 3)
    gpio_put(LED_GREEN, 1);
    gpio_put(LED_BLUE, 0);
    printf("Sinal: Amarelo\n"); // Requisito 6
}

// ==============================================
// CONTROLE DO BUZZER (Atende requisito de acessibilidade)
// ==============================================
void setup_buzzer_pwm() {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    buzzer_pwm_slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    buzzer_pwm_chan = pwm_gpio_to_channel(BUZZER_PIN);
    pwm_set_enabled(buzzer_pwm_slice_num, false); // Inicia desligado
}

void play_buzzer_tone(uint16_t freq) {
    if (freq == 0) {
        pwm_set_enabled(buzzer_pwm_slice_num, false);
        return;
    }
    // Configuração precisa do PWM (requisito técnico)
    pwm_set_clkdiv_int_frac(buzzer_pwm_slice_num, 100, 0);
    float wrap_float = 1250000.0f / freq;
    if (wrap_float < 1.0f) wrap_float = 1.0f;
    uint16_t wrap_value = (uint16_t)wrap_float;
    pwm_set_wrap(buzzer_pwm_slice_num, wrap_value);
    pwm_set_chan_level(buzzer_pwm_slice_num, buzzer_pwm_chan, wrap_value / 2); // 50% duty
    pwm_set_enabled(buzzer_pwm_slice_num, true);
}

void stop_buzzer_tone() {
    pwm_set_enabled(buzzer_pwm_slice_num, false);
}

// ==============================================
// TEMPORIZADOR DO PEDESTRE (Atende requisitos 4-5)
// ==============================================
bool pedestrian_countdown_timer_callback(struct repeating_timer *t) {
    if (pedestrian_countdown_value > 0) {
        printf("Pedestre: %d segundos restantes\n", pedestrian_countdown_value);
        
        // Alerta sonoro intermitente (1s liga, 1s desliga)
        static bool play_the_tone_this_second = false;
        play_the_tone_this_second = !play_the_tone_this_second;
        if (play_the_tone_this_second) {
            play_buzzer_tone(PEDESTRIAN_ALERT_FREQ);
        } else {
            stop_buzzer_tone();
        }
    }
    pedestrian_countdown_value--;

    if (pedestrian_countdown_value < 0) {
        stop_buzzer_tone();
        pedestrian_walk_active = false;
        return false; // Para o timer
    }
    return true;
}

// ==============================================
// ESTADOS PRINCIPAL 
// ==============================================
int64_t main_timer_callback(alarm_id_t id, void *user_data) {
    // Verifica se há travessia de pedestre em andamento
    if (pedestrian_walk_active && current_state == STATE_RED) {
        cancel_repeating_timer(&pedestrian_countdown_timer_obj);
        stop_buzzer_tone();
        pedestrian_walk_active = false;
    }

    // Transição de estados
    switch (current_state) {
        case STATE_RED:
            set_green();
            current_state = STATE_GREEN;
            main_timer_alarm_id = add_alarm_in_ms(10000, main_timer_callback, NULL, false); // 10s verde
            break;
            
        case STATE_GREEN:
            set_yellow();
            current_state = STATE_YELLOW;
            main_timer_alarm_id = add_alarm_in_ms(3000, main_timer_callback, NULL, false); // 3s amarelo
            break;
            
        case STATE_YELLOW:
            set_red();
            current_state = STATE_RED;
            main_timer_alarm_id = add_alarm_in_ms(10000, main_timer_callback, NULL, false); // 10s vermelho
            break;
            
        case STATE_PEDESTRIAN_YELLOW: // Requisito 5 - transição pedestre
            set_red();
            current_state = STATE_RED;
            
            // Inicia contagem regressiva para pedestre
            pedestrian_walk_active = true;
            button_pressed_flag = false;
            pedestrian_countdown_value = 10; // 10s para travessia
            add_repeating_timer_ms(-1000, pedestrian_countdown_timer_callback, NULL, &pedestrian_countdown_timer_obj);
            stop_buzzer_tone();
            
            main_timer_alarm_id = add_alarm_in_ms(10000, main_timer_callback, NULL, false); // 10s vermelho
            break;
    }
    return 0;
}

// ==============================================
// INTERRUPÇÃO DO BOTÃO (Atende requisito de não usar polling)
// ==============================================
void button_isr(uint gpio, uint32_t events) {
    if (gpio == BUTTON_PIN && (events & GPIO_IRQ_EDGE_FALL)) {
        static absolute_time_t last_press = 0;
        absolute_time_t now = get_absolute_time();
        // Debounce de 200ms (evita múltiplas ativações)
        if (absolute_time_diff_us(last_press, now) > 200000) {
            last_press = now;
            if (!button_pressed_flag) {
                button_pressed_flag = true;
                printf("Botão de Pedestres acionado\n"); // Requisito 6
                
                // Cancela o timer atual e inicia transição
                if (main_timer_alarm_id != 0) cancel_alarm(main_timer_alarm_id);
                set_yellow();
                current_state = STATE_PEDESTRIAN_YELLOW;
                main_timer_alarm_id = add_alarm_in_ms(3000, main_timer_callback, NULL, false); // 3s amarelo
            }
        }
    }
}

// ==============================================
// CONFIGURAÇÃO INICIAL (Setup)
// ==============================================
void setup() {
    stdio_init_all(); // Inicializa comunicação serial
    sleep_ms(1000);   // Espera inicialização
    printf("Semáforo Interativo Iniciado\n");

    // Configura LEDs
    gpio_init(LED_RED); gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_init(LED_GREEN); gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_init(LED_BLUE); gpio_set_dir(LED_BLUE, GPIO_OUT);
    
    // Configura Buzzer PWM
    setup_buzzer_pwm();
    stop_buzzer_tone();

    // Configura Botão com interrupção
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &button_isr);

    // Inicia ciclo (começa no vermelho por 10s - requisito 1)
    set_red();
    current_state = STATE_RED;
    main_timer_alarm_id = add_alarm_in_ms(10000, main_timer_callback, NULL, false);
}

// ==============================================
// LOOP PRINCIPAL (Vazio - tudo é feito por interrupções)
// ==============================================
int main() {
    setup();
    while (true) {
        tight_loop_contents(); // Economiza energia (requisito técnico)
    }
    return 0;
}