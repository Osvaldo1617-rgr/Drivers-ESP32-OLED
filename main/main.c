#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "rom/ets_sys.h"
#include "soc/i2c_reg.h"
#include "soc/interrupts.h"   // Contiene las fuentes de interrupción del ESP32 clásico
#include "esp_intr_alloc.h"    // Manejo directo de interrupciones por hardware
#include "driver/i2c.h"

#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22
#define I2C_PUERTO   I2C_NUM_0

static volatile bool i2c_bus_libre = true;

/**
 * @brief Rutina de Servicio de Interrupción (ISR) para el I2C.
 * Registrada en la memoria rápida IRAM para cumplir la regla de rapidez.
 */
static void IRAM_ATTR oled_i2c_isr(void *arg) {
    // 1. Leer el registro de interrupciones del hardware I2C0
    uint32_t status = READ_PERI_REG(I2C_INT_STATUS_REG(I2C_PUERTO));

    // 2. Si terminó la transmisión, levantamos la bandera para el superloop
    if (status & I2C_MASTER_TRAN_COMP_INT_ST) {
        i2c_bus_libre = true; 
    }

    // 3. Limpiar explícitamente los flags por hardware escribiendo en el registro clear
    WRITE_PERI_REG(I2C_INT_CLR_REG(I2C_PUERTO), status);
}

/**
 * @brief Inicializa el hardware I2C y enlaza la ISR a nivel de registros
 */
esp_err_t oled_hardware_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = OLED_SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = OLED_SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000 // 100 kHz
    };
    
    // Configuración inicial de pines y velocidad
    esp_err_t err = i2c_param_config(I2C_PUERTO, &conf);
    if (err != ESP_OK) return err;

    // Instalar driver de forma limpia (sin colas de FreeRTOS)
    err = i2c_driver_install(I2C_PUERTO, conf.mode, 0, 0, 0);
    if (err != ESP_OK) return err;

    // Habilitar la interrupción de transmisión completa en el periférico
    SET_PERI_REG_MASK(I2C_INT_ENA_REG(I2C_PUERTO), I2C_MASTER_TRAN_COMP_INT_ENA);

    // SOLUCIÓN V6.1: Reservar y enlazar la interrupción física del I2C0 directamente
    err = esp_intr_alloc(ETS_I2C_EXT0_INTR_SOURCE, ESP_INTR_FLAG_IRAM, oled_i2c_isr, NULL, NULL);
    
    return err;
}

void oled_driver_service(void) {
    if (i2c_bus_libre) {
        // Aquí añadiremos la lógica para el OLED
    }
}

void app_main(void) {
    printf("--- Driver OLED Baremetal Inicializado v6.1 ---\n");

    if (oled_hardware_init() == ESP_OK) {
        printf("[OK] Hardware I2C y asignación de ISR listos de forma baremetal.\n");
    }

    // El Superloop obligatorio corregido para evitar el Watchdog
    for (;;) {
        oled_driver_service();
        
        // Alimenta al perro guardián del sistema permitiendo un parpadeo mínimo de descanso
        // Esta función es un macro de bajo nivel compatible con el enfoque baremetal
        vTaskDelay(1); 
    }
}
