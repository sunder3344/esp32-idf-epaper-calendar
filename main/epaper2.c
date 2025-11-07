#include "DEV_Config.h"
#include "EPD.h"
#include "esp_log.h"
#include "GUI_Paint.h"
#include "esp_http_client.h"
#include "epaper.h"
#include "badge.h"
#include "cJSON.h"
#include "nvs.h"
#include <string.h>

UBYTE *BlackImage;
static const char *NVS_UUID = "nvs-uuid";
static TaskHandle_t screen_show_task_handle = NULL;

typedef struct {
	char *buffer;
	int buffer_len;
	int data_len;
} http_response_t;

static const char *WEATHER_IP_URL = "http://restapi.amap.com/v3/ip";
static const char *WEATHER_FORCAST_URL = "http://restapi.amap.com/v3/weather/weatherInfo";

//wifi未配置显示界面
void epaper_init_show() {
	DEV_Module_Init();
	EPD_2IN9_V2_Init();
    //Create a new image cache
    
    UWORD Imagesize = EPD_2IN9_V2_WIDTH * EPD_2IN9_V2_HEIGHT;
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        while(1);
    }
    
    Paint_NewImage(BlackImage, EPD_2IN9_V2_WIDTH, EPD_2IN9_V2_HEIGHT, 270, WHITE);  
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
    
    Paint_DrawString_EN(10, 40, "Please config wifi first", &Font16, WHITE, BLACK);
    Paint_DrawString_EN(10, 56, "http://192.168.4.1:8888", &Font16, WHITE, BLACK);
    EPD_2IN9_V2_Display_Base(BlackImage);
    EPD_2IN9_V2_Sleep();
    free(BlackImage);
}

//wifi配置成功后显示
void epaper_wifi_config_correct() {
	//强制重新复位电子纸
    DEV_Digital_Write(EPD_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(200));   // 拉低200ms
    DEV_Digital_Write(EPD_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    
	DEV_Module_Init();
	EPD_2IN9_V2_Init();
    //Create a new image cache
    
    UWORD Imagesize = EPD_2IN9_V2_WIDTH * EPD_2IN9_V2_HEIGHT;
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        while(1);
    }
    
    Paint_NewImage(BlackImage, EPD_2IN9_V2_WIDTH, EPD_2IN9_V2_HEIGHT, 270, WHITE);  
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
    
    Paint_DrawString_EN(10, 40, "WIFI has connected...", &Font16, WHITE, BLACK);
    EPD_2IN9_V2_Display_Base(BlackImage);
    DEV_Delay_ms(3000);
    EPD_2IN9_V2_Clear();
    EPD_2IN9_V2_Sleep();
    free(BlackImage);
}

esp_err_t http_req_handler(esp_http_client_event_t *evt) {
	http_response_t *resp = (http_response_t *)evt->user_data;
	switch (evt->event_id) {
		case HTTP_EVENT_ON_DATA:
			if (evt->user_data) {
				if (resp->data_len + evt->data_len < resp->buffer_len) {
					memcpy(resp->buffer + resp->data_len, evt->data, evt->data_len);
					resp->data_len += evt->data_len;
				}
			}
			//printf("Http get response: %d, %s\n", evt->data_len, (char *)evt->data);
			break;
		default:
			break;
	}
	return ESP_OK;
}

void http_get_task(void *pvParameters) {
	char *url = (char *)pvParameters;
	http_response_t resp;
	resp.buffer_len = 1024;
	resp.buffer = malloc(resp.buffer_len);
	resp.data_len = 0;
	
	esp_http_client_config_t config = {
		.url = (char *)pvParameters,
		.event_handler = http_req_handler,
		.timeout_ms = 500,  					// 0.5秒超时
		.user_data = &resp,						//把响应缓存传进去
		.skip_cert_common_name_check = true,   // 调试阶段先跳过证书验证
    	.cert_pem = NULL                       // 不验证证书
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_err_t err = esp_http_client_perform(client);
	if (err == ESP_OK) {
		resp.buffer[resp.data_len] = '\0';
		ESP_LOGI(TAG, "Http get status = %d, content_length = %lld", esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));
		
	} else {
		ESP_LOGE(TAG, "Http get request failed: %s", esp_err_to_name(err));
	}
	free(resp.buffer);
	free(url);
	esp_http_client_close(client);
	vTaskDelete(NULL);
}

//同步获取url链接
char *http_get(const char *url) {
	static char buffer[2048];
	http_response_t resp = {
		.buffer = buffer,
		.buffer_len = sizeof(buffer),
		.data_len = 0,
	};
	
	esp_http_client_config_t config = {
		.url = url,
		.event_handler = http_req_handler,
		.user_data = &resp,						//把响应缓存传进去
		.skip_cert_common_name_check = true,   // 调试阶段先跳过证书验证
    	.cert_pem = NULL                       // 不验证证书
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_http_client_set_header(client, "Accept-Charset", "UTF-8");
	esp_err_t err = esp_http_client_perform(client);
	esp_http_client_cleanup(client);
	if (err == ESP_OK) {
		buffer[resp.data_len] = '\0';
		return buffer;
	} else {
		ESP_LOGE(TAG, "Http get request failed: %s", esp_err_to_name(err));
		return NULL;
	}
}

//获取未来天气
Weather_info *weather_get_ip() {
	char *url = (char *)malloc(128);
	const cJSON *code = NULL;
	int i;
	snprintf(url, 128, "%s?key=%s", WEATHER_IP_URL, WEATHER_API_KEY);
	ESP_LOGE(TAG, "url=%s", url);
	//xTaskCreate(&http_get_task, "http_get_task", 8192, (void *)url, 0, NULL);
	char *resp = http_get(url);
	//printf("resp_content=%s\n", resp);
	Weather_info *winfos = (Weather_info *)malloc(sizeof(Weather_info)*4);
	//parse json
	if (resp != NULL) {
		cJSON *json = cJSON_Parse(resp);
		if (json != NULL) {
			code = cJSON_GetObjectItemCaseSensitive(json, "adcode");
			if (cJSON_IsString(code) && code->valuestring != NULL) {		//拿到code
				printf("adcode = %s\n", code->valuestring);
				//继续请求天气预报
				snprintf(url, 128, "%s?key=%s&city=%s", WEATHER_FORCAST_URL, WEATHER_API_KEY, code->valuestring);
				ESP_LOGE(TAG, "current_url=%s", url);
				char *resp3 = http_get(url);
				const cJSON *current_lives = NULL;
				const cJSON *current_item = NULL;
				cJSON *current_json = cJSON_Parse(resp3);
				i = 0;
				if (current_json != NULL) {
					current_lives = cJSON_GetObjectItemCaseSensitive(current_json, "lives");
					if (cJSON_IsArray(current_lives) && cJSON_GetArraySize(current_lives) > 0) {
						cJSON_ArrayForEach(current_item, current_lives) {
							if (i > 0)
								break;
							cJSON *current_weather = cJSON_GetObjectItemCaseSensitive(current_item, "weather");
							cJSON *current_temp = cJSON_GetObjectItemCaseSensitive(current_item, "temperature");
							if (cJSON_IsString(current_temp)) {
								winfos[0].daytemp = atof(current_temp->valuestring);
								winfos[0].nighttemp = atof(current_temp->valuestring);
							}
							strcpy(winfos[0].date, "Blank");
							strcpy(winfos[0].dayweather, current_weather->valuestring);
							strcpy(winfos[0].nightweather, current_weather->valuestring);
							i++;
						}
					}
				}
				printf("%s, %d, %.1f, %.1f, %s\n", winfos[0].date, winfos[0].weeks, winfos[0].daytemp, winfos[0].nighttemp, winfos[0].dayweather);
				
				//请求未来天气
				snprintf(url, 128, "%s?key=%s&city=%s&extensions=all", WEATHER_FORCAST_URL, WEATHER_API_KEY, code->valuestring);
				ESP_LOGE(TAG, "url2=%s", url);
				char *resp2 = http_get(url);
				//printf("weather_content=%s\n", resp2);
				//解析天气预报
				const cJSON *weather_arr = NULL;
				const cJSON *weather_item = NULL;
				cJSON *weather_json = cJSON_Parse(resp2);
				if (weather_json != NULL) {
					i = 0;
					const cJSON *forecast = cJSON_GetObjectItemCaseSensitive(weather_json, "forecasts");
					if (cJSON_IsArray(forecast) && cJSON_GetArraySize(forecast) > 0) {
						cJSON *first_forecast = cJSON_GetArrayItem(forecast, 0);
						weather_arr = cJSON_GetObjectItemCaseSensitive(first_forecast, "casts");
						if (cJSON_IsArray(weather_arr)) {
							cJSON_ArrayForEach(weather_item, weather_arr) {
								if (i == 0) {
									i++;
									continue;
								}	
								cJSON *date = cJSON_GetObjectItemCaseSensitive(weather_item, "date");
								cJSON *weeks = cJSON_GetObjectItemCaseSensitive(weather_item, "week");
								cJSON *dayweather = cJSON_GetObjectItemCaseSensitive(weather_item, "dayweather");
								cJSON *nightweather = cJSON_GetObjectItemCaseSensitive(weather_item, "nightweather");
								cJSON *daytemp = cJSON_GetObjectItemCaseSensitive(weather_item, "daytemp_float");
								cJSON *nighttemp = cJSON_GetObjectItemCaseSensitive(weather_item, "nighttemp_float");
								strcpy(winfos[i].date, date->valuestring);
								strcpy(winfos[i].dayweather, dayweather->valuestring);
								strcpy(winfos[i].nightweather, nightweather->valuestring);
								winfos[i].weeks = 1;
								winfos[i].daytemp = 0.0;
								winfos[i].nighttemp = 0.0;
								
								if (cJSON_IsString(weeks)) {
									winfos[i].weeks = atoi(weeks->valuestring);
								}
								if (cJSON_IsString(daytemp)) {
									winfos[i].daytemp = atof(daytemp->valuestring);
								}
								if (cJSON_IsString(nighttemp)) {
									winfos[i].nighttemp = atof(nighttemp->valuestring);
								}
								printf("%s, %d, %.1f, %.1f\n", winfos[i].date, winfos[i].weeks, winfos[i].daytemp, winfos[i].nighttemp);
								i++;
							}
						}
					}
				}
			}
		}
	}
	free(url);
	return winfos;
}

MsgBody *getMsg() {
	MsgBody *msgBody = (MsgBody *)malloc(sizeof(MsgBody) * 1);
	char url[50] = "http://121.5.90.115/api/user/getmsg";
	const cJSON *status = NULL;
	ESP_LOGE(TAG, "get msg url=%s", url);
	char *resp = http_get(url);
	if (resp != NULL) {
		cJSON *json = cJSON_Parse(resp);
		if (json != NULL) {
			status = cJSON_GetObjectItemCaseSensitive(json, "status");	
			if (cJSON_IsString(status) && status->valuestring != NULL) {		//拿到code
				//如果status = ok，说明拿到了，直接显示
				printf("msg status = %s\n", status->valuestring);
				if (strcmp(status->valuestring, "ok") == 0) {		//拿到就直接赋值
					cJSON *message = cJSON_GetObjectItem(json, "message");
				    if (!message) {
				        memset(msgBody, 0, sizeof(MsgBody));
				        return msgBody;
				    }
					const cJSON *uuid = cJSON_GetObjectItemCaseSensitive(message, "id");
					const cJSON *msg = cJSON_GetObjectItemCaseSensitive(message, "content");
					if (cJSON_IsString(uuid)) {
						strcpy(msgBody->uuid, uuid->valuestring);
					}
					if (cJSON_IsString(msg)) {
						strcpy(msgBody->msg, msg->valuestring);
					}
				} else {		//如果其他情况，就是没有，直接置空
					memset(msgBody, 0, sizeof(MsgBody));
				}
			} else {
				memset(msgBody, 0, sizeof(MsgBody));
			}
		}	
	}
	printf("%s = %s\n", msgBody->uuid, msgBody->msg);
	return msgBody;
}

void get_msg_task(void *pvParameters) {
    for (;;) {
        MsgBody *msgBody = getMsg();   //同步阻塞无所谓，这里是独立线程
        if (msgBody) {
            if (xSemaphoreTake(g_shared_msg.mutex, pdMS_TO_TICKS(100))) {
                g_shared_msg.msg = *msgBody;
                xSemaphoreGive(g_shared_msg.mutex);
            }
            free(msgBody);
        }
        vTaskDelay(pdMS_TO_TICKS(30000));   // 每30秒请求一次
    }
}

void delete_and_create_task() {
    if (screen_show_task_handle != NULL) {
        // 删除已有任务
        vTaskDelete(screen_show_task_handle);
        screen_show_task_handle = NULL;  // 清空任务句柄
        printf("Old screen_show task deleted.\n");
    }

    // 创建新任务
    xTaskCreate(&screen_show, "screen_show_task", 40960, NULL, 5, &screen_show_task_handle);
    printf("New screen_show task created.\n");
}

// 时间局刷任务
void epaper_time_refresh_task(void *pvParameters) {
	Paint_SelectImage(BlackImage);
    PAINT_TIME sPaint_time;
    time_t now = 0;
    struct tm timeinfo = {0};
    char msg[20];		//消息只能有20个字符
    nvs_init();

    for (;;) {
        time(&now);
        localtime_r(&now, &timeinfo);

        sPaint_time.Hour = timeinfo.tm_hour;
        sPaint_time.Min  = timeinfo.tm_min;
        sPaint_time.Sec  = timeinfo.tm_sec;
        //printf("%d %d %d\n", sPaint_time.Hour, sPaint_time.Min, sPaint_time.Sec);

		//每5分钟，需要全局刷新一下，否则微雪的墨水屏容易损坏
		if (timeinfo.tm_min % 2 == 0 && timeinfo.tm_sec == 1) {
			printf("time to break...\n");
			//break;
			//直接刷屏一次
		}
        //只刷新时间区域
        Paint_ClearWindows(20, 10, 20 + Font20.Width * 8, 10 + Font20.Height, WHITE);
        Paint_DrawTime(20, 10, &sPaint_time, &Font20, WHITE, BLACK);
        
        //消息局刷
        //现在esp32的flash中存储上一条消息的ID，然后比较，如果ID不一样，就局刷提醒，否则不变
        if (xSemaphoreTake(g_shared_msg.mutex, pdMS_TO_TICKS(10))) {
            snprintf(msg, sizeof(msg), "%s", g_shared_msg.msg.msg[0] ? g_shared_msg.msg.msg : "");
            xSemaphoreGive(g_shared_msg.mutex);
        }
        //printf("id = %s, len=%d\n", g_shared_msg.msg.uuid, strlen(g_shared_msg.msg.uuid));
        if (strlen(g_shared_msg.msg.uuid) > 0) {		//如果uuid存在再去刷新
			//比对下esp32里面存储的uuid是否是之前的，如果是，也不刷新，否则就要局刷一下
			char stored_uuid[37];
			size_t len = sizeof(stored_uuid);
			esp_err_t err = nvs_read(NVS_UUID, stored_uuid, &len);
			if (err == ESP_OK) {
				if (strcmp(stored_uuid, g_shared_msg.msg.uuid) != 0) {		//如果uuid没变过，就不刷新了
					Paint_ClearWindows(10, 65, 10 + Font12.Width * 18, 65 + Font12.Height, WHITE);
        			Paint_DrawString_EN(10, 65, msg, &Font12, WHITE, BLACK);
        			nvs_write(NVS_UUID, g_shared_msg.msg.uuid);
				}
			} else {
				Paint_ClearWindows(10, 65, 10 + Font12.Width * 18, 65 + Font12.Height, WHITE);
		        Paint_DrawString_EN(10, 65, msg, &Font12, WHITE, BLACK);
		        nvs_write(NVS_UUID, g_shared_msg.msg.uuid);
			}
		}

        EPD_2IN9_V2_Display_Partial(BlackImage);

        //DEV_Delay_ms(1000);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    printf("now start a new task\n");
    //xTaskCreate(&screen_show, "screen_show_task", 40960, NULL, 0, NULL);
    vTaskDelete(NULL);
}

void screen_sleep() {
	EPD_2IN9_V2_Sleep();
}

void screen_show_task() {
	
}

//draw the screen
void screen_show(void *pvParameters) {
	time_t now = 0;
	struct tm timeinfo = {0};
	int i, j;
  	int x_pos = 150;
  	
  	g_shared_msg.mutex = xSemaphoreCreateMutex();
	memset(&g_shared_msg.msg, 0, sizeof(MsgBody));
	
	//拿天气预报信息
	Weather_info *winfos = weather_get_ip();
	
	DEV_Module_Init();
	EPD_2IN9_V2_Init();
	//Create a new image cache
    UWORD Imagesize = EPD_2IN9_V2_WIDTH * EPD_2IN9_V2_HEIGHT;
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        while(1);
    }
    Paint_NewImage(BlackImage, EPD_2IN9_V2_WIDTH, EPD_2IN9_V2_HEIGHT, 270, WHITE);
    Paint_Clear(WHITE);
    Paint_SelectImage(BlackImage);
    
    time(&now);
	localtime_r(&now, &timeinfo);
	
    char day_section[20];
    //sprintf(time_section, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    sprintf(day_section, "%02d-%02d %s", timeinfo.tm_mon + 1, timeinfo.tm_mday, WEEK[timeinfo.tm_wday]);
    Paint_DrawString_EN(36, 40, day_section, &Font12, WHITE, BLACK);
    
    //单色的图片不能随着屏幕旋转而旋转，且会有一个黑影，这里只能用Paint_DrawImage_Rotated方法来显示
    //Paint_DrawString_EN(10, 70, "By continuing to browser", &Font12, WHITE, BLACK);
    for (j = 0; j < sizeof(WEATHER_DESC) / sizeof(WEATHER_DESC[0]); j++) {
        if (strcmp(WEATHER_DESC[j], winfos[0].dayweather) == 0) {
			if (timeinfo.tm_hour >= 5 && timeinfo.tm_hour <= 18) {
            	Paint_DrawImage_Rotated(WEATHER_PIC[j], 155, 6, 40, 40, 0);
            } else {
				Paint_DrawImage_Rotated(WEATHER_NIGHT_PIC[j], 155, 6, 40, 40, 0);
			}
            break;
        }
    }
    Paint_DrawLine(140, 10, 140, 118, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(10, 60, 130, 60, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(148, 45, 290, 45, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    
    //根据当前时间的hour判断是白天还是晚上
    char nowTemp[10];
    if (timeinfo.tm_hour >= 5 && timeinfo.tm_hour <= 18) {		//早上5点到晚上18点就是白天
		sprintf(nowTemp, "%.1fC", winfos[0].daytemp);
	} else {
		sprintf(nowTemp, "%.1fC", winfos[0].nighttemp);
	}
    Paint_DrawString_EN(195, 15, nowTemp, &Font24, WHITE, BLACK);
    
    for (i = 1; i <= 3; i++) {
		char week_desc[4];
		char temp_range[6];
		printf("day_weather:=%s, %d\n", winfos[i].dayweather, strcmp("阴", winfos[i].dayweather));
		sprintf(week_desc, "%s", WEEK[winfos[i].weeks]);
		sprintf(temp_range, "%.0f-%.0f", winfos[i].nighttemp, winfos[i].daytemp);
		for (j = 0; j < sizeof(WEATHER_DESC) / sizeof(WEATHER_DESC[0]); j++) {
	        if (strcmp(WEATHER_DESC[j], winfos[i].dayweather) == 0) {
				printf("$j=%d\n", j);
				if (timeinfo.tm_hour >= 5 && timeinfo.tm_hour <= 18) {
	            	Paint_DrawImage_Rotated(WEATHER_PIC[j], x_pos, 50, 40, 40, 0);
	            } else {
					Paint_DrawImage_Rotated(WEATHER_NIGHT_PIC[j], x_pos, 50, 40, 40, 0);
				}
	            break;
	        }
	    }
	    Paint_DrawString_EN(x_pos + 10, 90, week_desc, &Font12, WHITE, BLACK);
	    Paint_DrawString_EN(x_pos + 5, 105, temp_range, &Font12, WHITE, BLACK);
	    x_pos += 50;
    }
    //4灰度不支持局刷，所以这里不能用EPD_2IN9_V2_4GrayDisplay_Base
    EPD_2IN9_V2_Display_Base(BlackImage);
    free(winfos);
    //时间局刷(放到最后执行)
    xTaskCreate(&epaper_time_refresh_task, "epaper_time_refresh", 8192, NULL, 1, NULL);
    xTaskCreate(&get_msg_task, "get_msg_task", 8192, NULL, 1, NULL);
    vTaskDelete(NULL);
}