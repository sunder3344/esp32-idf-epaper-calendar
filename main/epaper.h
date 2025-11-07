#ifndef _EPAPER_H_
#define _EPAPER_H_

#include "esp_http_client.h"
#include "badge.h"

#define WEATHER_API_KEY CONFIG_WEATHER_API_KEY

static const char *TAG = "e-paper";
static char *WEEK[] __attribute__((unused)) = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
static const char *WEATHER_DESC[] __attribute__((unused)) = {
	"晴",
	"少云",
	"晴间多云",
	"多云",
	"阴",
	"有风",
	"平静",
	"微风",
	"和风",
	"清风",
	"强风/劲风",
	"疾风",
	"大风",
	"烈风",
	"风暴",
	"狂爆风",
	"飓风",
	"热带风暴",
	"霾",
	"中度霾",
	"重度霾",
	"严重霾",
	"阵雨",
	"雷阵雨",
	"雷阵雨并伴有冰雹",
	"小雨",
	"中雨",
	"大雨",
	"暴雨",
	"大暴雨",
	"特大暴雨",
	"强阵雨",
	"强雷阵雨",
	"极端降雨",
	"毛毛雨/细雨",
	"雨",
	"小雨-中雨",
	"中雨-大雨",
	"大雨-暴雨",
	"暴雨-大暴雨",
	"大暴雨-特大暴雨",
	"雨雪天气",
	"雨夹雪",
	"阵雨夹雪",
	"冻雨",
	"雪",
	"阵雪",
	"小雪",
	"中雪",
	"大雪",
	"暴雪",
	"小雪-中雪",
	"中雪-大雪",
	"大雪-暴雪",
	"浮尘",
	"扬沙",
	"沙尘暴",
	"强沙尘暴",
	"龙卷风",
	"雾",
	"浓雾",
	"强浓雾",
	"轻雾",
	"大雾",
	"特强浓雾",
	"热",
	"冷",
	"未知"
};

static const uint8_t *WEATHER_PIC[] __attribute__((unused)) = {
	gImage_sunny,
	gImage_sunny2,
	gImage_cloudy_sunny,
	gImage_cloudy,
	gImage_cloudy2,
	gImage_wind,
	gImage_sunny2,
	gImage_wind,
	gImage_wind,
	gImage_wind,
	gImage_gale,
	gImage_gale,
	gImage_gale,
	gImage_gale2,
	gImage_gale2,
	gImage_gale3,
	gImage_gale3,
	gImage_gale3,
	gImage_mist,
	gImage_mist_mid,
	gImage_mist_mid,
	gImage_mist_heavy,
	gImage_rain_sunny,
	gImage_thunder_rain,
	gImage_thunder_rain2,
	gImage_gentle_rain,
	gImage_mid_rain,
	gImage_heavy_rain,
	gImage_heavy_rain,
	gImage_thunder_storm,
	gImage_thunder_storm,
	gImage_thunder_storm,
	gImage_thunder_storm,
	gImage_thunder_storm,
	gImage_gentle_rain,
	gImage_rain,
	gImage_mid_rain,
	gImage_heavy_rain,
	gImage_heavy_rain,
	gImage_thunder_storm,
	gImage_thunder_storm,
	gImage_rain,
	gImage_rain,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_mist,
	gImage_mist,
	gImage_gale2,
	gImage_gale2,
	gImage_gale3,
	gImage_mist,
	gImage_mist,
	gImage_mist_heavy,
	gImage_mist,
	gImage_mist_mid,
	gImage_mist_heavy,
	gImage_sunny,
	gImage_froze,
	gImage_unknow
};

static const uint8_t *WEATHER_NIGHT_PIC[] __attribute__((unused)) = {
	gImage_sunny_night,
	gImage_sunny2,
	gImage_cloudy_sunny2,
	gImage_cloudy_night,
	gImage_cloudy2,
	gImage_wind,
	gImage_sunny_night,
	gImage_wind,
	gImage_wind,
	gImage_wind,
	gImage_gale,
	gImage_gale,
	gImage_gale,
	gImage_gale2,
	gImage_gale2,
	gImage_gale3,
	gImage_gale3,
	gImage_gale3,
	gImage_mist,
	gImage_mist_mid,
	gImage_mist_mid,
	gImage_mist_heavy,
	gImage_rainy_night,
	gImage_thunder_rain,
	gImage_thunder_rain2,
	gImage_gentle_rain,
	gImage_mid_rain,
	gImage_heavy_rain,
	gImage_heavy_rain,
	gImage_thunder_storm,
	gImage_thunder_storm,
	gImage_thunder_storm,
	gImage_thunder_storm,
	gImage_thunder_storm,
	gImage_gentle_rain,
	gImage_rain,
	gImage_mid_rain,
	gImage_heavy_rain,
	gImage_heavy_rain,
	gImage_thunder_storm,
	gImage_thunder_storm,
	gImage_rain,
	gImage_rain,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_snow,
	gImage_mist,
	gImage_mist,
	gImage_gale2,
	gImage_gale2,
	gImage_gale3,
	gImage_mist,
	gImage_mist,
	gImage_mist_heavy,
	gImage_mist,
	gImage_mist_mid,
	gImage_mist_heavy,
	gImage_sunny,
	gImage_froze,
	gImage_unknow
};

typedef struct {
	char ad_code[10];
	char province[10];
	char city[10];
} WEATHER_POS;

typedef struct {
	char date[11];
	int weeks;
	char dayweather[10];
	char nightweather[10];
	float daytemp;
	float nighttemp;
} Weather_info;

typedef struct {
	char uuid[37];
	char msg[18];		//18个字符
} MsgBody;

typedef struct {
    MsgBody msg;
    SemaphoreHandle_t mutex;   // 保护数据
} SharedMsg;

extern SharedMsg g_shared_msg;

esp_err_t http_req_handler(esp_http_client_event_t *evt);
void http_get_task(void *pvParameters);
void epaper_init_show();
void epaper_wifi_config_correct();
Weather_info * weather_get_ip();
void screen_show(void *pvParameters);
void screen_sleep();

#endif
/* FILE END */