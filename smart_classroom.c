#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_rom_sys.h"

#define WIFI_SSID               "YourWiFiSSID"
#define WIFI_PASSWORD           "YourWiFiPassword"
#define BLYNK_AUTH_TOKEN        "YourBlynkAuthToken"
#define BLYNK_HOST              "blynk.cloud"

#define PIR_FRONT_PIN           GPIO_NUM_5
#define PIR_MIDDLE_PIN          GPIO_NUM_18
#define PIR_BACK_PIN            GPIO_NUM_19

#define RELAY_FRONT_PIN         GPIO_NUM_25
#define RELAY_MIDDLE_PIN        GPIO_NUM_26
#define RELAY_BACK_PIN          GPIO_NUM_27

#define DHT22_PIN               GPIO_NUM_4

#define I2C_SDA_PIN             GPIO_NUM_21
#define I2C_SCL_PIN             GPIO_NUM_22
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ      100000
#define LCD_ADDR                0x27
#define LCD_COLS                16
#define LCD_ROWS                2

#define ZONE_COUNT              3
#define DHT_RETRIES             3
#define SENSOR_POLL_MS          500
#define DHT_INTERVAL_MS         2000
#define BLYNK_INTERVAL_MS       10000

#define TEMP_HIGH               30.0f
#define TEMP_MID                27.0f
#define FAN_DELAY_HIGH_MS       (9  * 60 * 1000)
#define FAN_DELAY_MID_MS        (6  * 60 * 1000)
#define FAN_DELAY_LOW_MS        (2  * 60 * 1000)

static const char *TAG = "SmartClassroom";

typedef struct {
    const char  *name;
    gpio_num_t   pir_pin;
    gpio_num_t   relay_pin;
    int          occupied;
    uint32_t     last_motion_ms;
    uint32_t     fan_delay_ms;
} Zone;

static Zone zones[ZONE_COUNT] = {
    { "Front",  PIR_FRONT_PIN,  RELAY_FRONT_PIN,  0, 0, 0 },
    { "Middle", PIR_MIDDLE_PIN, RELAY_MIDDLE_PIN, 0, 0, 0 },
    { "Back",   PIR_BACK_PIN,   RELAY_BACK_PIN,   0, 0, 0 },
};

static float    g_temperature = 0.0f;
static float    g_humidity    = 0.0f;

void     wifi_init(void);
void     gpio_init(void);
void     i2c_init(void);
void     lcd_send_cmd(uint8_t cmd);
void     lcd_send_data(uint8_t data);
void     lcd_init(void);
void     lcd_set_cursor(uint8_t col, uint8_t row);
void     lcd_print(const char *str);
void     lcd_clear(void);
int      dht22_read(float *temp, float *hum);
uint32_t get_fan_delay(float temp);
void     zone_update(Zone *z, uint32_t now_ms);
void     blynk_send(int vpin, float value);
void     blynk_send_str(int vpin, const char *str);
void     sensor_task(void *pv);
void     display_task(void *pv);
void     blynk_task(void *pv);

void wifi_init(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_cfg = {
        .sta = { .ssid = WIFI_SSID, .password = WIFI_PASSWORD }
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();
    esp_wifi_connect();
    ESP_LOGI(TAG, "Connecting to WiFi...");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "WiFi Ready");
}

void gpio_init(void)
{
    gpio_num_t pir_pins[]   = { PIR_FRONT_PIN,   PIR_MIDDLE_PIN,   PIR_BACK_PIN   };
    gpio_num_t relay_pins[] = { RELAY_FRONT_PIN, RELAY_MIDDLE_PIN, RELAY_BACK_PIN };

    for (int i = 0; i < ZONE_COUNT; i++)
    {
        gpio_set_direction(pir_pins[i],   GPIO_MODE_INPUT);
        gpio_set_pull_mode(pir_pins[i],   GPIO_PULLDOWN_ONLY);

        gpio_set_direction(relay_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(relay_pins[i],     0);
    }
}

void i2c_init(void)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_SDA_PIN,
        .scl_io_num       = I2C_SCL_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

static void lcd_write_nibble(uint8_t nibble, uint8_t mode)
{
    uint8_t data = (nibble & 0xF0) | mode | 0x08;
    uint8_t buf[4];
    buf[0] = data | 0x04;
    buf[1] = data;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (LCD_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, buf, 2, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

void lcd_send_cmd(uint8_t cmd)
{
    lcd_write_nibble(cmd & 0xF0, 0x00);
    lcd_write_nibble((cmd << 4) & 0xF0, 0x00);
    vTaskDelay(2 / portTICK_PERIOD_MS);
}

void lcd_send_data(uint8_t data)
{
    lcd_write_nibble(data & 0xF0, 0x01);
    lcd_write_nibble((data << 4) & 0xF0, 0x01);
}

void lcd_init(void)
{
    vTaskDelay(50 / portTICK_PERIOD_MS);
    lcd_write_nibble(0x30, 0x00); vTaskDelay(5 / portTICK_PERIOD_MS);
    lcd_write_nibble(0x30, 0x00); vTaskDelay(1 / portTICK_PERIOD_MS);
    lcd_write_nibble(0x30, 0x00);
    lcd_write_nibble(0x20, 0x00);
    lcd_send_cmd(0x28);
    lcd_send_cmd(0x0C);
    lcd_send_cmd(0x06);
    lcd_send_cmd(0x01);
    vTaskDelay(2 / portTICK_PERIOD_MS);
}

void lcd_set_cursor(uint8_t col, uint8_t row)
{
    uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
    lcd_send_cmd(addr);
}

void lcd_print(const char *str)
{
    while (*str) lcd_send_data((uint8_t)(*str++));
}

void lcd_clear(void)
{
    lcd_send_cmd(0x01);
    vTaskDelay(2 / portTICK_PERIOD_MS);
}

int dht22_read(float *temp, float *hum)
{
    uint8_t  data[5] = {0};
    uint32_t count   = 0;

    gpio_set_direction(DHT22_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT22_PIN, 0);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    gpio_set_level(DHT22_PIN, 1);
    esp_rom_delay_us(30);
    gpio_set_direction(DHT22_PIN, GPIO_MODE_INPUT);

    count = 0;
    while (gpio_get_level(DHT22_PIN) == 0 && ++count < 10000);
    count = 0;
    while (gpio_get_level(DHT22_PIN) == 1 && ++count < 10000);

    for (int i = 0; i < 40; i++)
    {
        count = 0;
        while (gpio_get_level(DHT22_PIN) == 0 && ++count < 10000);
        esp_rom_delay_us(35);
        data[i / 8] <<= 1;
        if (gpio_get_level(DHT22_PIN)) data[i / 8] |= 1;
        count = 0;
        while (gpio_get_level(DHT22_PIN) == 1 && ++count < 10000);
    }

    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
        return 0;

    *hum  = ((data[0] << 8) | data[1]) / 10.0f;
    *temp = (((data[2] & 0x7F) << 8) | data[3]) / 10.0f;
    if (data[2] & 0x80) *temp = -(*temp);
    return 1;
}

uint32_t get_fan_delay(float temp)
{
    if      (temp > TEMP_HIGH) return FAN_DELAY_HIGH_MS;
    else if (temp > TEMP_MID)  return FAN_DELAY_MID_MS;
    else                       return FAN_DELAY_LOW_MS;
}

void zone_update(Zone *z, uint32_t now_ms)
{
    int pir = gpio_get_level(z->pir_pin);

    if (pir == 1)
    {
        z->occupied      = 1;
        z->last_motion_ms = now_ms;
        z->fan_delay_ms  = get_fan_delay(g_temperature);
        gpio_set_level(z->relay_pin, 1);
        ESP_LOGI(TAG, "[%s] Motion detected. Relay ON. Temp=%.1fC Delay=%lums",
            z->name, g_temperature, z->fan_delay_ms);
    }
    else
    {
        uint32_t elapsed = now_ms - z->last_motion_ms;
        if (elapsed >= z->fan_delay_ms)
        {
            if (z->occupied)
            {
                z->occupied = 0;
                gpio_set_level(z->relay_pin, 0);
                ESP_LOGI(TAG, "[%s] No motion. Delay expired. Relay OFF.", z->name);
            }
        }
    }
}

void blynk_send(int vpin, float value)
{
    char url[256];
    snprintf(url, sizeof(url),
        "http://%s/%s/update/V%d?value=%.1f",
        BLYNK_HOST, BLYNK_AUTH_TOKEN, vpin, value);
    esp_http_client_config_t config = { .url = url };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

void blynk_send_str(int vpin, const char *str)
{
    char url[512];
    snprintf(url, sizeof(url),
        "http://%s/%s/update/V%d?value=%s",
        BLYNK_HOST, BLYNK_AUTH_TOKEN, vpin, str);
    esp_http_client_config_t config = { .url = url };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

void sensor_task(void *pv)
{
    uint32_t last_dht_ms = 0;

    while (1)
    {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if ((now - last_dht_ms) >= DHT_INTERVAL_MS)
        {
            float t = 0, h = 0;
            int   ok = 0;
            for (int i = 0; i < DHT_RETRIES && !ok; i++)
            {
                ok = dht22_read(&t, &h);
                if (!ok) vTaskDelay(200 / portTICK_PERIOD_MS);
            }
            if (ok) { g_temperature = t; g_humidity = h; }
            else    { ESP_LOGW(TAG, "DHT22 read failed"); }
            last_dht_ms = now;
        }

        for (int i = 0; i < ZONE_COUNT; i++)
            zone_update(&zones[i], now);

        vTaskDelay(SENSOR_POLL_MS / portTICK_PERIOD_MS);
    }
}

void display_task(void *pv)
{
    char line1[17], line2[17];

    while (1)
    {
        lcd_clear();

        snprintf(line1, sizeof(line1), "T:%.1fC H:%.1f%%", g_temperature, g_humidity);
        lcd_set_cursor(0, 0);
        lcd_print(line1);

        snprintf(line2, sizeof(line2), "F:%s M:%s B:%s",
            zones[0].occupied ? "ON " : "OFF",
            zones[1].occupied ? "ON " : "OFF",
            zones[2].occupied ? "ON " : "OFF");
        lcd_set_cursor(0, 1);
        lcd_print(line2);

        ESP_LOGI(TAG, "LCD | %s | %s", line1, line2);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void blynk_task(void *pv)
{
    while (1)
    {
        vTaskDelay(BLYNK_INTERVAL_MS / portTICK_PERIOD_MS);

        blynk_send(0, g_temperature);
        blynk_send(1, g_humidity);
        blynk_send(2, (float)zones[0].occupied);
        blynk_send(3, (float)zones[1].occupied);
        blynk_send(4, (float)zones[2].occupied);

        char status[64];
        snprintf(status, sizeof(status), "Front:%s Middle:%s Back:%s",
            zones[0].occupied ? "ON" : "OFF",
            zones[1].occupied ? "ON" : "OFF",
            zones[2].occupied ? "ON" : "OFF");
        blynk_send_str(5, status);

        ESP_LOGI(TAG, "Blynk updated | Temp=%.1f Hum=%.1f | %s",
            g_temperature, g_humidity, status);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Smart Classroom System Booting...");

    gpio_init();
    i2c_init();
    lcd_init();
    wifi_init();

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("Smart Classroom");
    lcd_set_cursor(0, 1);
    lcd_print("System Ready");
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    xTaskCreate(sensor_task,  "sensor_task",  4096, NULL, 5, NULL);
    xTaskCreate(display_task, "display_task", 4096, NULL, 3, NULL);
    xTaskCreate(blynk_task,   "blynk_task",   4096, NULL, 2, NULL);

    ESP_LOGI(TAG, "All tasks running.");
}
