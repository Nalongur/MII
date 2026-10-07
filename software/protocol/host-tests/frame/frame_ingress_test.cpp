#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "mii/frame_ingress.hpp"

namespace {

using mii::protocol::FrameIngress;
using mii::protocol::IngressError;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FRAME_INGRESS_TEST_FAIL: " << message << '\n';
    std::exit(1);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

struct Capture {
    std::array<std::array<std::uint8_t, mii::protocol::kMaxFramePayloadSize>, 4>
        payloads{};
    std::array<std::size_t, 4> sizes{};
    std::size_t count{0};
    bool reject{false};
};

bool capture_handler(
    const std::uint8_t* payload,
    std::size_t payload_size,
    void* context) noexcept {
    auto* capture = static_cast<Capture*>(context);
    if (capture == nullptr || capture->reject ||
        capture->count >= capture->payloads.size() ||
        payload_size > capture->payloads[0].size()) {
        return false;
    }
    std::memcpy(capture->payloads[capture->count].data(), payload, payload_size);
    capture->sizes[capture->count] = payload_size;
    ++capture->count;
    return true;
}

std::vector<std::uint8_t> make_frame(const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> frame(
        mii::protocol::kFrameHeaderSize + payload.size());
    const auto result = mii::protocol::encode_frame(
        payload.data(), payload.size(), frame.data(), frame.size());
    require(result.ok() && result.bytes_written == frame.size(), "frame encode failed");
    return frame;
}

void expect_capture(
    const Capture& capture,
    std::size_t index,
    const std::vector<std::uint8_t>& expected,
    const std::string& context) {
    require(index < capture.count, context + ": missing callback");
    require(capture.sizes[index] == expected.size(), context + ": wrong size");
    require(
        std::memcmp(
            capture.payloads[index].data(), expected.data(), expected.size()) == 0,
        context + ": wrong payload");
}

}  // namespace

int main() {
    const std::vector<std::uint8_t> payload{0x08, 0x01, 0x4d, 0x49, 0xff};
    const std::vector<std::uint8_t> frame = make_frame(payload);

    FrameIngress guards;
    Capture capture;
    require(
        guards.consume(nullptr, 1, 0, capture_handler, &capture).error ==
            IngressError::kInvalidArgument,
        "null input accepted");
    require(
        guards.consume(frame.data(), frame.size(), 0, nullptr, &capture).error ==
            IngressError::kInvalidArgument,
        "null handler accepted");

    FrameIngress whole;
    auto result = whole.consume(
        frame.data(), frame.size(), 10, capture_handler, &capture);
    require(result.ok() && result.bytes_consumed == frame.size() &&
                result.frames_delivered == 1 && result.frame_errors == 0,
            "whole frame delivery failed");
    expect_capture(capture, 0, payload, "whole frame");
    require(whole.counters().frames_delivered == 1, "delivery counter wrong");

    FrameIngress consecutive;
    Capture consecutive_capture;
    std::vector<std::uint8_t> doubled = frame;
    doubled.insert(doubled.end(), frame.begin(), frame.end());
    result = consecutive.consume(
        doubled.data(), doubled.size(), 20, capture_handler, &consecutive_capture);
    require(result.ok() && result.bytes_consumed == doubled.size() &&
                result.frames_delivered == 2,
            "consecutive frame delivery failed");
    expect_capture(consecutive_capture, 0, payload, "consecutive first");
    expect_capture(consecutive_capture, 1, payload, "consecutive second");

    FrameIngress fragmented;
    Capture fragmented_capture;
    const std::size_t split = 7;
    result = fragmented.consume(
        frame.data(), split, 100, capture_handler, &fragmented_capture);
    require(result.ok() && fragmented.has_partial_frame(), "partial frame missing");
    require(!fragmented.poll_timeout(2099), "partial frame expired too early");
    result = fragmented.consume(
        frame.data() + split,
        frame.size() - split,
        2099,
        capture_handler,
        &fragmented_capture);
    require(result.frames_delivered == 1 && !fragmented.has_partial_frame(),
            "fragment completion failed");

    FrameIngress timeout;
    Capture timeout_capture;
    static_cast<void>(timeout.consume(
        frame.data(), split, 5000, capture_handler, &timeout_capture));
    require(!timeout.poll_timeout(6999), "timeout fired before 2 seconds");
    require(timeout.poll_timeout(7000), "timeout did not fire at 2 seconds");
    require(timeout.counters().partial_timeouts == 1 &&
                !timeout.has_partial_frame(),
            "timeout state or counter wrong");
    result = timeout.consume(
        frame.data(), frame.size(), 7001, capture_handler, &timeout_capture);
    require(result.frames_delivered == 1, "decoder did not recover after timeout");

    FrameIngress stale_on_consume;
    Capture stale_capture;
    static_cast<void>(stale_on_consume.consume(
        frame.data(), split, 1000, capture_handler, &stale_capture));
    result = stale_on_consume.consume(
        frame.data() + split,
        frame.size() - split,
        3000,
        capture_handler,
        &stale_capture);
    require(result.frames_delivered == 0 &&
                stale_on_consume.counters().partial_timeouts == 1,
            "late input did not expire stale partial frame first");
    result = stale_on_consume.consume(
        frame.data(), frame.size(), 3001, capture_handler, &stale_capture);
    require(result.frames_delivered == 1,
            "decoder did not recover after consume-time timeout");

    FrameIngress refreshed;
    Capture refreshed_capture;
    static_cast<void>(refreshed.consume(
        frame.data(), 3, 0, capture_handler, &refreshed_capture));
    static_cast<void>(refreshed.consume(
        frame.data() + 3, 2, 1999, capture_handler, &refreshed_capture));
    require(!refreshed.poll_timeout(3998), "progress did not refresh timeout");
    require(refreshed.poll_timeout(3999), "refreshed timeout did not fire");

    FrameIngress wrapped_clock;
    Capture wrapped_capture;
    const std::uint32_t near_wrap =
        std::numeric_limits<std::uint32_t>::max() - 999U;
    static_cast<void>(wrapped_clock.consume(
        frame.data(), split, near_wrap, capture_handler, &wrapped_capture));
    require(!wrapped_clock.poll_timeout(999), "wrapped clock expired early");
    require(wrapped_clock.poll_timeout(1000), "wrapped clock timeout failed");

    FrameIngress recover_header;
    Capture recovered_header_capture;
    std::vector<std::uint8_t> bad_header(frame.begin(), frame.begin() + 12);
    bad_header[4] ^= 0x01U;
    bad_header.insert(bad_header.end(), frame.begin(), frame.end());
    result = recover_header.consume(
        bad_header.data(),
        bad_header.size(),
        50,
        capture_handler,
        &recovered_header_capture);
    require(result.ok() && result.frame_errors == 1 &&
                result.frames_delivered == 1,
            "header-error recovery failed");
    require(recover_header.counters().framing_errors == 1 &&
                recover_header.counters().header_errors == 1,
            "header-error counters wrong");

    FrameIngress recover_payload;
    Capture recovered_payload_capture;
    std::vector<std::uint8_t> bad_payload = frame;
    bad_payload.back() ^= 0x80U;
    bad_payload.insert(bad_payload.end(), frame.begin(), frame.end());
    result = recover_payload.consume(
        bad_payload.data(),
        bad_payload.size(),
        60,
        capture_handler,
        &recovered_payload_capture);
    require(result.frame_errors == 1 && result.frames_delivered == 1 &&
                recover_payload.counters().payload_crc_errors == 1,
            "payload-error recovery or counter failed");

    FrameIngress noisy;
    Capture noisy_capture;
    std::vector<std::uint8_t> noise{0x00, 0x01, 0x02};
    noise.insert(noise.end(), frame.begin(), frame.end());
    result = noisy.consume(
        noise.data(), noise.size(), 70, capture_handler, &noisy_capture);
    require(result.frames_delivered == 1 && noisy.counters().discarded_bytes == 3,
            "noise accounting failed");

    FrameIngress rejected;
    Capture rejecting_capture;
    rejecting_capture.reject = true;
    result = rejected.consume(
        doubled.data(), doubled.size(), 80, capture_handler, &rejecting_capture);
    require(result.error == IngressError::kHandlerRejected &&
                result.bytes_consumed == frame.size() &&
                rejected.counters().handler_rejections == 1,
            "handler rejection boundary failed");
    rejecting_capture.reject = false;
    result = rejected.consume(
        doubled.data() + result.bytes_consumed,
        doubled.size() - result.bytes_consumed,
        81,
        capture_handler,
        &rejecting_capture);
    require(result.frames_delivered == 1, "post-rejection recovery failed");

    static_cast<void>(rejected.consume(
        frame.data(), split, 90, capture_handler, &rejecting_capture));
    rejected.reset_session();
    require(!rejected.has_partial_frame() &&
                rejected.counters().handler_rejections == 1,
            "session reset did not preserve counters");
    rejected.clear_counters();
    require(rejected.counters().frames_delivered == 0 &&
                rejected.counters().handler_rejections == 0,
            "counter clear failed");

    std::cout << "FRAME_INGRESS_TEST_PASS: delivery, timeout, wraparound, "
                 "recovery, rejection, reset, and counters\n";
    return 0;
}
