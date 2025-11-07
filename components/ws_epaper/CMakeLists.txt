# 使用 idf_component_register 来注册组件
idf_component_register(
    SRCS
        "DEV_Config.c"
        "font8.c"
        "font12.c"
        "font16.c"
        "font20.c"
        "font24.c"
        "GUI_Paint.c"
        "font12CN.c"
        "font24CN.c"
        "utility/EPD_2in9_V2.c"

    INCLUDE_DIRS
        "."
        "utility"                 # 添加：包含 utility 目录下的头文件
    REQUIRES
        "driver"    # 用于 GPIO 操作
        "freertos"  # 用于 vTaskDelay 和 FreeRTOS 类型
        "esp_lcd"   # 如果你的代码使用了 esp_lcd 库的任何部分，虽然看起来是自定义驱动，但保险起见可以加上
)