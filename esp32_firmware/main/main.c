#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "mqtt_client.h"
#include "cJSON.h"

// Wi-Fi and MQTT Configuration
#define WIFI_SSID       "YOUR_WIFI_NAME"     // Replace with network SSID
#define WIFI_PASS       "YOUR_WIFI_PASSWORD" // Replace with network password

#define MQTT_BROKER_URI "mqtt://broker.hivemq.com:1883"
#define MQTT_TOPIC      "gesture/actuate"

// Pin Assignments
#define GPIO_LED        GPIO_NUM_2
#define GPIO_RELAY      GPIO_NUM_19
#define GPIO_SERVO      GPIO_NUM_18

#define SERVO_LEDC_TIMER    LEDC_TIMER_0
#define SERVO_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_CHANNEL  LEDC_CHANNEL_0

static const char *TAG = "FIRMGEN_ESP32";

static void init_servo(void) {
    ledc_timer_config_t timer_conf = {
        .speed_mode       = SERVO_LEDC_MODE,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .timer_num        = SERVO_LEDC_TIMER,
        .freq_hz          = 50,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t channel_conf = {
        .gpio_num       = GPIO_SERVO,
        .speed_mode     = SERVO_LEDC_MODE,
        .channel        = SERVO_LEDC_CHANNEL,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = SERVO_LEDC_TIMER,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_conf));
}

static void set_servo_angle(uint32_t angle) {
    uint32_t duty = (uint32_t)(163.84 + (angle * 655.36 / 180.0));
    ESP_ERROR_CHECK(ledc_set_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL));
    ESP_LOGI(TAG, "[ACTUATOR] Servo position set to %" PRIu32 " degrees", angle);
}

static void init_gpios(void) {
    ESP_ERROR_CHECK(gpio_reset_pin(GPIO_LED));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT));

    ESP_ERROR_CHECK(gpio_reset_pin(GPIO_RELAY));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_RELAY, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_RELAY, 0));
}

static void execute_actuation(const char *gesture) {
    ESP_LOGI(TAG, "Command Received: %s", gesture);

    if (strcmp(gesture, "OPEN_PALM") == 0) {
        gpio_set_level(GPIO_RELAY, 1);
        gpio_set_level(GPIO_LED, 1);
        ESP_LOGI(TAG, "[ACTUATOR_SUCCESS] OPEN_PALM -> Relay HIGH / LED ON");
    } else if (strcmp(gesture, "FIST") == 0) {
        gpio_set_level(GPIO_RELAY, 0);
        gpio_set_level(GPIO_LED, 0);
        ESP_LOGI(TAG, "[ACTUATOR_SUCCESS] FIST -> Relay LOW / LED OFF");
    } else if (strcmp(gesture, "POINT_LEFT") == 0) {
        set_servo_angle(0);
        ESP_LOGI(TAG, "[ACTUATOR_SUCCESS] POINT_LEFT -> Servo 0 deg");
    } else if (strcmp(gesture, "POINT_RIGHT") == 0) {
        set_servo_angle(180);
        ESP_LOGI(TAG, "[ACTUATOR_SUCCESS] POINT_RIGHT -> Servo 180 deg");
    } else {
        ESP_LOGW(TAG, "Ignoring unsupported gesture: %s", gesture);
    }
}

static void parse_mqtt(const char *data, int len) {
    char buf[256];
    if (data == NULL || len < 0 || len >= sizeof(buf)) {
        ESP_LOGW(TAG, "Ignoring invalid or oversized MQTT message (%d bytes)", len);
        return;
    }
    memcpy(buf, data, len);
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        ESP_LOGW(TAG, "Ignoring malformed MQTT JSON");
        return;
    }

    cJSON *g = cJSON_GetObjectItem(root, "gesture");
    if (cJSON_IsString(g) && g->valuestring) {
        execute_actuation(g->valuestring);
    } else {
        ESP_LOGW(TAG, "Ignoring MQTT JSON without a string 'gesture' field");
    }
    cJSON_Delete(root);
}

static void mqtt_handler(void *args, esp_event_base_t base, int32_t id, void *data) {
    esp_mqtt_event_handle_t event = data;
    if (id == MQTT_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "MQTT Broker Connected Successfully.");
        esp_mqtt_client_subscribe(event->client, MQTT_TOPIC, 0);
    } else if (id == MQTT_EVENT_DATA) {
        parse_mqtt(event->data, event->data_len);
    }
}

static void wifi_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Wi-Fi Connected Successfully.");
    }
}

void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    init_gpios();
    init_servo();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (esp_netif_create_default_wifi_sta() == NULL) {
        ESP_LOGE(TAG, "Failed to create default Wi-Fi station interface");
        abort();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_mqtt_client_config_t mqtt_cfg = { .broker.address.uri = MQTT_BROKER_URI };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        abort();
    }
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        client, ESP_EVENT_ANY_ID, mqtt_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));
}