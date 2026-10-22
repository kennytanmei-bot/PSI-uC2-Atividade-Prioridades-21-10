#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(preempcao, LOG_LEVEL_DBG);

// --- Configuração de LEDs ---
#define LED_A_NODE DT_ALIAS(led0)
#define LED_B_NODE DT_ALIAS(led2)

static const struct gpio_dt_spec ledA = GPIO_DT_SPEC_GET(LED_A_NODE, gpios);
static const struct gpio_dt_spec ledB = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);

// --- Prioridades e tempos ---
#define PRIO_THREAD_A 5
#define PRIO_THREAD_B 7
#define TEMPO_A_MS   1500
#define TEMPO_B_MS   1000

// LEDs e Botão
#define LED_C_NODE  DT_ALIAS(led1)
#define BUTTON_NODE DT_NODELABEL(user_button_0)

static const struct gpio_dt_spec ledc   = GPIO_DT_SPEC_GET(LED_C_NODE, gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static struct gpio_callback button_cb_data;

// ISR - Toggle LED
void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    gpio_pin_toggle_dt(&ledc);
    LOG_INF("[ISR] botao pressionado em t=%u ms", k_uptime_get_32());
}

// THREAD A — Tarefa curta, alta prioridade
void thread_A(void *p1, void *p2, void *p3)
{
    while (1) {
        uint32_t startA = k_uptime_get_32();

        gpio_pin_set_dt(&ledA, 1);
        for (volatile int i = 0; i < 200000; i++) {}
        uint32_t midA = k_uptime_get_32();     // tempo intermediário
        gpio_pin_set_dt(&ledA, 0);

        uint32_t endA = k_uptime_get_32();

        LOG_DBG("[A] processamento terminou apos %u ms", midA - startA);
        LOG_INF("[A] inicio=%u fim=%u duracao=%u ms", startA, endA, endA - startA);

        k_msleep(TEMPO_A_MS);
    }
}

// THREAD B — Tarefa longa, baixa prioridade
void thread_B(void *p1, void *p2, void *p3)
{
    while (1) {
        uint32_t startB = k_uptime_get_32();

        gpio_pin_set_dt(&ledB, 1);
        for (volatile int i = 0; i < 10000000; i++) {}
        uint32_t midB = k_uptime_get_32();     // tempo intermediário
        gpio_pin_set_dt(&ledB, 0);

        uint32_t endB = k_uptime_get_32();

        LOG_DBG("[B] processamento terminou apos %u ms", midB - startB);
        LOG_INF("[B] inicio=%u fim=%u duracao=%u ms", startB, endB, endB - startB);

        k_msleep(TEMPO_B_MS);
    }
}

K_THREAD_DEFINE(a_tid, 1024, thread_A, NULL, NULL, NULL, PRIO_THREAD_A, 0, 0);
K_THREAD_DEFINE(b_tid, 1024, thread_B, NULL, NULL, NULL, PRIO_THREAD_B, 0, 0);

int main(void)
{
    if (!device_is_ready(ledA.port) || !device_is_ready(ledB.port)) {
        LOG_ERR("GPIO nao esta pronto");
        return 0;
    }
    gpio_pin_configure_dt(&ledA, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledB, GPIO_OUTPUT_INACTIVE);

    LOG_INF("=== Demonstracao de Preempcao com LEDs ===");
    LOG_INF("Thread A: prioridade %d | Thread B: prioridade %d",
            PRIO_THREAD_A, PRIO_THREAD_B);

    // Configurar LED e botão com pull-up
    gpio_pin_configure_dt(&ledc, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);

    // Configurar interrupção na borda de descida
    gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&button_cb_data, button_isr, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    while (1) {
        k_sleep(K_FOREVER);
    }
    return 0;
}