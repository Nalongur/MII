#include "mii/owner_session.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace mii::auth {
namespace {
template <std::size_t N>
bool nonzero(const std::array<std::uint8_t, N>& value) noexcept {
    std::uint8_t aggregate = 0;
    for (const auto byte : value) { aggregate |= byte; }
    return aggregate != 0;
}
bool valid_credential(const Credential& value) noexcept {
    return nonzero(value.owner_id) && nonzero(value.owner_key) &&
           nonzero(value.https_token) && nonzero(value.certificate_sha256);
}
bool deadline(std::uint64_t now, std::uint64_t duration,
              std::uint64_t& result) noexcept {
    if (now > std::numeric_limits<std::uint64_t>::max() - duration) {
        return false;
    }
    result = now + duration;
    return true;
}
}  // namespace

bool LinkEvidence::sufficient() const noexcept {
    return encrypted && authenticated && secure_connections_only && key_size == 16;
}

void wipe(void* memory, std::size_t size) noexcept {
    auto* bytes = static_cast<volatile std::uint8_t*>(memory);
    while (size-- != 0) { *bytes++ = 0; }
}

bool constant_time_equal(const std::uint8_t* left, const std::uint8_t* right,
                         std::size_t size) noexcept {
    volatile unsigned difference = 0;
    for (std::size_t index = 0; index < size; ++index) {
        difference |= static_cast<unsigned>(left[index] ^ right[index]);
    }
    return difference == 0;
}

bool valid_record(const OwnerRecord& record) noexcept {
    if (record.format_version != 1) { return false; }
    if (record.bound) {
        return record.generation != 0 && valid_credential(record.credential);
    }
    const auto& c = record.credential;
    return !nonzero(c.owner_id) && !nonzero(c.owner_key) &&
           !nonzero(c.https_token) && !nonzero(c.certificate_sha256);
}

std::array<std::uint8_t, kProofTranscriptSize> proof_transcript(
    std::uint8_t role, const Id& device_id, const OwnerRecord& record,
    const Secret& client_nonce, const Secret& device_nonce) noexcept {
    std::array<std::uint8_t, kProofTranscriptSize> result{};
    constexpr char kDomain[] = "MIIAUTH1";
    std::memcpy(result.data(), kDomain, 8);
    result[8] = role;
    std::memcpy(result.data() + 9, device_id.data(), 16);
    std::memcpy(result.data() + 25, record.credential.owner_id.data(), 16);
    for (unsigned i = 0; i < 8; ++i) {
        result[41 + i] = static_cast<std::uint8_t>(record.generation >> (i * 8));
    }
    std::memcpy(result.data() + 49, client_nonce.data(), 32);
    std::memcpy(result.data() + 81, device_nonce.data(), 32);
    return result;
}

OwnerSession::OwnerSession(const Id& device_id, CryptoPort crypto,
                           StoragePort storage) noexcept
    : device_id_(device_id), crypto_(crypto), storage_(storage) {}

OwnerSession::~OwnerSession() {
    clear_live();
    wipe(&owner_, sizeof(owner_));
}

void OwnerSession::clear_live() noexcept {
    wipe(&candidate_, sizeof(candidate_));
    wipe(client_nonce_.data(), client_nonce_.size());
    wipe(device_nonce_.data(), device_nonce_.size());
    wipe(session_key_.data(), session_key_.size());
    session_id_.fill(0);
    replay_.reset();
    operation_deadline_ = 0;
    binding_deadline_ = 0;
}

void OwnerSession::settle() noexcept {
    if (state_ != State::kStorageFault) {
        state_ = owner_.bound ? State::kBound : State::kUnbound;
    }
}

Error OwnerSession::restore(StorageLoad load, const OwnerRecord& record) noexcept {
    clear_live();
    connection_ = 0;
    link_ = {};
    wipe(&owner_, sizeof(owner_));
    state_ = State::kStorageFault;
    if (!nonzero(device_id_) || crypto_.random == nullptr ||
        crypto_.hmac == nullptr || storage_.commit == nullptr ||
        load == StorageLoad::kFault) {
        return Error::kStorageFault;
    }
    if (load == StorageLoad::kAbsent) {
        owner_ = OwnerRecord{};
    } else if (valid_record(record)) {
        owner_ = record;
    } else {
        return Error::kStorageFault;
    }
    state_ = owner_.bound ? State::kBound : State::kUnbound;
    return Error::kOk;
}

void OwnerSession::connected(std::uint64_t connection, LinkEvidence link) noexcept {
    // Any replacement clears all volatile authority, even if a handle repeats.
    clear_live();
    settle();
    connection_ = connection;
    link_ = connection != 0 ? link : LinkEvidence{};
}

void OwnerSession::disconnected(std::uint64_t connection) noexcept {
    if (connection != connection_ || connection == 0) { return; }
    clear_live();
    settle();
    connection_ = 0;
    link_ = {};
}

void OwnerSession::poll(std::uint64_t now) noexcept {
    if (backoff_deadline_ != 0 && now >= backoff_deadline_) {
        backoff_deadline_ = 0;
        failures_ = 0;
    }
    if ((operation_deadline_ != 0 && now >= operation_deadline_) ||
        (binding_deadline_ != 0 && now >= binding_deadline_)) {
        clear_live();
        settle();
    }
}

Error OwnerSession::check_link(std::uint64_t connection,
                               std::uint64_t now) noexcept {
    poll(now);
    if (state_ == State::kStorageFault) { return Error::kStorageFault; }
    if (connection == 0 || connection != connection_) { return Error::kStaleConnection; }
    if (!link_.sufficient()) { return Error::kInsecureLink; }
    if (backoff_deadline_ != 0) { return Error::kBackoff; }
    return Error::kOk;
}

Error OwnerSession::open_local_binding_window(std::uint64_t now) noexcept {
    poll(now);
    if (state_ == State::kStorageFault) { return Error::kStorageFault; }
    if (state_ != State::kUnbound) { return Error::kWrongState; }
    return deadline(now, kBindingWindowMs, binding_deadline_)
               ? Error::kOk : Error::kInvalidArgument;
}

Error OwnerSession::propose_binding(std::uint64_t connection,
    const Credential& candidate, std::uint64_t now) noexcept {
    const Error guard = check_link(connection, now);
    if (guard != Error::kOk) { return guard; }
    if (state_ != State::kUnbound) { return Error::kWrongState; }
    if (binding_deadline_ == 0) { return Error::kPhysicalWindowClosed; }
    if (!valid_credential(candidate)) { return Error::kInvalidArgument; }
    if (owner_.generation == std::numeric_limits<std::uint64_t>::max()) {
        return Error::kGenerationExhausted;
    }
    candidate_ = candidate;
    operation_deadline_ = binding_deadline_;
    state_ = State::kBindingPending;
    return Error::kOk;
}

Error OwnerSession::commit_record(const OwnerRecord& record) noexcept {
    clear_live();
    if (!storage_.commit(storage_.context, record)) {
        state_ = State::kStorageFault;
        wipe(&owner_.credential, sizeof(owner_.credential));
        return Error::kStorageFault;
    }
    wipe(&owner_, sizeof(owner_));
    owner_ = record;
    state_ = owner_.bound ? State::kBound : State::kUnbound;
    return Error::kOk;
}

Error OwnerSession::confirm_local_binding(std::uint64_t connection,
                                         std::uint64_t now) noexcept {
    const Error guard = check_link(connection, now);
    if (guard != Error::kOk) { return guard; }
    if (state_ != State::kBindingPending) { return Error::kWrongState; }
    OwnerRecord record{};
    record.generation = owner_.generation + 1;
    record.bound = true;
    record.credential = candidate_;
    const Error result = commit_record(record);
    wipe(&record, sizeof(record));
    return result;
}

bool OwnerSession::derive(std::uint8_t role, Secret& result) noexcept {
    const auto transcript = proof_transcript(role, device_id_, owner_,
                                              client_nonce_, device_nonce_);
    return crypto_.hmac(crypto_.context, owner_.credential.owner_key,
                        transcript.data(), transcript.size(), result);
}

Error OwnerSession::begin_session(std::uint64_t connection, const Id& owner_id,
    const Secret& client_nonce, std::uint64_t now, Challenge& challenge) noexcept {
    challenge = {};
    const Error guard = check_link(connection, now);
    if (guard != Error::kOk) { return guard; }
    if (state_ != State::kBound) { return Error::kWrongState; }
    if (!constant_time_equal(owner_id.data(), owner_.credential.owner_id.data(), 16)) {
        static_cast<void>(reject_proof(now));
        return Error::kWrongOwner;
    }
    if (!nonzero(client_nonce) || !deadline(now, kChallengeTimeoutMs, operation_deadline_)) {
        return Error::kInvalidArgument;
    }
    client_nonce_ = client_nonce;
    if (!crypto_.random(crypto_.context, device_nonce_.data(), device_nonce_.size()) ||
        !nonzero(device_nonce_) || !derive('D', challenge.device_proof)) {
        clear_live();
        challenge = {};
        return Error::kCryptoFault;
    }
    challenge.device_nonce = device_nonce_;
    state_ = State::kChallengePending;
    return Error::kOk;
}

Error OwnerSession::reject_proof(std::uint64_t now) noexcept {
    clear_live();
    settle();
    if (++failures_ >= 3) {
        if (!deadline(now, kFailureBackoffMs, backoff_deadline_)) {
            backoff_deadline_ = std::numeric_limits<std::uint64_t>::max();
        }
    }
    return Error::kProofFailed;
}

Error OwnerSession::finish_session(std::uint64_t connection,
    const Secret& proof, std::uint64_t now) noexcept {
    const Error guard = check_link(connection, now);
    if (guard != Error::kOk) { return guard; }
    if (state_ != State::kChallengePending) { return Error::kWrongState; }
    Secret expected{};
    if (!derive('C', expected)) {
        wipe(expected.data(), expected.size());
        clear_live();
        settle();
        return Error::kCryptoFault;
    }
    const bool matches = constant_time_equal(expected.data(), proof.data(), 32);
    wipe(expected.data(), expected.size());
    if (!matches) { return reject_proof(now); }
    Secret id_mac{};
    if (!derive('K', session_key_) || !derive('I', id_mac) ||
        !deadline(now, kSessionLifetimeMs, operation_deadline_)) {
        wipe(id_mac.data(), id_mac.size());
        clear_live();
        settle();
        return Error::kCryptoFault;
    }
    std::copy_n(id_mac.begin(), session_id_.size(), session_id_.begin());
    wipe(id_mac.data(), id_mac.size());
    wipe(client_nonce_.data(), client_nonce_.size());
    wipe(device_nonce_.data(), device_nonce_.size());
    replay_.reset();
    failures_ = 0;
    state_ = State::kAuthenticated;
    return Error::kOk;
}

Error OwnerSession::verify_control_mac(std::uint64_t connection,
    const Id& id, const std::uint8_t* payload, std::size_t size,
    const Secret& mac, std::uint64_t now) noexcept {
    const Error guard = check_link(connection, now);
    if (guard != Error::kOk) { return guard; }
    if (state_ != State::kAuthenticated) { return Error::kWrongState; }
    if (payload == nullptr || size == 0 || size > kMaxAuthenticatedPayload) { return Error::kInvalidArgument; }
    if (!constant_time_equal(id.data(), session_id_.data(), id.size())) {
        return Error::kWrongSession;
    }
    std::array<std::uint8_t, 26 + kMaxAuthenticatedPayload> input{};
    std::memcpy(input.data(), "MIICTL1", 7);
    input[7] = 'C';
    std::memcpy(input.data() + 8, id.data(), 16);
    input[24] = static_cast<std::uint8_t>(size);
    input[25] = static_cast<std::uint8_t>(size >> 8);
    std::memcpy(input.data() + 26, payload, size);
    Secret expected{};
    if (!crypto_.hmac(crypto_.context, session_key_, input.data(), 26 + size, expected)) {
        wipe(expected.data(), expected.size());
        clear_live();
        settle();
        return Error::kCryptoFault;
    }
    const bool matches = constant_time_equal(expected.data(), mac.data(), mac.size());
    wipe(expected.data(), expected.size());
    if (!matches) { return reject_proof(now); }
    return Error::kOk;
}

Authorization OwnerSession::observe_authorized_numbers(std::uint64_t message_id,
                                                       std::uint64_t sequence) noexcept {
    if (state_ != State::kAuthenticated) { return {}; }
    if (message_id == 0 || sequence == 0) { return {Error::kInvalidArgument, {}}; }
    return {Error::kOk, replay_.observe(message_id, sequence)};
}

Authorization OwnerSession::authorize(std::uint64_t connection, const Id& id,
    const std::uint8_t* payload, std::size_t size, const Secret& mac,
    std::uint64_t message_id, std::uint64_t sequence, std::uint64_t now) noexcept {
    const Error guard = verify_control_mac(connection, id, payload, size, mac, now);
    if (guard != Error::kOk) { return {guard, {}}; }
    // Trusted low-level/test entry. The wire gateway decodes these numbers only
    // AFTER MAC verification and directly calls observe_authorized_numbers.
    return observe_authorized_numbers(message_id, sequence);
}

Error OwnerSession::request_revocation(std::uint64_t connection,
                                      std::uint64_t now) noexcept {
    const Error guard = check_link(connection, now);
    if (guard != Error::kOk) { return guard; }
    if (state_ != State::kAuthenticated) { return Error::kWrongState; }
    clear_live();
    state_ = State::kRevocationPending;
    if (!deadline(now, kBindingWindowMs, operation_deadline_)) {
        settle();
        return Error::kInvalidArgument;
    }
    return Error::kOk;
}

Error OwnerSession::revoke() noexcept {
    clear_live();
    settle();
    if (state_ == State::kStorageFault) { return Error::kStorageFault; }
    if (!owner_.bound) { return Error::kWrongState; }
    if (owner_.generation == std::numeric_limits<std::uint64_t>::max()) {
        return Error::kGenerationExhausted;
    }
    OwnerRecord tombstone{};
    tombstone.generation = owner_.generation + 1;
    return commit_record(tombstone);
}

Error OwnerSession::confirm_local_revocation(std::uint64_t connection,
                                             std::uint64_t now) noexcept {
    const Error guard = check_link(connection, now);
    if (guard != Error::kOk) { return guard; }
    if (state_ != State::kRevocationPending) { return Error::kWrongState; }
    return revoke();
}

Error OwnerSession::recover_local_owner() noexcept { return revoke(); }

}  // namespace mii::auth
