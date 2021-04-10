

#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
//#include <esp_event_loop.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include "esp_attr.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/queue.h"

#include <driver/gpio.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"

#include "semphr.h"

#include <esp_event.h>
#include <freertos/event_groups.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_softap.h>

#define ESP_INTR_FLAG_DEFAULT 0
#define relay_gpio  27
#define button_gpio 13
#define reset_button_gpio 0

#define mainBUTTON_PRIORITY		( configMAX_PRIORITIES - 1 )
#define butDEBOUNCE_DELAY	( 200 / portTICK_RATE_MS )
static xSemaphoreHandle xButtonSemaphore;
void vButtonTask( void *pvParameters );

uint32_t button_last_event_time = 0;
uint16_t button_debounce_time = 300;
xQueueHandle button_event_queue = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);
homekit_characteristic_t switch_on = HOMEKIT_CHARACTERISTIC_(ON, true, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback)
);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//WIFI PROVISION
void on_wifi_ready();
static const char *TAG = "app";
const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;
static void event_handler(void* arg, esp_event_base_t event_base, int event_id, void* event_data)
{
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(TAG, "Received Wi-Fi credentials"
                         "\n\tSSID     : %s\n\tPassword : %s",
                         (const char *) wifi_sta_cfg->ssid,
                         (const char *) wifi_sta_cfg->password);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
                         "\n\tPlease reset to factory and retry provisioning",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                         "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                break;
            case WIFI_PROV_END:
                /* De-initialize manager once provisioning is finished */
                wifi_prov_mgr_deinit();
                break;
            default:
                break;
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
        /* Signal main application to continue execution */
        on_wifi_ready();
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
        esp_wifi_connect();
    }
}
static void wifi_init_sta(void)
{
    /* Start Wi-Fi in station mode */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}
static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}
esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen, uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    if (inbuf) {
        ESP_LOGI(TAG, "Received data: %.*s", inlen, (char *)inbuf);
    }
    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    if (*outbuf == NULL) {
        ESP_LOGE(TAG, "System out of memory");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1; /* +1 for NULL terminating byte */

    return ESP_OK;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void relay_write(bool on) {
    gpio_set_level(relay_gpio, on ? 1 : 0);
}
void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write(switch_on.value.bool_value);
}
void switch_identify_task(void *_args) {
        for (int j=0; j<2; j++) {
            relay_write(true);
            vTaskDelay(300 / portTICK_PERIOD_MS);
            relay_write(false);
            vTaskDelay(300 / portTICK_PERIOD_MS);
        }

    relay_write(false);
    vTaskDelete(NULL);
}
void switch_identify(homekit_value_t _value) {
    xTaskCreate(switch_identify_task, "Switch identify", 2048, NULL, 2, NULL);
}


void vButtonTask( void *pvParameters )
{
	vSemaphoreCreateBinary( xButtonSemaphore );
	for( ;; )
	{
		xSemaphoreTake( xButtonSemaphore, portMAX_DELAY );
    	switch_on.value.bool_value = !switch_on.value.bool_value;
        relay_write(switch_on.value.bool_value);
        homekit_characteristic_notify(&switch_on, switch_on.value);
		vTaskDelay( butDEBOUNCE_DELAY );
		xSemaphoreTake( xButtonSemaphore, 0 );
	}
}


void button_pressed_isr(void)
{
    short sHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR( xButtonSemaphore, &sHigherPriorityTaskWoken );
	portYIELD_FROM_ISR( sHigherPriorityTaskWoken );
}


void IRAM_ATTR button_event_ISR(void *arg)
{
    uint32_t button_number = (uint32_t)arg;
    uint32_t now = xTaskGetTickCountFromISR();
    
    if ((now - button_last_event_time) * portTICK_PERIOD_MS < button_debounce_time)
    {
        return;
    }
    else
    {
        xQueueSendFromISR(button_event_queue, &button_number, NULL);
    }
    button_last_event_time = xTaskGetTickCountFromISR();
}
void button_event_task(void *arg)
{
    uint32_t button_name;
    for (;;)
    {
        if (xQueueReceive(button_event_queue, &button_name, portMAX_DELAY))
        {

            switch (button_name)
            {
            case reset_button_gpio:
                printf("Performing reset... \n");

                
                vTaskDelay(4000 / portTICK_PERIOD_MS);
                bool stillPressed =  gpio_get_level(reset_button_gpio);
                if (stillPressed == 0) {
                    printf("Reseting... \n");
                    ESP_ERROR_CHECK(nvs_flash_erase());
                    bool provisioned = false;
                    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
                    printf("Reseted, rebooting... \n");
                    homekit_server_reset();
                    esp_restart();
                } else {
                    printf("Reset aborted \n");
                }
                
                break;
            default:
                 printf("unknown button event\n");
            }
        }
    }
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void user_gpio_init(void){
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL<<relay_gpio);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << button_gpio);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_PIN_INTR_NEGEDGE;
    io_conf.pin_bit_mask = (1ULL << reset_button_gpio);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    
    
    
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(button_gpio, button_pressed_isr,(void *)button_gpio);
    gpio_isr_handler_add(reset_button_gpio, button_event_ISR, (void *)reset_button_gpio);
   
     
    button_event_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(button_event_task, "button_event_task", 2048, NULL, 100, NULL);

    xTaskCreate(vButtonTask, "Button", configMINIMAL_STACK_SIZE, NULL, mainBUTTON_PRIORITY, NULL );
  

    gpio_set_direction(relay_gpio, GPIO_MODE_OUTPUT);
    relay_write(switch_on.value.bool_value);
    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Switch"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Konifer4Przemki"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "037A2BABF004"),
            HOMEKIT_CHARACTERISTIC(MODEL, "Smart Switch"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "3.0"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, switch_identify),
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Smart Switch"),
            &switch_on,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "605-22-744"
};

void on_wifi_ready() {
   homekit_server_init(&config);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_event_group = xEventGroupCreate();
    

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };

    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    user_gpio_init();


    /* If device is not yet provisioned start provisioning service */
    if (!provisioned) {
        ESP_LOGI(TAG, "Starting provisioning");
        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
        const char *pop = "60522744";
        const char *service_key = NULL;
        wifi_prov_mgr_endpoint_create("custom-data");
        /* Start provisioning service */
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, pop, service_name, service_key));
        wifi_prov_mgr_endpoint_register("custom-data", custom_prov_data_handler, NULL);
    } else {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");
        wifi_prov_mgr_deinit();
        wifi_init_sta();
        on_wifi_ready();
        
    }
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, false, true, portMAX_DELAY);
}
