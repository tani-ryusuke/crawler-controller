#ifndef ESP_NOW_HANDLER_H
#define ESP_NOW_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

#include "esp_now.h"

// 送信するデータの構造体
typedef struct {
    int dir;      // テンキー方向 (1-9)
    bool btn[8];  // ボタン8個の状態
} controller_data_t;

// 初期化関数の宣言
void wifi_now_init(void);

// 送信関数の宣言
void send_data_to_main(controller_data_t *data);

#endif