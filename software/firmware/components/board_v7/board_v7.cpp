#include "mii/board_v7.hpp"

#include <cstdint>

namespace mii::board_v7 {
namespace {

esp_err_t configure_output(gpio_num_t pin_number, int initial_level) {
    // Load the output latch before enabling the driver to minimise glitches,
    // then reassert the level after configuration.
    esp_err_t result = gpio_set_level(pin_number, initial_level);
    if (result != ESP_OK) {
        return result;
    }

    gpio_config_t config{};
    config.pin_bit_mask = std::uint64_t{1} << static_cast<unsigned>(pin_number);
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;

    result = gpio_config(&config);
    if (result != ESP_OK) {
        return result;
    }

    return gpio_set_level(pin_number, initial_level);
}

}  // namespace

esp_err_t assert_power_hold() {
    return configure_output(pin::kPowerHold, level::kPowerHoldAsserted);
}

esp_err_t initialize_safe_outputs() {
    esp_err_t result = configure_output(pin::kGnssEnable, level::kGnssDisabled);
    if (result != ESP_OK) {
        return result;
    }

    result = configure_output(pin::kStatusLed, level::kStatusLedOff);
    if (result != ESP_OK) {
        return result;
    }

    return configure_output(pin::kEpdChipSelect, level::kEpdChipSelectInactive);
}

}  // namespace mii::board_v7
