#pragma once

namespace mii::protocol {

// Exercises the committed protocol v1 vector without heap allocation.
// This is compiled into firmware only when the component Kconfig option is on.
[[nodiscard]] bool run_known_vector_self_test() noexcept;

// Exercises session counters, retry recognition and the replay window.
[[nodiscard]] bool run_session_state_self_test() noexcept;

// Exercises fixed framing, CRC validation and fragmented stream assembly.
[[nodiscard]] bool run_frame_codec_self_test() noexcept;

// Runs every allocation-free protocol startup check enabled in Debug builds.
[[nodiscard]] bool run_startup_self_test() noexcept;

}  // namespace mii::protocol
