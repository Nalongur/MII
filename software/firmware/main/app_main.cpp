#include "esp_err.h"
#include "esp_log.h"
#include "mii/ble_control.hpp"
#include "mii/board_v7.hpp"
#include "mii/protocol_self_test.hpp"
#include "sdkconfig.h"

namespace {
constexpr char kTag[] = "mii_boot";
}

extern "C" void app_main(void) {
    // This must remain the first application action: keep STM6601 power held
    // before logging, service construction, or peripheral initialisation.
    ESP_ERROR_CHECK(mii::board_v7::assert_power_hold());

    ESP_ERROR_CHECK(mii::board_v7::initialize_safe_outputs());

#if CONFIG_MII_PROTOCOL_STARTUP_SELF_TEST
    ESP_ERROR_CHECK(
        mii::protocol::run_startup_self_test() ? ESP_OK : ESP_FAIL);
    ESP_LOGI(kTag, "protocol v1 codec, session and frame self-tests passed");
#endif

#if CONFIG_MII_BLE_CONTROL_SERVICE
    ESP_ERROR_CHECK(mii::ble::start_control_service());
    ESP_LOGW(kTag, "experimental BLE transport started; command execution is quarantined");
#else
    ESP_LOGI(kTag, "MII V7 firmware baseline started; peripherals remain disabled");
#endif
}
