#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

namespace mii::board_v7 {

// MII V7 is fitted with ESP32-S3-MINI-1-N8: 8 MB flash, no PSRAM.
inline constexpr unsigned kFlashSizeBytes = 8U * 1024U * 1024U;
inline constexpr bool kHasPsram = false;

// This is the only firmware header that may own raw V7 GPIO numbers. Drivers
// and application code consume these names instead of duplicating integers.
namespace pin {
inline constexpr gpio_num_t kFunctionKeyBoot = GPIO_NUM_0;
inline constexpr gpio_num_t kFunctionKey2 = GPIO_NUM_1;
inline constexpr gpio_num_t kPowerButtonOut = GPIO_NUM_2;
inline constexpr gpio_num_t kBatteryAdc = GPIO_NUM_4;
inline constexpr gpio_num_t kBmeSda = GPIO_NUM_5;
inline constexpr gpio_num_t kBmeScl = GPIO_NUM_6;
inline constexpr gpio_num_t kEpdBusy = GPIO_NUM_7;
inline constexpr gpio_num_t kEpdReset = GPIO_NUM_8;
inline constexpr gpio_num_t kEpdDataCommand = GPIO_NUM_9;
inline constexpr gpio_num_t kEpdChipSelect = GPIO_NUM_10;
inline constexpr gpio_num_t kEpdMosi = GPIO_NUM_11;
inline constexpr gpio_num_t kEpdClock = GPIO_NUM_12;
inline constexpr gpio_num_t kChargeSense = GPIO_NUM_13;
inline constexpr gpio_num_t kPowerControllerInterrupt = GPIO_NUM_14;
inline constexpr gpio_num_t kGnssEnable = GPIO_NUM_15;
inline constexpr gpio_num_t kGnssPulsePerSecond = GPIO_NUM_16;
inline constexpr gpio_num_t kGnssUartTx = GPIO_NUM_17;
inline constexpr gpio_num_t kGnssUartRx = GPIO_NUM_18;
inline constexpr gpio_num_t kUsbDataMinus = GPIO_NUM_19;
inline constexpr gpio_num_t kUsbDataPlus = GPIO_NUM_20;
inline constexpr gpio_num_t kPowerHold = GPIO_NUM_21;
inline constexpr gpio_num_t kDebugUartTx = GPIO_NUM_43;
inline constexpr gpio_num_t kDebugUartRx = GPIO_NUM_44;
inline constexpr gpio_num_t kPowerGood = GPIO_NUM_47;
inline constexpr gpio_num_t kStatusLed = GPIO_NUM_48;
}  // namespace pin

namespace level {
inline constexpr int kPowerHoldAsserted = 1;
inline constexpr int kPowerHoldReleased = 0;
inline constexpr int kGnssDisabled = 0;
inline constexpr int kGnssEnabled = 1;
inline constexpr int kStatusLedOff = 1;
inline constexpr int kStatusLedOn = 0;
inline constexpr int kEpdChipSelectInactive = 1;
}  // namespace level

// Assert PSHOLD as the first statement in app_main. It is intentionally
// separate from the remaining defaults so no other GPIO work can precede it.
[[nodiscard]] esp_err_t assert_power_hold();

// Configure only confirmed, conservative output defaults. Inputs and buses are
// left in their reset state until the owning driver is brought up.
[[nodiscard]] esp_err_t initialize_safe_outputs();

}  // namespace mii::board_v7
