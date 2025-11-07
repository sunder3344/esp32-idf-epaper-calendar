#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_flash.h"
#include "esp_event.h"
#include "freertos/projdefs.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_netif_net_stack.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_task_wdt.h"
#include "epaper.h"

#define EXAMPLE_ESP_WIFI_AP_SSID      CONFIG_ESP_WIFI_AP_SSID
#define EXAMPLE_ESP_WIFI_AP_PASSWD    CONFIG_ESP_WIFI_AP_PASSWORD
#define EXAMPLE_ESP_WIFI_AP_CHANNEL   CONFIG_ESP_WIFI_AP_CHANNEL
#define EXAMPLE_MAX_STA_CONN_AP       CONFIG_ESP_MAX_STA_CONN_AP

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define HTTP_SERVER_PORT   8888

// NTP服务器配置
#define NTP_SERVER1 "ntp1.aliyun.com" // 阿里云NTP服务器
#define NTP_SERVER2 "ntp.ntsc.ac.cn"   // 国家授时中心NTP服务器

static const char *TAG_AP = "wifi softAP";
static const char *TAG_STA = "WiFi Sta";
static const char *NVS_TAG = "e-paper-nvs";
static const char *NTP_TAG = "ntp_sync";

static int s_retry_num = 3;
SharedMsg g_shared_msg = {0};
int global_restart = 0;
static httpd_handle_t global_server_handle = NULL;

/* FreeRTOS event group to signal when we are connected/disconnected */
//static EventGroupHandle_t s_wifi_event_group;

/*esp_err_t http_req_handler(esp_http_client_event_t *evt);
void http_get_task(void *pvParameters);*/

void time_sync_notification_cb(struct timeval *tv) {
	ESP_LOGI(NTP_TAG, "time sync");
	setenv("TZ", "CST-8", 1);
	tzset();
}

static void initialize_sntp(void) {

	ESP_LOGI(NTP_TAG, "init sntp");
	esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
	esp_sntp_setservername(0, NTP_SERVER1);
	esp_sntp_setservername(1, NTP_SERVER2);
	esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
	esp_sntp_init();
}

//设置看门狗超时时间
void set_twdt_timeout(int milisec) {
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = milisec,
        .trigger_panic = true, //触发致命错误（panic）
    };
    
    // 重新配置 TWDT
    if (esp_task_wdt_init(&twdt_config) == ESP_OK) {
        ESP_LOGI("WDT", "Task WDT timeout set to %d seconds", milisec);
    } else {
        ESP_LOGE("WDT", "Failed to reconfigure Task WDT");
    }
}

//sync time here
static void sync_time(void) {
	initialize_sntp();
	int retry = 0;
	const int retry_count = 10;
	time_t now = 0;
	struct tm timeinfo = {0};
	
	while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
		ESP_LOGI(NTP_TAG, "waiting time sync... (%d/%d)", retry, retry_count);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
	
	time(&now);
	localtime_r(&now, &timeinfo);
	ESP_LOGE(NTP_TAG, "year:=%d", timeinfo.tm_year);
	ESP_LOGE(NTP_TAG, "%d", esp_sntp_get_sync_status());
	if (timeinfo.tm_year < (2020 - 1900)) {
		ESP_LOGI(NTP_TAG, "time sync error!");
	} else {
		ESP_LOGI(NTP_TAG, "now: %s", asctime(&timeinfo));
		ESP_LOGI(NTP_TAG, "time: %d:%d:%d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
	}
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station "MACSTR" joined, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station "MACSTR" left, AID=%d, reason:%d", MAC2STR(event->mac), event->aid, event->reason);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_ERROR_CHECK(esp_wifi_connect());
        ESP_LOGI(TAG_STA, "Station started");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		// 临时禁用当前任务的看门狗
        //esp_task_wdt_delete(NULL);
		//wifi has connect,show in epaper
		vTaskDelay(pdMS_TO_TICKS(1000));
		epaper_wifi_config_correct();
		// 恢复看门狗监控
        //esp_task_wdt_add(NULL);
		
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG_STA, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 3;
        // Adding a small delay to allow other tasks to run
        vTaskDelay(pdMS_TO_TICKS(10));
        //start time sync here
        sync_time();
        //weather_get_ip();
        //screen_show();
        xTaskCreate(&screen_show, "screen_show_task", 40960, NULL, 0, NULL);
        //start init epaper here
    }
}

/*esp_err_t http_req_handler(esp_http_client_event_t *evt) {
	switch (evt->event_id) {
		case HTTP_EVENT_ON_DATA:
			printf("Http get response: %d, %s\n", evt->data_len, (char *)evt->data);
			break;
		default:
			break;
	}
	return ESP_OK;
}

void http_get_task(void *pvParameters) {
	esp_http_client_config_t config = {
		.url = (char *)pvParameters,
		.event_handler = http_req_handler
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_err_t err = esp_http_client_perform(client);
	if (err == ESP_OK) {
		ESP_LOGI(TAG, "Http get status = %d, content_length = %lld", esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));
		
	} else {
		ESP_LOGE(TAG, "Http get request failed: %s", esp_err_to_name(err));
	}
	esp_http_client_close(client);
	vTaskDelete(NULL);
}*/

/* Initialize soft AP */
esp_netif_t *wifi_init_softap(void) {
    esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_AP_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_AP_SSID),
            .channel = EXAMPLE_ESP_WIFI_AP_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_AP_PASSWD,
            .max_connection = EXAMPLE_MAX_STA_CONN_AP,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    
    if (strlen(EXAMPLE_ESP_WIFI_AP_PASSWD) == 0) {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));

    ESP_LOGI(TAG_AP, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_AP_SSID, EXAMPLE_ESP_WIFI_AP_PASSWD, EXAMPLE_ESP_WIFI_AP_CHANNEL);

    return esp_netif_ap;
}

/* Initialize wifi station */
esp_netif_t *wifi_init_sta(char *ssid, char *passwd) {
    esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_sta_config = {
        .sta = {
            .ssid = "",
            .password = "",
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .failure_retry_cnt = s_retry_num,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    strncpy((char *)wifi_sta_config.sta.ssid, ssid, strlen(ssid));
    strncpy((char *)wifi_sta_config.sta.password, passwd, strlen(passwd));
    
    ESP_LOGI(TAG_AP, "%s", "start------------------");
    ESP_LOGI(TAG_AP, "%s", ssid);
    ESP_LOGI(TAG_AP, "%d", strlen(ssid));
    ESP_LOGI(TAG_AP, "%s", passwd);
    ESP_LOGI(TAG_AP, "%d", strlen(passwd));

	//ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));

    ESP_LOGI(TAG_STA, "wifi_init_sta finished.");

    return esp_netif_sta;
}

esp_err_t index_get_handler(httpd_req_t *req) {
	const char resp[] = "<form method=\"POST\" action=\"/config\">"
                        "SSID:<input name=\"ssid\"><br>"
                        "Password:<input name=\"password\" type=\"password\"><br>"
                        "<input type=\"submit\" value=\"config\">"
                        "</form>";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t uri_index = {
	.uri = "/",
	.method = HTTP_GET,
	.handler = index_get_handler,
	.user_ctx = NULL
};

// stop server manually
void stop_webserver() {
	if (global_server_handle) {
		ESP_LOGI(TAG_STA, "stopping web server");
		httpd_stop(global_server_handle);
		global_server_handle = NULL;
	}
}

esp_err_t config_post_handler(httpd_req_t *req) {
	char buf[128];
	int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf) - 1));
	if (ret <= 0)
		return ESP_FAIL;
	buf[ret] = '\0';
	
	//parse post data
	char ssid[64] = {0}, passwd[64] = {0};
	sscanf(buf, "ssid=%[^&]&password=%s", ssid, passwd);
	
	//save to nvs
	nvs_handle_t nvs_handler;
	nvs_open(NVS_TAG, NVS_READWRITE, &nvs_handler);
	nvs_set_str(nvs_handler, "ssid", ssid);
	nvs_set_str(nvs_handler, "password", passwd);
	nvs_commit(nvs_handler);
	nvs_close(nvs_handler);
	
	httpd_resp_send(req, "config successful, the device will restart!!!", HTTPD_RESP_USE_STRLEN);
	screen_sleep();
	//vTaskDelay(2000 / portTICK_PERIOD_MS);
	vTaskDelay(pdMS_TO_TICKS(100));
	esp_restart();
	return ESP_OK;
}

static const httpd_uri_t uri_config = {
	.uri = "/config",
	.method = HTTP_ANY,
	.handler = config_post_handler,
	.user_ctx = NULL
};

/*local http server to enter ssid and pwd*/
void start_webserver() {
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	//httpd_handle_t server = NULL;
	
	config.server_port = HTTP_SERVER_PORT;
	config.lru_purge_enable = true;
	
	if (httpd_start(&global_server_handle, &config) == ESP_OK) {
		//register URI handlers
		httpd_register_uri_handler(global_server_handle, &uri_index);
		httpd_register_uri_handler(global_server_handle, &uri_config);
	}
	
	ESP_LOGI(TAG, "start web server...");
}

void app_main(void) {
	nvs_handle_t nvs_handle;
	char ssid[64], password[64];
	size_t len;
	esp_netif_t *esp_netif_ap = NULL;
	esp_netif_t *esp_netif_sta = NULL;
	
	ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
	
	//init NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	
	/* Register Event handler */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID,
                    &wifi_event_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP,
                    &wifi_event_handler,
                    NULL,
                    NULL));
	
	ESP_LOGI(TAG, "ESP_WIFI_MODE_AP start");
    
    /*Initialize WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    //read from nvs whether exist the ssid and password
    nvs_open(NVS_TAG, NVS_READONLY, &nvs_handle);
    if (nvs_get_str(nvs_handle, "ssid", NULL, &len) == ESP_OK) {
		nvs_get_str(nvs_handle, "ssid", ssid, &len);
		nvs_get_str(nvs_handle, "password", NULL, &len);
		nvs_get_str(nvs_handle, "password", password, &len);
		nvs_close(nvs_handle);
		
		/* Initialize STA */
	    ESP_LOGI(TAG_STA, "WIFI_MODE_APSTA");
	    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	    
	    /* Initialize AP */
	    esp_netif_ap = wifi_init_softap();
	    
	    esp_netif_sta = wifi_init_sta(ssid, password);
	    //start wifi
	 	ESP_ERROR_CHECK(esp_wifi_start());
	 	
	 	//start web server
	    start_webserver();
	 	
	 	/* Set sta as the default interface */
    	esp_netif_set_default_netif(esp_netif_sta);
	} else {
		//show epaper init words
		epaper_init_show();
	
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

	    /* Initialize AP */
	    ESP_LOGI(TAG_AP, "ESP_WIFI_MODE_AP");
	    esp_netif_ap = wifi_init_softap();
	
	    /* Start WiFi */
	    ESP_ERROR_CHECK(esp_wifi_start());
	    //start web server
	    start_webserver();
	}
	
    /*EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        //ESP_LOGI(TAG_STA, "connected to ap SSID:%s password:%s", EXAMPLE_ESP_WIFI_STA_SSID, EXAMPLE_ESP_WIFI_STA_PASSWD);
    } else if (bits & WIFI_FAIL_BIT) {
        //ESP_LOGI(TAG_STA, "Failed to connect to SSID:%s, password:%s", EXAMPLE_ESP_WIFI_STA_SSID, EXAMPLE_ESP_WIFI_STA_PASSWD);
    } else {
        ESP_LOGE(TAG_STA, "UNEXPECTED EVENT");
        return;
    }*/
    
    /* Enable napt on the AP netif */
    if (esp_netif_napt_enable(esp_netif_ap) != ESP_OK) {
        ESP_LOGE(TAG_STA, "NAPT not enabled on the netif: %p", esp_netif_ap);
    }
}
