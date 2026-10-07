#include "mii/numeric_comparison.hpp"

#include <algorithm>
#include <limits>

namespace mii::auth {
namespace {
bool deadline(std::uint64_t now, std::uint64_t duration, std::uint64_t& value) noexcept {
    if (now > std::numeric_limits<std::uint64_t>::max() - duration) { return false; }
    value = now + duration; return true;
}
}
void NumericComparisonGate::clear() noexcept {
    state_ = PairingState::kIdle; window_deadline_ = 0; prompt_deadline_ = 0;
    shown_at_ = 0; released_at_ = 0; prompt_ = {}; accepted_ = false;
}
void NumericComparisonGate::connected(std::uint64_t generation) noexcept {
    const auto first_connection_window = connection_ == 0 && state_ == PairingState::kIdle
        ? window_deadline_ : 0;
    clear(); connection_ = generation;
    if (generation != 0) { window_deadline_ = first_connection_window; }
}
void NumericComparisonGate::disconnected(std::uint64_t generation) noexcept {
    if (generation == 0 || generation != connection_) { return; }
    clear(); connection_ = 0;
}
void NumericComparisonGate::poll(std::uint64_t now) noexcept {
    if (window_deadline_ != 0 && now >= window_deadline_) { window_deadline_ = 0; }
    if (prompt_deadline_ != 0 && now >= prompt_deadline_ && state_ != PairingState::kDecisionReady) {
        state_ = PairingState::kDecisionReady; accepted_ = false;
    }
}
PairingError NumericComparisonGate::open_local_window(std::uint64_t now) noexcept {
    poll(now);
    if (state_ != PairingState::kIdle) { return PairingError::kWrongState; }
    return deadline(now, kPairingWindowMs, window_deadline_) ? PairingError::kOk : PairingError::kInvalidArgument;
}
PairingError NumericComparisonGate::request(std::uint64_t connection, std::uint32_t code,
    std::uint64_t now, PairingPrompt& out) noexcept {
    out = {}; poll(now);
    if (connection == 0 || connection != connection_) { return PairingError::kStaleConnection; }
    if (state_ != PairingState::kIdle) { return PairingError::kWrongState; }
    if (code > 999999) { return PairingError::kInvalidArgument; }
    if (window_deadline_ == 0) { return PairingError::kWindowClosed; }
    if (next_prompt_ == 0) { return PairingError::kExhausted; }
    std::uint64_t timeout = 0;
    if (!deadline(now, kNumericComparisonTimeoutMs, timeout)) { return PairingError::kInvalidArgument; }
    prompt_deadline_ = std::min(timeout, window_deadline_);
    window_deadline_ = 0; prompt_ = {connection, next_prompt_, code};
    next_prompt_ = next_prompt_ == std::numeric_limits<std::uint64_t>::max() ? 0 : next_prompt_ + 1;
    state_ = PairingState::kAwaitDisplay; out = prompt_; return PairingError::kOk;
}
PairingError NumericComparisonGate::check_prompt(std::uint64_t connection,
    std::uint64_t id, std::uint64_t now) noexcept {
    poll(now);
    if (connection == 0 || connection != connection_) { return PairingError::kStaleConnection; }
    if (id == 0 || id != prompt_.prompt_id) { return PairingError::kStalePrompt; }
    if (prompt_deadline_ != 0 && now >= prompt_deadline_) { return PairingError::kExpired; }
    return PairingError::kOk;
}
PairingError NumericComparisonGate::display_presented(std::uint64_t connection,
    std::uint64_t id, std::uint64_t now, bool released) noexcept {
    const auto guard = check_prompt(connection,id,now);
    if (guard != PairingError::kOk) { return guard; }
    if (state_ != PairingState::kAwaitDisplay) { return PairingError::kWrongState; }
    shown_at_ = now; released_at_ = released ? now : 0;
    state_ = released ? PairingState::kAwaitPress : PairingState::kAwaitRelease;
    return PairingError::kOk;
}
PairingError NumericComparisonGate::physical_button(std::uint64_t connection,
    std::uint64_t id, ButtonEdge edge, std::uint64_t at, std::uint64_t now) noexcept {
    const auto guard = check_prompt(connection,id,now);
    if (guard != PairingError::kOk) { return guard; }
    if (at > now || at < shown_at_) { return PairingError::kInvalidArgument; }
    if (state_ == PairingState::kAwaitRelease && edge == ButtonEdge::kReleased) {
        released_at_ = at; state_ = PairingState::kAwaitPress; return PairingError::kOk;
    }
    if (state_ == PairingState::kAwaitPress && edge == ButtonEdge::kPressed &&
        at > shown_at_ && at > released_at_) {
        accepted_ = true; state_ = PairingState::kDecisionReady; return PairingError::kOk;
    }
    return PairingError::kWrongState;
}
PairingError NumericComparisonGate::cancel_local(std::uint64_t connection,
    std::uint64_t id, std::uint64_t now) noexcept {
    const auto guard = check_prompt(connection,id,now);
    if (guard != PairingError::kOk) { return guard; }
    if (state_ == PairingState::kIdle) { return PairingError::kWrongState; }
    accepted_ = false; state_ = PairingState::kDecisionReady; return PairingError::kOk;
}
bool NumericComparisonGate::take_decision(std::uint64_t connection,
    std::uint64_t now, PairingDecision& out) noexcept {
    out = {}; poll(now);
    if (connection == 0 || connection != connection_ || state_ != PairingState::kDecisionReady) { return false; }
    // Even a once-accepted but unconsumed decision expires at the SMP deadline.
    out = {connection_, prompt_.prompt_id, accepted_ && now < prompt_deadline_};
    clear(); return true;
}
}  // namespace mii::auth
