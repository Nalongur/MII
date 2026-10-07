#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "mii/session_state.hpp"

namespace mii::auth {

using Id = std::array<std::uint8_t, 16>;
using Secret = std::array<std::uint8_t, 32>;
inline constexpr std::size_t kProofTranscriptSize = 113;
inline constexpr std::size_t kMaxAuthenticatedPayload = 604;
inline constexpr std::uint64_t kBindingWindowMs = 60000;
inline constexpr std::uint64_t kChallengeTimeoutMs = 30000;
inline constexpr std::uint64_t kSessionLifetimeMs = 900000;
inline constexpr std::uint64_t kFailureBackoffMs = 60000;

struct Credential {
    Id owner_id{};
    Secret owner_key{};
    Secret https_token{};
    Secret certificate_sha256{};
};

struct OwnerRecord {
    std::uint32_t format_version{1};
    std::uint64_t generation{0};
    bool bound{false};
    Credential credential{};
};

enum class StorageLoad : std::uint8_t { kOk, kAbsent, kFault };
enum class State : std::uint8_t {
    kStorageFault, kUnbound, kBound, kBindingPending, kChallengePending,
    kAuthenticated, kRevocationPending,
};
enum class Error : std::uint8_t {
    kOk, kInvalidArgument, kWrongState, kStaleConnection, kInsecureLink,
    kPhysicalWindowClosed, kTimedOut, kStorageFault, kCryptoFault,
    kProofFailed, kBackoff, kWrongOwner, kWrongSession, kGenerationExhausted,
};

// Only the trusted SC-only NimBLE adapter may supply this evidence.
struct LinkEvidence {
    bool encrypted{false};
    bool authenticated{false};
    bool secure_connections_only{false};
    std::uint8_t key_size{0};
    [[nodiscard]] bool sufficient() const noexcept;
};

// Implement with platform CSPRNG and mature HMAC library. No fake/default port.
struct CryptoPort {
    void* context{nullptr};
    bool (*random)(void*, std::uint8_t*, std::size_t) noexcept{nullptr};
    bool (*hmac)(void*, const Secret&, const std::uint8_t*, std::size_t,
                 Secret&) noexcept{nullptr};
};
// Success means atomic durable commit AND readback on protected storage.
struct StoragePort {
    void* context{nullptr};
    bool (*commit)(void*, const OwnerRecord&) noexcept{nullptr};
};
struct Challenge {
    Secret device_nonce{};
    Secret device_proof{};
};
struct Authorization {
    Error error{Error::kWrongState};
    mii::protocol::InboundDecision replay{
        mii::protocol::InboundDecision::kInvalidArgument};
};

[[nodiscard]] bool valid_record(const OwnerRecord&) noexcept;
void wipe(void*, std::size_t) noexcept;
[[nodiscard]] bool constant_time_equal(const std::uint8_t*,
                                       const std::uint8_t*, std::size_t) noexcept;
[[nodiscard]] std::array<std::uint8_t, kProofTranscriptSize> proof_transcript(
    std::uint8_t role, const Id& device_id, const OwnerRecord&,
    const Secret& client_nonce, const Secret& device_nonce) noexcept;

// Single-task ownership. Monotonic milliseconds; no trusted wall clock needed.
// This component is not wired to the experimental business RX dispatcher.
class OwnerSession {
public:
    OwnerSession(const Id& device_id, CryptoPort, StoragePort) noexcept;
    ~OwnerSession();
    OwnerSession(const OwnerSession&) = delete;
    OwnerSession& operator=(const OwnerSession&) = delete;

    [[nodiscard]] Error restore(StorageLoad, const OwnerRecord&) noexcept;
    void connected(std::uint64_t connection_generation, LinkEvidence) noexcept;
    void disconnected(std::uint64_t connection_generation) noexcept;
    // Called ONLY by trusted local UI; remote protocol cannot invoke these.
    [[nodiscard]] Error open_local_binding_window(std::uint64_t now_ms) noexcept;
    [[nodiscard]] Error propose_binding(std::uint64_t connection_generation,
        const Credential&, std::uint64_t now_ms) noexcept;
    [[nodiscard]] Error confirm_local_binding(std::uint64_t connection_generation,
        std::uint64_t now_ms) noexcept;
    [[nodiscard]] Error begin_session(std::uint64_t connection_generation,
        const Id& owner_id, const Secret& client_nonce, std::uint64_t now_ms,
        Challenge&) noexcept;
    [[nodiscard]] Error finish_session(std::uint64_t connection_generation,
        const Secret& client_proof, std::uint64_t now_ms) noexcept;
    [[nodiscard]] Authorization authorize(std::uint64_t connection_generation,
        const Id& session_id, const std::uint8_t* canonical_payload,
        std::size_t size, const Secret& mac, std::uint64_t message_id,
        std::uint64_t sequence_number, std::uint64_t now_ms) noexcept;
    [[nodiscard]] Error verify_control_mac(std::uint64_t connection_generation,
        const Id& session_id, const std::uint8_t* canonical_payload,
        std::size_t size, const Secret& mac, std::uint64_t now_ms) noexcept;
    [[nodiscard]] Error request_revocation(std::uint64_t connection_generation,
        std::uint64_t now_ms) noexcept;
    [[nodiscard]] Error confirm_local_revocation(std::uint64_t connection_generation,
        std::uint64_t now_ms) noexcept;
    [[nodiscard]] Error recover_local_owner() noexcept;
    void poll(std::uint64_t now_ms) noexcept;
    [[nodiscard]] State state() const noexcept { return state_; }
    [[nodiscard]] std::uint64_t binding_generation() const noexcept {
        return owner_.generation;
    }
    [[nodiscard]] const Id& session_id() const noexcept { return session_id_; }

private:
    friend class AuthWireGateway;
    [[nodiscard]] Authorization observe_authorized_numbers(std::uint64_t message_id,
        std::uint64_t sequence_number) noexcept;
    [[nodiscard]] Error check_link(std::uint64_t, std::uint64_t) noexcept;
    [[nodiscard]] Error commit_record(const OwnerRecord&) noexcept;
    [[nodiscard]] Error revoke() noexcept;
    [[nodiscard]] Error reject_proof(std::uint64_t) noexcept;
    [[nodiscard]] bool derive(std::uint8_t role, Secret&) noexcept;
    void clear_live() noexcept;
    void settle() noexcept;

    Id device_id_{};
    CryptoPort crypto_{};
    StoragePort storage_{};
    OwnerRecord owner_{};
    Credential candidate_{};
    State state_{State::kStorageFault};
    LinkEvidence link_{};
    std::uint64_t connection_{0};
    std::uint64_t binding_deadline_{0};
    std::uint64_t operation_deadline_{0};
    std::uint64_t backoff_deadline_{0};
    std::uint8_t failures_{0};
    Secret client_nonce_{};
    Secret device_nonce_{};
    Secret session_key_{};
    Id session_id_{};
    mii::protocol::InboundSessionTracker replay_{};
};

}  // namespace mii::auth
