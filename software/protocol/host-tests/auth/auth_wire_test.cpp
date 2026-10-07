#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "esp_bt.h"
#include "mii/auth_wire.hpp"
#include "mii/idf_auth_crypto.hpp"

namespace {
using namespace mii::auth;
using mii::protocol::InboundDecision;
unsigned checks = 0, nonce_count = 0;
void require(bool ok, const char* description) {
    ++checks;
    if (!ok) { std::cerr << "AUTH_WIRE_TEST_FAIL: " << description << '\n'; std::exit(1); }
}
std::string field(const std::string& json, const char* name) {
    const auto key = json.find('"' + std::string(name) + '"');
    require(key != std::string::npos, "fixture field");
    const auto start = json.find('"', json.find(':', key)) + 1;
    return json.substr(start, json.find('"', start) - start);
}
std::vector<std::uint8_t> unhex(const std::string& hex) {
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return bytes;
}
template<std::size_t N> std::array<std::uint8_t, N> bytes(unsigned start) {
    std::array<std::uint8_t, N> value{};
    for (std::size_t i = 0; i < N; ++i) { value[i] = static_cast<std::uint8_t>(start + i); }
    return value;
}
bool deterministic_nonce(void*, std::uint8_t* out, std::size_t size) noexcept {
    for (std::size_t i = 0; i < size; ++i) { out[i] = static_cast<std::uint8_t>(193 + i + nonce_count); }
    ++nonce_count; return true;
}
bool commit(void*, const OwnerRecord&) noexcept { return true; }
std::vector<std::uint8_t> encode(const mii_protocol_v1_AuthenticationPacket& packet) {
    std::array<std::uint8_t, 604> out{};
    const auto result = encode_authentication_packet(packet, out.data(), out.size());
    require(result.ok(), "packet encode");
    return {out.begin(), out.begin() + static_cast<std::ptrdiff_t>(result.size)};
}
void set_command(mii_protocol_v1_AuthenticationPacket& packet, CryptoPort crypto,
                 const Secret& key, const std::uint8_t* inner, std::size_t size) {
    require(size <= 512, "test inner bound");
    auto& p = packet.body.authenticated_envelope;
    p.canonical_envelope.size = static_cast<pb_size_t>(size);
    std::memcpy(p.canonical_envelope.bytes, inner, size);
    std::array<std::uint8_t, 630> input{};
    std::memcpy(input.data(), "MIICTL1C", 8);
    std::memcpy(input.data() + 8, p.session_id.bytes, 16);
    input[24] = static_cast<std::uint8_t>(size); input[25] = static_cast<std::uint8_t>(size >> 8);
    std::memcpy(input.data() + 26, inner, size);
    Secret tag{};
    require(crypto.hmac(crypto.context, key, input.data(), 26 + size, tag), "test command MAC");
    p.authentication_tag.size = 32;
    std::memcpy(p.authentication_tag.bytes, tag.data(), 32);
}
}  // namespace
esp_bt_controller_status_t esp_bt_controller_get_status() { return ESP_BT_CONTROLLER_STATUS_IDLE; }
void esp_fill_random(void*, std::size_t) { std::abort(); } // Never used: only deterministic test nonce injection.

int main(int argc, char** argv) {
    require(argc == 2, "fixture path");
    std::ifstream file(argv[1]); require(file.good(), "fixture input");
    const std::string json((std::istreambuf_iterator<char>(file)), {});
    std::vector<std::uint8_t> command_wire;
    mii_protocol_v1_AuthenticationPacket command_packet{};
    for (const char* name : {"binding_proposal", "binding_accepted", "session_hello", "session_challenge",
                            "session_proof", "session_ready", "authenticated", "failure"}) {
        const auto wire = unhex(field(json, ("packet_" + std::string(name) + "_hex").c_str()));
        mii_protocol_v1_AuthenticationPacket packet{};
        require(decode_authentication_packet(wire.data(), wire.size(), packet) == WireError::kOk, "golden packet decode");
        require(encode(packet) == wire, "Python/nanopb exact packet bytes");
        for (std::size_t size = 0; size < wire.size(); ++size) {
            mii_protocol_v1_AuthenticationPacket truncated{};
            require(decode_authentication_packet(wire.data(), size, truncated) != WireError::kOk, "all packet truncations rejected");
        }
        auto unknown = wire; unknown.insert(unknown.end(), {0xf8, 0x07, 0x01});
        require(decode_authentication_packet(unknown.data(), unknown.size(), packet) == WireError::kNonCanonical,
                "outer unknown field rejected");
        require(packet.which_body == 0, "rejected security packet wiped");
        auto duplicate = wire; duplicate.insert(duplicate.end(), {0x08, 0x01});
        require(decode_authentication_packet(duplicate.data(), duplicate.size(), packet) == WireError::kNonCanonical,
                "duplicate security field rejected");
        if (std::strcmp(name, "authenticated") == 0) {
            command_wire = wire;
            require(decode_authentication_packet(wire.data(), wire.size(), command_packet) == WireError::kOk, "command fixture");
        }
    }
    auto invalid = command_packet; invalid.authentication_version = 2;
    std::array<std::uint8_t, 604> output{};
    require(encode_authentication_packet(invalid, output.data(), output.size()).error == WireError::kInvalidFields, "wrong auth version");
    invalid = command_packet; invalid.request_id = 0;
    require(encode_authentication_packet(invalid, output.data(), output.size()).error == WireError::kInvalidFields, "zero correlation ID");
    invalid = command_packet; invalid.body.authenticated_envelope.authentication_tag.size = 31;
    require(encode_authentication_packet(invalid, output.data(), output.size()).error == WireError::kInvalidFields, "short MAC");
    require(encode_authentication_packet(command_packet, output.data(), 1).error == WireError::kOutputTooSmall, "output bound");
    invalid = command_packet; invalid.body.authenticated_envelope.canonical_envelope.size = 512;
    std::fill_n(invalid.body.authenticated_envelope.canonical_envelope.bytes, 512, 0x55);
    require(encode(invalid).size() <= 604, "largest wrapped message fits frame bound");

    IdfAuthCrypto crypto; require(crypto.initialize() == ESP_OK, "actual mbedTLS setup");
    auto port = crypto.port(); port.random = deterministic_nonce;
    const Id device = bytes<16>(1);
    const OwnerRecord record{1, UINT64_C(0x0102030405060708), true,
        {bytes<16>(17), bytes<32>(33), bytes<32>(65), bytes<32>(97)}};
    OwnerSession session(device, port, {nullptr, commit});
    require(session.restore(StorageLoad::kOk, record) == Error::kOk, "restore fixture owner");
    session.connected(77, {true,true,true,16});
    mii::protocol::DecodedDeviceStatusRequest decoded{};
    const auto device_name = field(json, "device_id_hex");
    auto result = AuthWireGateway::authorize_status(session, 77, command_wire.data(), command_wire.size(), 1, decoded);
    require(result.wire == WireError::kUnauthorized && result.authorization.error == Error::kWrongState, "unauthenticated wire cannot execute");
    Challenge challenge{};
    require(session.begin_session(77, record.credential.owner_id, bytes<32>(161), 2, challenge) == Error::kOk, "begin fixture handshake");
    Secret proof{}; const auto proof_wire = unhex(field(json, "client_proof_hex"));
    std::copy(proof_wire.begin(), proof_wire.end(), proof.begin());
    require(session.finish_session(77, proof, 3) == Error::kOk, "finish exact fixture proof");
    result = AuthWireGateway::authorize_status(session, 77, command_wire.data(), command_wire.size(), 4, decoded);
    require(result.wire == WireError::kOk && result.authorization.replay == InboundDecision::kAcceptNew
        && decoded.message_id == 1 && decoded.sequence_number == 1 && decoded.include_runtime_counters,
        "MAC verified then same bytes decoded");
    result = AuthWireGateway::authorize_status(session, 77, command_wire.data(), command_wire.size(), 4, decoded);
    require(result.wire == WireError::kOk && result.authorization.replay == InboundDecision::kDuplicateMessage, "wire duplicate not processed");
    Secret session_key{}; const auto key = unhex(field(json, "session_key_hex")); std::copy(key.begin(), key.end(), session_key.begin());
    auto numbered_command = [&](std::uint64_t number, const char* name) {
        std::array<std::uint8_t, 512> inner{};
        const auto encoded = mii::protocol::encode_device_status_request({number,name,number,0}, true, inner.data(), inner.size());
        require(encoded.ok(), "numbered status request");
        set_command(command_packet, port, session_key, inner.data(), encoded.bytes_written);
        return encode(command_packet);
    };
    auto wire = numbered_command(3, device_name.c_str());
    result = AuthWireGateway::authorize_status(session,77,wire.data(),wire.size(),5,decoded);
    require(result.wire == WireError::kOk && result.authorization.replay == InboundDecision::kAcceptNew, "wire sequence advance");
    wire = numbered_command(2, device_name.c_str());
    result = AuthWireGateway::authorize_status(session,77,wire.data(),wire.size(),5,decoded);
    require(result.authorization.replay == InboundDecision::kAcceptReordered, "wire bounded reordering");
    wire = numbered_command(99, "WRONG-DEVICE");
    result = AuthWireGateway::authorize_status(session,77,wire.data(),wire.size(),5,decoded);
    require(result.wire == WireError::kWrongDevice && decoded.message_id == 0, "authenticated wrong device rejected");
    wire = numbered_command(4, device_name.c_str());
    result = AuthWireGateway::authorize_status(session,77,wire.data(),wire.size(),5,decoded);
    require(result.authorization.replay == InboundDecision::kAcceptNew, "wrong device did not advance replay window");
    const auto invalid_inner = std::array<std::uint8_t, 2>{0xff,0xff};
    set_command(command_packet,port,session_key,invalid_inner.data(),invalid_inner.size()); wire = encode(command_packet);
    result = AuthWireGateway::authorize_status(session,77,wire.data(),wire.size(),5,decoded);
    require(result.wire == WireError::kInvalidEnvelope && session.state() == State::kAuthenticated, "verified malformed inner payload does not alter authority/replay");
    // A bad MAC on malformed protobuf must fail AUTH first, before inner decode.
    command_packet.body.authenticated_envelope.authentication_tag.bytes[0] ^= 1; wire = encode(command_packet);
    result = AuthWireGateway::authorize_status(session,77,wire.data(),wire.size(),5,decoded);
    require(result.wire == WireError::kUnauthorized && result.authorization.error == Error::kProofFailed,
            "bad MAC rejected before protobuf decoding");
    require(session.state() == State::kBound, "bad MAC invalidates live session");
    session.disconnected(77); session.connected(78,{true,true,true,16});
    result = AuthWireGateway::authorize_status(session,77,command_wire.data(),command_wire.size(),6,decoded);
    require(result.authorization.error == Error::kStaleConnection, "old queued wire cannot enter new connection");
    std::cout << "AUTH_WIRE_TEST_PASS: " << checks
              << " assertions; eight Python/nanopb vectors, canonical bounds, verify-before-decode, identity and replay\n"
              << "HOST_TYPE_BUDGET: owner_session=" << sizeof(OwnerSession)
              << ", owner_record=" << sizeof(OwnerRecord)
              << ", auth_packet=" << sizeof(mii_protocol_v1_AuthenticationPacket)
              << " bytes; host ABI only\n";
}
