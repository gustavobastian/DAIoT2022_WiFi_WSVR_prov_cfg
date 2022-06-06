/* Console example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "freertos/event_groups.h"
#include "esp_wifi.h"

#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "cmd_decl.h"
#include <esp_http_server.h>

#if !IP_NAPT
#error "IP_NAPT must be defined"
#endif
#include "lwip/lwip_napt.h"

#include "app_globals.h"

// On board LED
#define BLINK_GPIO 2

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

#define DEFAULT_AP_SSID "DAIoT_ESP32_WS"
#define DEFAULT_AP_IP "192.168.4.1"
#define DEFAULT_DNS "8.8.8.8"

/* Global vars */
uint16_t connect_count = 0;
bool ap_connect = false;
bool has_static_ip = false;

uint32_t my_ip;
uint32_t my_ap_ip;

esp_netif_t* wifiAP;
esp_netif_t* wifiSTA;

httpd_handle_t start_webserver(void);

static const char *TAG = "ESP32 APP";

static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}


void * led_status_thread(void * p)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    while (true)
    {
        gpio_set_level(BLINK_GPIO, ap_connect);

        for (int i = 0; i < connect_count; i++)
        {
            gpio_set_level(BLINK_GPIO, 1 - ap_connect);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            gpio_set_level(BLINK_GPIO, ap_connect);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
  esp_netif_dns_info_t dns;

  switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ap_connect = true;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
        my_ip = event->event_info.got_ip.ip_info.ip.addr;
        if (esp_netif_get_dns_info(wifiSTA, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK) {
            dhcps_dns_setserver((const ip_addr_t *)&dns.ip);
            ESP_LOGI(TAG, "set dns to:" IPSTR, IP2STR(&dns.ip.u_addr.ip4));
        }
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG,"disconnected - retry to connect to the AP");
        ap_connect = false;
        if (!has_static_ip) {

        }
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        connect_count++;
        ESP_LOGI(TAG,"%d. station connected", connect_count);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        connect_count--;
        ESP_LOGI(TAG,"station disconnected - %d remain", connect_count);
        break;
    default:
        break;
  }
  return ESP_OK;
}

const int CONNECTED_BIT = BIT0;
#define JOIN_TIMEOUT_MS (2000)

void wifi_init(const char* ssid, const char* passwd)
{
    ip_addr_t dnsserver;
    //tcpip_adapter_dns_info_t dnsinfo;

    wifi_event_group = xEventGroupCreate();
  
    esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifiAP = esp_netif_create_default_wifi_ap();
    wifiSTA = esp_netif_create_default_wifi_sta();

    my_ap_ip = ipaddr_addr(DEFAULT_AP_IP);

    esp_netif_ip_info_t ipInfo_ap;
    ipInfo_ap.ip.addr = my_ap_ip;
    ipInfo_ap.gw.addr = my_ap_ip;
    IP4_ADDR(&ipInfo_ap.netmask, 255,255,255,0);
    esp_netif_dhcps_stop(wifiAP); // stop before setting ip WifiAP
    esp_netif_set_ip_info(wifiAP, &ipInfo_ap);
    esp_netif_dhcps_start(wifiAP);

    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* ESP WIFI CONFIG */
    wifi_config_t wifi_config = { 0 };
        wifi_config_t ap_config = {
        .ap = {
            .channel = 0,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = 0,
            .max_connection = 8,
            .beacon_interval = 100,
        }
    };

    strlcpy((char*)ap_config.sta.ssid, DEFAULT_AP_SSID, sizeof(DEFAULT_AP_SSID));
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    
    if (strlen(ssid) > 0) {
        strlcpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        strlcpy((char*)wifi_config.sta.password, passwd, sizeof(wifi_config.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA) );
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config) );
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP) );
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config) );        
    }

    // Enable DNS (offer) for dhcp server
    dhcps_offer_t dhcps_dns_value = OFFER_DNS;
    dhcps_set_option_info(6, &dhcps_dns_value, sizeof(dhcps_dns_value));

    // Set custom dns server address for dhcp server
    dnsserver.u_addr.ip4.addr = ipaddr_addr(DEFAULT_DNS);;
    dnsserver.type = IPADDR_TYPE_V4;
    dhcps_dns_setserver(&dnsserver);

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
        pdFALSE, pdTRUE, JOIN_TIMEOUT_MS / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(esp_wifi_start());

    if (strlen(ssid) > 0) {
        ESP_LOGI(TAG, "wifi_init_apsta finished.");
        ESP_LOGI(TAG, "connect to ap SSID: %s ", ssid);
    } else {
        ESP_LOGI(TAG, "wifi_init_ap with default finished.");      
    }
}

char* ssid = NULL;
char* passwd = NULL;
char* static_ip = NULL;
char* subnet_mask = NULL;
char* gateway_addr = NULL;
char* ap_ssid = NULL;
char* ap_passwd = NULL;
char* ap_ip = NULL;

char* param_set_default(const char* def_val) {
    char * retval = malloc(strlen(def_val)+1);
    strcpy(retval, def_val);
    return retval;
}

void app_main(void)
{
    initialize_nvs();

    ap_ssid = DEFAULT_AP_SSID;

    get_config_param_str("ssid", &ssid);
    if (ssid == NULL) {
        ssid = param_set_default("");
    }
    get_config_param_str("passwd", &passwd);
    if (passwd == NULL) {
        passwd = param_set_default("");
    }
 
    // Setup WIFI
    wifi_init(ssid, passwd);

    pthread_t t1;
    pthread_create(&t1, NULL, led_status_thread, NULL);

    char* lock = NULL;
    get_config_param_str("lock", &lock);
    if (lock == NULL) {
        lock = param_set_default("0");
    }
    if (strcmp(lock, "0") ==0) {
        ESP_LOGI(TAG,"Starting config web server");
        start_webserver();
    }
    free(lock);

  
    if (strlen(ssid) == 0) {
         printf("\n"
               "Unconfigured WiFi\n"
               "Configure using 'set_sta' and 'set_ap' and restart.\n");       
    }



    /* Main loop */
    while(true) {

        vTaskDelay(1000 / portTICK_PERIOD_MS);

    }
}
