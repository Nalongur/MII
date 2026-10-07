#include <cstdlib>
#include <iostream>
#include <limits>

#include "mii/numeric_comparison.hpp"

using namespace mii::auth;
unsigned checks = 0;
void require(bool ok, const char* message) {
    ++checks;
    if (!ok) { std::cerr << "NUMERIC_COMPARISON_TEST_FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
    NumericComparisonGate gate; PairingPrompt prompt{}; PairingDecision decision{};
    require(gate.request(1,123456,1,prompt) == PairingError::kStaleConnection, "request without connection");
    require(gate.open_local_window(1) == PairingError::kOk, "physical window before connection");
    gate.connected(1);
    require(gate.request(1,1000000,2,prompt) == PairingError::kInvalidArgument, "numeric range");
    require(gate.request(1,0,2,prompt) == PairingError::kOk && prompt.code == 0, "zero valid six-digit code");
    const auto first_id = prompt.prompt_id;
    require(gate.request(1,123456,3,prompt) == PairingError::kWrongState, "one pending SMP request");
    require(gate.open_local_window(3) == PairingError::kWrongState, "cannot replace active window");
    require(gate.physical_button(1,first_id,ButtonEdge::kPressed,3,3) == PairingError::kWrongState, "press before display ignored");
    require(!gate.take_decision(1,3,decision), "no automatic accept");
    require(gate.display_presented(2,first_id,4,true) == PairingError::kStaleConnection, "stale connection display");
    require(gate.display_presented(1,first_id+1,4,true) == PairingError::kStalePrompt, "stale display epoch");
    require(gate.display_presented(1,first_id,4,false) == PairingError::kOk, "preheld confirm button");
    require(gate.physical_button(1,first_id,ButtonEdge::kPressed,5,5) == PairingError::kWrongState, "held button cannot approve");
    require(gate.physical_button(1,first_id,ButtonEdge::kReleased,3,6) == PairingError::kInvalidArgument, "old queued edge cannot release");
    require(gate.physical_button(1,first_id,ButtonEdge::kReleased,6,6) == PairingError::kOk, "fresh release");
    require(gate.physical_button(1,first_id,ButtonEdge::kPressed,7,6) == PairingError::kInvalidArgument, "future timestamp rejected");
    require(gate.physical_button(1,first_id,ButtonEdge::kPressed,6,6) == PairingError::kWrongState, "same-timestamp release/press cannot approve");
    require(gate.physical_button(1,first_id,ButtonEdge::kPressed,7,7) == PairingError::kOk, "fresh physical press");
    require(!gate.take_decision(2,8,decision), "stale host cannot consume decision");
    require(gate.take_decision(1,8,decision) && decision.accept && decision.prompt_id == first_id, "exact pending decision delivered");
    require(!gate.take_decision(1,8,decision), "decision delivered only once");
    require(gate.request(1,123456,9,prompt) == PairingError::kWindowClosed, "window consumed by first request");
    require(gate.display_presented(1,first_id,9,true) == PairingError::kStalePrompt, "old UI cannot revive consumed request");

    require(gate.open_local_window(10) == PairingError::kOk, "new physical window");
    require(gate.request(1,999999,11,prompt) == PairingError::kOk && prompt.prompt_id > first_id, "new epoch maximum code");
    auto current = prompt.prompt_id;
    require(gate.display_presented(1,current,12,true) == PairingError::kOk, "released snapshot after display");
    require(gate.physical_button(1,current,ButtonEdge::kPressed,11,13) == PairingError::kInvalidArgument, "pre-prompt queued press rejected");
    require(gate.cancel_local(1,current,13) == PairingError::kOk, "local rejection");
    require(gate.take_decision(1,14,decision) && !decision.accept, "reject delivered");
    require(gate.open_local_window(20) == PairingError::kOk, "display timeout window");
    require(gate.request(1,123456,21,prompt) == PairingError::kOk, "display never finishes");
    current = prompt.prompt_id;
    require(gate.display_presented(1,current,30021,true) == PairingError::kExpired, "deadline rejects late display");
    require(gate.take_decision(1,30021,decision) && !decision.accept, "timeout produces explicit reject");

    require(gate.open_local_window(40000) == PairingError::kOk, "disconnect window");
    require(gate.request(1,123456,40001,prompt) == PairingError::kOk, "disconnect prompt");
    current = prompt.prompt_id;
    gate.disconnected(99); require(gate.state() == PairingState::kAwaitDisplay, "old disconnect ignored");
    gate.disconnected(1); gate.connected(2);
    require(gate.display_presented(2,current,40002,true) == PairingError::kStalePrompt, "disconnect wipes display epoch");
    require(!gate.take_decision(2,40002,decision), "no old connection decision");
    require(gate.request(2,123456,40002,prompt) == PairingError::kWindowClosed, "disconnect wipes pairing window");

    require(gate.open_local_window(50000) == PairingError::kOk, "expired accept window");
    require(gate.request(2,123456,50001,prompt) == PairingError::kOk, "accept timeout prompt");
    current = prompt.prompt_id;
    require(gate.display_presented(2,current,50002,true) == PairingError::kOk, "accept timeout display");
    require(gate.physical_button(2,current,ButtonEdge::kPressed,50003,50003) == PairingError::kOk, "accept recorded");
    require(gate.take_decision(2,80001,decision) && !decision.accept, "delayed accepted decision expires before host injection");
    require(gate.open_local_window(90000) == PairingError::kOk, "window expiry");
    require(gate.request(2,123456,150000,prompt) == PairingError::kWindowClosed, "pairing window expires at boundary");
    require(gate.open_local_window(160000) == PairingError::kOk, "short remaining physical window");
    require(gate.request(2,123456,219999,prompt) == PairingError::kOk, "request near physical deadline");
    require(gate.display_presented(2,prompt.prompt_id,220000,true) == PairingError::kExpired,
            "comparison cannot extend original physical window");
    require(gate.take_decision(2,220000,decision) && !decision.accept, "short remaining window rejects");
    require(gate.open_local_window(230000) == PairingError::kOk, "cancel accepted pending decision window");
    require(gate.request(2,123456,230001,prompt) == PairingError::kOk, "cancel accepted pending request");
    current = prompt.prompt_id;
    require(gate.display_presented(2,current,230002,true) == PairingError::kOk, "cancel accepted display");
    require(gate.physical_button(2,current,ButtonEdge::kPressed,230003,230003) == PairingError::kOk,
            "accept awaits trusted host consumption");
    require(gate.cancel_local(2,current,230004) == PairingError::kOk, "local cancel wins before host consumption");
    require(gate.take_decision(2,230005,decision) && !decision.accept, "cancelled acceptance rejected");
    require(gate.open_local_window(std::numeric_limits<std::uint64_t>::max()) == PairingError::kInvalidArgument,
            "deadline cannot overflow");
    std::cout << "NUMERIC_COMPARISON_TEST_PASS: " << checks << " assertions; local window, display, fresh edges, epochs, one-shot decision, timeout; gate "
              << sizeof(NumericComparisonGate) << " bytes (host ABI)\n";
}
