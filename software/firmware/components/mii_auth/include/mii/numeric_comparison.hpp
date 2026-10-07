#pragma once

#include <cstdint>

namespace mii::auth {
inline constexpr std::uint64_t kPairingWindowMs = 60000;
inline constexpr std::uint64_t kNumericComparisonTimeoutMs = 30000;
enum class PairingState : std::uint8_t { kIdle, kAwaitDisplay, kAwaitRelease, kAwaitPress, kDecisionReady };
enum class PairingError : std::uint8_t {
    kOk, kInvalidArgument, kStaleConnection, kStalePrompt, kWrongState,
    kWindowClosed, kExpired, kExhausted,
};
enum class ButtonEdge : std::uint8_t { kPressed, kReleased };
struct PairingPrompt {
    std::uint64_t connection_generation{0};
    std::uint64_t prompt_id{0};
    std::uint32_t code{0}; // Display exactly six decimal digits, including leading zeroes.
};
struct PairingDecision {
    std::uint64_t connection_generation{0};
    std::uint64_t prompt_id{0};
    bool accept{false};
};
// Single trusted task; firmware display/button adapters supply local events.
// No protocol or automatic "yes" route. SMP itself remains NimBLE's job.
class NumericComparisonGate {
public:
    void connected(std::uint64_t generation) noexcept;
    void disconnected(std::uint64_t generation) noexcept;
    [[nodiscard]] PairingError open_local_window(std::uint64_t now_ms) noexcept;
    [[nodiscard]] PairingError request(std::uint64_t connection_generation,
        std::uint32_t nimble_code, std::uint64_t now_ms, PairingPrompt&) noexcept;
    [[nodiscard]] PairingError display_presented(std::uint64_t connection_generation,
        std::uint64_t prompt_id, std::uint64_t now_ms, bool debounced_button_released) noexcept;
    [[nodiscard]] PairingError physical_button(std::uint64_t connection_generation,
        std::uint64_t prompt_id, ButtonEdge, std::uint64_t event_at_ms,
        std::uint64_t now_ms) noexcept;
    [[nodiscard]] PairingError cancel_local(std::uint64_t connection_generation,
        std::uint64_t prompt_id, std::uint64_t now_ms) noexcept;
    [[nodiscard]] bool take_decision(std::uint64_t connection_generation,
        std::uint64_t now_ms, PairingDecision&) noexcept;
    void poll(std::uint64_t now_ms) noexcept;
    [[nodiscard]] PairingState state() const noexcept { return state_; }
private:
    [[nodiscard]] PairingError check_prompt(std::uint64_t, std::uint64_t, std::uint64_t) noexcept;
    void clear() noexcept;
    PairingState state_{PairingState::kIdle};
    std::uint64_t connection_{0};
    std::uint64_t next_prompt_{1};
    std::uint64_t window_deadline_{0};
    std::uint64_t prompt_deadline_{0};
    std::uint64_t shown_at_{0};
    std::uint64_t released_at_{0};
    PairingPrompt prompt_{};
    bool accepted_{false};
};
}  // namespace mii::auth
