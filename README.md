# ATIVCAP4semaforo_interativo_pico

# **🚦 Semáforo Interativo com BitDogLab (RP2040)**  
**Projeto completo** que atende a todos os requisitos técnicos e funcionais de um semáforo inteligente, com:  
- Temporização automática por hardware  
- Botão de pedestre com interrupção  
- Buzzer PWM para acessibilidade  
- Logs no monitor serial  

---

## **📜 Requisitos Cumpridos**  

### **1. Requisitos Técnicos**  
| #  | **Requisito**                                      | **Implementação no Código**                                                                 |
|----|---------------------------------------------------|---------------------------------------------------------------------------------------------|
| 1  | Usar `hardware/timer.h`                           | Uso de `add_alarm_in_ms()` e `repeating_timer` 
| 2  | Utilizar alarmes, interrupções e callbacks        | - `main_timer_callback()` para transições<br>- `button_isr()` para o botão 
| 3  | Processamento fora do `loop`                      | Tudo é controlado por **interrupções** e timers (`tight_loop_contents()`) 
| 4  | Sem polling no botão                              | Botão tratado via **GPIO interrupt** (`GPIO_IRQ_EDGE_FALL`) 

---

### **2. Requisitos Funcionais**  
| #  | **Descrição**                                                                 | **Implementação no Código**                                                                 |
|----|-------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------|
| 1  | Iniciar no **vermelho (10s)**                                                | `set_red()` no `setup()` + timer de 10s
| 2  | Mudar para **verde (10s)**                                                   | Transição no `main_timer_callback()`
| 3  | **Amarelo (3s)** com LEDs vermelho + verde                                   | `set_yellow()` (GPIO 11 + 13) 
| 4  | **Botão de pedestre** interrompe o ciclo                                     | Aciona `STATE_PEDESTRIAN_YELLOW` 
| 5  | **Buzzer PWM (2kHz)** intermitente durante travessia                         | `play_buzzer_tone()` + `stop_buzzer_tone()` 
| 6  | **Logs no serial** (estados e botão pressionado)                             | `printf("Sinal: Vermelho")` e `printf("Botão acionado")` 

---

## **🛠️ Hardware Utilizado**  
- **BitDogLab (RP2040)**  
- **LED RGB**:  
  - Vermelho: GPIO 13  
  - Verde: GPIO 11  
  - Amarelo: Mistura de **GPIO 13 + 11** (requisito 3)  
- **Buzzer**: GPIO 21 (PWM via transistor)  
- **Botão**: GPIO 5 (pull-up interno)  

---

## **📌 Exemplo de Saída no Serial**  
```plaintext
[INICIALIZAÇÃO] Semáforo Interativo
[SINAL] Vermelho (10s)
[BOTÃO] Pedestre acionado
[SINAL] Amarelo (3s)
[PEDESTRE] 10 segundos restantes
```

## **🎯 Possiveis Melhorias**  
1. Adicionar display para cronômetro visível.  
2. Implementar modo noturno (piscar amarelo).  

--- 

## Propósito



Este projeto foi desenvolvido com fins estritamente educacionais e aprendizdo durante a residência em sistemas embarcados pelo embarcatech
