#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

#include "mii/owner_session.hpp"

namespace {
using namespace mii::auth;
using mii::protocol::InboundDecision;
unsigned checks = 0;
void require(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::cerr << "OWNER_SESSION_TEST_FAIL: " << message << '\n';
        std::exit(1);
    }
}
struct Host {
    OwnerRecord persisted{};
    bool stored{false};
    bool storage_failure{false};
    bool durable_but_reply_lost{false};
    bool random_failure{false};
    bool hmac_failure{false};
    unsigned nonce_counter{0};
    unsigned commits{0};
    static bool random(void* context, std::uint8_t* output, std::size_t size) noexcept {
        auto& host = *static_cast<Host*>(context);
        if (host.random_failure) { return false; }
        // Deterministic test nonce source ONLY. Production uses esp_fill_random.
        for (std::size_t i = 0; i < size; ++i) {
            output[i] = static_cast<std::uint8_t>(193 + i + host.nonce_counter);
        }
        ++host.nonce_counter;
        return true;
    }
    static bool hmac(void* context, const Secret& key, const std::uint8_t* data,
                      std::size_t size, Secret& result) noexcept {
        if (context != nullptr && static_cast<Host*>(context)->hmac_failure) { return false; }
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        NTSTATUS status = BCryptOpenAlgorithmProvider(
            &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
        if (status >= 0) {
            status = BCryptCreateHash(algorithm, &hash, nullptr, 0,
                const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0);
        }
        if (status >= 0) {
            status = BCryptHashData(hash, const_cast<PUCHAR>(data), static_cast<ULONG>(size), 0);
        }
        if (status >= 0) { status = BCryptFinishHash(hash, result.data(), 32, 0); }
        if (hash != nullptr) { BCryptDestroyHash(hash); }
        if (algorithm != nullptr) { BCryptCloseAlgorithmProvider(algorithm, 0); }
        return status >= 0;
    }
    static bool commit(void* context, const OwnerRecord& record) noexcept {
        auto& host = *static_cast<Host*>(context);
        ++host.commits;
        if (host.storage_failure) { return false; }
        host.persisted = record;
        host.stored = true;
        return !host.durable_but_reply_lost;
    }
    CryptoPort crypto() { return {this, random, hmac}; }
    StoragePort storage() { return {this, commit}; }
};
template <std::size_t N>
std::array<std::uint8_t, N> bytes(unsigned start) {
    std::array<std::uint8_t, N> result{};
    for (std::size_t i = 0; i < N; ++i) { result[i] = static_cast<std::uint8_t>(start + i); }
    return result;
}
const Id device = bytes<16>(1);
const Credential credential{bytes<16>(17), bytes<32>(33), bytes<32>(65), bytes<32>(97)};
const LinkEvidence secure{true, true, true, 16};
std::string hex(const std::uint8_t* data, std::size_t size) {
    const char* digits = "0123456789abcdef";
    std::string result;
    for (std::size_t i = 0; i < size; ++i) {
        result += digits[data[i] >> 4]; result += digits[data[i] & 15];
    }
    return result;
}
std::string property(const std::string& json, const char* name) {
    const auto key = json.find('"' + std::string(name) + '"');
    require(key != std::string::npos, "missing vector field");
    const auto start = json.find('"', json.find(':', key)) + 1;
    const auto end = json.find('"', start);
    return json.substr(start, end - start);
}
Secret proof(const OwnerRecord& record, const Secret& client, const Challenge& challenge,
             char role) {
    const auto transcript = proof_transcript(role, device, record, client, challenge.device_nonce);
    Secret result{};
    require(Host::hmac(nullptr, record.credential.owner_key, transcript.data(),
                       transcript.size(), result), "CNG HMAC failed");
    return result;
}
Secret command_mac(const Secret& key, const Id& id, const std::uint8_t* data,
                   std::size_t size, char direction = 'C') {
    std::array<std::uint8_t, 630> input{};
    std::memcpy(input.data(), "MIICTL1", 7); input[7] = direction;
    std::memcpy(input.data() + 8, id.data(), 16);
    input[24] = static_cast<std::uint8_t>(size); input[25] = static_cast<std::uint8_t>(size >> 8);
    std::memcpy(input.data() + 26, data, size);
    Secret result{};
    require(Host::hmac(nullptr, key, input.data(), 26 + size, result), "command HMAC failed");
    return result;
}
void bind(OwnerSession& session, Host& host, std::uint64_t connection = 1,
          std::uint64_t now = 1) {
    session.connected(connection, secure);
    require(session.open_local_binding_window(now) == Error::kOk, "open binding");
    require(session.propose_binding(connection, credential, now + 1) == Error::kOk, "candidate");
    require(host.commits == 0, "candidate must not persist before local confirmation");
    require(session.confirm_local_binding(connection, now + 2) == Error::kOk, "bind commit");
    require(session.state() == State::kBound && host.persisted.bound, "committed owner");
}
Secret authenticate(OwnerSession& session, Host& host, std::uint64_t connection,
                    std::uint64_t now) {
    const auto client = bytes<32>(161);
    Challenge challenge{};
    require(session.begin_session(connection, credential.owner_id, client, now, challenge)
        == Error::kOk, "begin owner session");
    require(challenge.device_proof == proof(host.persisted, client, challenge, 'D'), "device proof");
    const auto key = proof(host.persisted, client, challenge, 'K');
    require(session.finish_session(connection, proof(host.persisted, client, challenge, 'C'), now + 1)
        == Error::kOk, "finish owner session");
    return key;
}
void vector_test(const char* path) {
    std::ifstream file(path);
    require(file.good(), "auth vector input");
    const std::string json((std::istreambuf_iterator<char>(file)), {});
    OwnerRecord record{1, UINT64_C(0x0102030405060708), true, credential};
    const auto client = bytes<32>(161);
    Challenge challenge{bytes<32>(193), {}};
    const auto transcript = proof_transcript('C', device, record, client, challenge.device_nonce);
    require(hex(transcript.data(), transcript.size()) == property(json, "client_transcript_hex"),
            "canonical transcript independent vector");
    for (const auto& entry : std::array<std::pair<char, const char*>, 4>{{
        {'D', "device_proof_hex"}, {'C', "client_proof_hex"},
        {'K', "session_key_hex"}, {'I', "session_id_mac_hex"}}}) {
        const auto result = proof(record, client, challenge, entry.first);
        require(hex(result.data(), result.size()) == property(json, entry.second),
                "Python/CNG HMAC interoperability");
    }
    Secret rfc_key{}; std::fill_n(rfc_key.begin(), 20, 0x0b);
    Secret result{};
    const std::uint8_t text[]{'H','i',' ','T','h','e','r','e'};
    require(Host::hmac(nullptr, rfc_key, text, sizeof(text), result), "RFC HMAC");
    require(hex(result.data(), 32) == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
            "RFC4231 test case 1");
}
}  // namespace

int main(int argc, char** argv) {
    require(argc == 2, "vector path argument");
    vector_test(argv[1]);
    {
        Host host; OwnerSession session(device, host.crypto(), host.storage());
        require(session.state() == State::kStorageFault, "closed until storage restore");
        require(session.restore(StorageLoad::kFault, {}) == Error::kStorageFault, "storage fault");
        require(session.open_local_binding_window(1) == Error::kStorageFault, "fault cannot bind");
        require(session.recover_local_owner() == Error::kStorageFault, "cannot erase fault");
        auto invalid = OwnerRecord{}; invalid.format_version = 2;
        require(session.restore(StorageLoad::kOk, invalid) == Error::kStorageFault, "unknown format");
        invalid = {}; invalid.credential.owner_key[0] = 1;
        require(session.restore(StorageLoad::kOk, invalid) == Error::kStorageFault, "dirty tombstone");
        require(session.restore(StorageLoad::kAbsent, {}) == Error::kOk, "healthy absent storage");
        session.connected(1, secure);
        require(session.propose_binding(1, credential, 1) == Error::kPhysicalWindowClosed, "no window");
        require(session.open_local_binding_window(1) == Error::kOk, "local window");
        require(session.propose_binding(2, credential, 2) == Error::kStaleConnection, "stale generation");
        for (const auto link : std::array<LinkEvidence, 4>{{
            {false,true,true,16}, {true,false,true,16}, {true,true,false,16}, {true,true,true,15}}}) {
            session.connected(1, link);
            require(session.open_local_binding_window(2) == Error::kOk, "insecure window");
            require(session.propose_binding(1, credential, 3) == Error::kInsecureLink, "link downgrade");
        }
        session.connected(1, secure);
        require(session.open_local_binding_window(1) == Error::kOk, "timeout window");
        require(session.propose_binding(1, credential, 60001) == Error::kPhysicalWindowClosed, "window expiry");
        bind(session, host);
        require(session.confirm_local_binding(1, 4) == Error::kWrongState, "duplicate confirmation");
        require(session.open_local_binding_window(4) == Error::kWrongState, "bound cannot replace");
        auto key = authenticate(session, host, 1, 10);
        const auto id = session.session_id();
        const std::uint8_t payload[]{0x08, 0x01};
        auto mac = command_mac(key, id, payload, sizeof(payload));
        require(session.authorize(2, id, payload, sizeof(payload), mac, 1, 1, 12).error
            == Error::kStaleConnection, "old connection cannot authorize");
        auto wrong_id = id; wrong_id[0] ^= 1;
        require(session.authorize(1, wrong_id, payload, sizeof(payload), mac, 1, 1, 12).error
            == Error::kWrongSession, "wrong session rejected");
        auto result = session.authorize(1, id, payload, sizeof(payload), mac, 1, 1, 12);
        require(result.error == Error::kOk && result.replay == InboundDecision::kAcceptNew, "first command");
        result = session.authorize(1, id, payload, sizeof(payload), mac, 1, 1, 12);
        require(result.replay == InboundDecision::kDuplicateMessage, "duplicate command");
        require(session.authorize(1, id, payload, sizeof(payload), mac, 3, 3, 12).replay
            == InboundDecision::kAcceptNew, "new sequence");
        require(session.authorize(1, id, payload, sizeof(payload), mac, 2, 2, 12).replay
            == InboundDecision::kAcceptReordered, "bounded reordered sequence");
        require(session.authorize(1, id, nullptr, 0, mac, 4, 4, 12).error == Error::kInvalidArgument,
                "bad payload rejected");
        mac[0] ^= 1;
        require(session.authorize(1, id, payload, sizeof(payload), mac, 99, 99, 12).error
            == Error::kProofFailed, "bad MAC closes session");
        require(session.state() == State::kBound, "no authority after bad MAC");
        key = authenticate(session, host, 1, 20);
        require(session.request_revocation(1, 22) == Error::kOk, "request revocation");
        require(session.state() == State::kRevocationPending, "commands closed pending revoke");
        require(session.confirm_local_revocation(2, 23) == Error::kStaleConnection, "stale revoke confirmation");
        require(session.confirm_local_revocation(1, 23) == Error::kOk, "confirmed revocation");
        require(!host.persisted.bound && host.persisted.generation == 2 && valid_record(host.persisted),
                "durable tombstone generation");
        session.connected(3, secure);
        require(session.open_local_binding_window(24) == Error::kOk, "rebind window");
        require(session.propose_binding(3, credential, 25) == Error::kOk, "rebind candidate");
        require(session.confirm_local_binding(3, 26) == Error::kOk, "rebind commit");
        require(host.persisted.generation == 3, "rebind advances generation");
        OwnerSession reboot(device, host.crypto(), host.storage());
        require(reboot.restore(StorageLoad::kOk, host.persisted) == Error::kOk, "reboot reload");
        require(reboot.state() == State::kBound && reboot.session_id() == Id{}, "no live authority after reboot");
    }
    {
        Host host; OwnerSession session(device, host.crypto(), host.storage());
        require(session.restore(StorageLoad::kAbsent, {}) == Error::kOk, "pending restore");
        session.connected(10, secure);
        require(session.open_local_binding_window(10) == Error::kOk, "pending window");
        require(session.propose_binding(10, credential, 11) == Error::kOk, "pending proposal");
        session.disconnected(9);
        require(session.state() == State::kBindingPending, "old disconnect does not affect candidate");
        session.disconnected(10); session.connected(11, secure);
        require(session.confirm_local_binding(11, 12) == Error::kWrongState, "disconnect wipes pending credential");
        require(!host.stored, "disconnect before confirmation leaves no owner");
        session.connected(12, secure);
        require(session.open_local_binding_window(20) == Error::kOk, "pending expiry window");
        require(session.propose_binding(12, credential, 21) == Error::kOk, "expiry candidate");
        require(session.confirm_local_binding(12, 60020) == Error::kWrongState, "expired button confirmation");
    }
    for (const bool ambiguous : {false, true}) {
        Host host; OwnerSession session(device, host.crypto(), host.storage());
        require(session.restore(StorageLoad::kAbsent, {}) == Error::kOk, "failed commit restore");
        session.connected(1, secure);
        require(session.open_local_binding_window(1) == Error::kOk, "failed commit window");
        require(session.propose_binding(1, credential, 2) == Error::kOk, "failed commit proposal");
        host.storage_failure = !ambiguous; host.durable_but_reply_lost = ambiguous;
        require(session.confirm_local_binding(1, 3) == Error::kStorageFault, "ambiguous commit fail closed");
        require(session.state() == State::kStorageFault, "commit fault state");
        OwnerSession reboot(device, host.crypto(), host.storage());
        require(reboot.restore(host.stored ? StorageLoad::kOk : StorageLoad::kAbsent, host.persisted)
                == Error::kOk, "recover actual durable state");
        require(reboot.state() == (ambiguous ? State::kBound : State::kUnbound), "power-loss durable outcome");
    }
    {
        Host host; OwnerSession session(device, host.crypto(), host.storage());
        require(session.restore(StorageLoad::kAbsent, {}) == Error::kOk, "proof restore"); bind(session, host);
        const auto client = bytes<32>(161); Challenge challenge{};
        require(session.finish_session(1, {}, 10) == Error::kWrongState, "finish without challenge");
        require(session.begin_session(1, credential.owner_id, client, 10, challenge) == Error::kOk, "first nonce");
        const auto old = proof(host.persisted, client, challenge, 'C');
        const auto reflected = challenge.device_proof;
        require(session.finish_session(1, reflected, 11) == Error::kProofFailed, "reflection rejected");
        session.disconnected(1); session.connected(2, secure);
        require(session.begin_session(2, credential.owner_id, client, 12, challenge) == Error::kOk, "new challenge");
        require(session.finish_session(2, old, 13) == Error::kProofFailed, "old challenge replay rejected");
        session.disconnected(2); session.connected(3, secure);
        require(session.begin_session(3, credential.owner_id, client, 14, challenge) == Error::kOk, "third challenge");
        require(session.finish_session(3, {}, 15) == Error::kProofFailed, "third failure");
        session.disconnected(3); session.connected(4, secure);
        require(session.begin_session(4, credential.owner_id, client, 16, challenge) == Error::kBackoff,
                "disconnect cannot bypass backoff");
        require(session.begin_session(4, credential.owner_id, client, 60015, challenge) == Error::kOk,
                "backoff expires");
        require(session.finish_session(4, proof(host.persisted, client, challenge, 'C'), 90015)
            == Error::kWrongState, "challenge expires at deadline");
        static_cast<void>(authenticate(session, host, 4, 90020));
        session.poll(990021);
        require(session.state() == State::kBound, "session lifetime expires");
        host.random_failure = true;
        require(session.begin_session(4, credential.owner_id, client, 990030, challenge) == Error::kCryptoFault,
                "CSPRNG failure closes challenge");
        host.random_failure = false; host.hmac_failure = true;
        require(session.begin_session(4, credential.owner_id, client, 990031, challenge) == Error::kCryptoFault,
                "HMAC failure closes challenge");
        host.hmac_failure = false;
        static_cast<void>(authenticate(session, host, 4, 990040));
        host.storage_failure = true;
        require(session.recover_local_owner() == Error::kStorageFault, "revoke commit failure");
        require(session.state() == State::kStorageFault && host.persisted.bound, "no live authority on failed revoke");
    }
    {
        Host host; OwnerSession session(device, host.crypto(), host.storage());
        OwnerRecord exhausted{1, std::numeric_limits<std::uint64_t>::max(), false, {}};
        require(session.restore(StorageLoad::kOk, exhausted) == Error::kOk, "generation exhaustion restore");
        session.connected(1, secure);
        require(session.open_local_binding_window(1) == Error::kOk, "exhaustion window");
        require(session.propose_binding(1, credential, 2) == Error::kGenerationExhausted, "no generation wrap");
        require(session.open_local_binding_window(std::numeric_limits<std::uint64_t>::max())
            == Error::kInvalidArgument, "no deadline wrap");
    }
    std::cout << "OWNER_SESSION_TEST_PASS: " << checks
              << " assertions; RFC/Python/CNG HMAC, binding, revocation, persistence faults, replay and lifecycle\n";
}
