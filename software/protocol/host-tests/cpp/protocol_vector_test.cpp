#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open vector: " + path);
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string json_string(const std::string& json, const std::string& name) {
    const std::regex pattern{"\\\"" + name + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\""};
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        throw std::runtime_error("missing JSON string: " + name);
    }
    return match[1].str();
}

std::uint64_t json_uint(const std::string& json, const std::string& name) {
    const std::regex pattern{"\\\"" + name + "\\\"\\s*:\\s*([0-9]+)"};
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        throw std::runtime_error("missing JSON integer: " + name);
    }
    return std::stoull(match[1].str());
}

bool json_bool(const std::string& json, const std::string& name) {
    const std::regex pattern{"\\\"" + name + "\\\"\\s*:\\s*(true|false)"};
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        throw std::runtime_error("missing JSON boolean: " + name);
    }
    return match[1].str() == "true";
}

void append_varint(Bytes& output, std::uint64_t value) {
    while (value > 0x7fU) {
        output.push_back(static_cast<std::uint8_t>((value & 0x7fU) | 0x80U));
        value >>= 7U;
    }
    output.push_back(static_cast<std::uint8_t>(value));
}

void append_varint_field(Bytes& output, std::uint32_t field_number, std::uint64_t value) {
    if (value == 0U) {
        return;
    }
    append_varint(output, static_cast<std::uint64_t>(field_number) << 3U);
    append_varint(output, value);
}

void append_bytes_field(Bytes& output, std::uint32_t field_number, const Bytes& value) {
    if (value.empty()) {
        return;
    }
    append_varint(output, (static_cast<std::uint64_t>(field_number) << 3U) | 2U);
    append_varint(output, value.size());
    output.insert(output.end(), value.begin(), value.end());
}

void append_string_field(Bytes& output, std::uint32_t field_number, const std::string& value) {
    append_bytes_field(output, field_number, Bytes{value.begin(), value.end()});
}

std::string to_hex(const Bytes& bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2U);
    for (const std::uint8_t byte : bytes) {
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0fU]);
    }
    return result;
}

std::string normalized_hex(std::string value) {
    value.erase(
        std::remove_if(value.begin(), value.end(), [](unsigned char character) {
            return std::isspace(character) != 0 || character == ':' || character == '_' ||
                   character == '-';
        }),
        value.end());
    return value;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            throw std::runtime_error("usage: protocol_vector_test <vector.json>");
        }

        const std::string json = read_file(argv[1]);

        Bytes version;
        append_varint_field(version, 1, json_uint(json, "major"));
        append_varint_field(version, 2, json_uint(json, "minor"));
        append_varint_field(version, 3, json_uint(json, "patch"));

        Bytes payload;
        append_varint_field(payload, 1, json_bool(json, "include_runtime_counters") ? 1U : 0U);

        Bytes envelope;
        append_bytes_field(envelope, 1, version);
        append_varint_field(envelope, 2, json_uint(json, "message_id"));
        append_string_field(envelope, 3, json_string(json, "device_id"));
        append_varint_field(envelope, 4, json_uint(json, "sequence_number"));
        append_varint_field(envelope, 5, json_uint(json, "sent_at_unix_ms"));
        append_varint_field(envelope, 6, json_uint(json, "payload_type"));
        append_bytes_field(envelope, 7, payload);

        const std::string payload_hex = to_hex(payload);
        const std::string envelope_hex = to_hex(envelope);
        const std::string expected_payload =
            normalized_hex(json_string(json, "expected_payload_wire_hex"));
        const std::string expected_envelope = normalized_hex(json_string(json, "expected_wire_hex"));
        const std::uint64_t expected_length = json_uint(json, "expected_wire_length");

        if (payload_hex != expected_payload) {
            throw std::runtime_error("payload mismatch: expected " + expected_payload + ", got " +
                                     payload_hex);
        }
        if (envelope_hex != expected_envelope) {
            throw std::runtime_error("envelope mismatch: expected " + expected_envelope + ", got " +
                                     envelope_hex);
        }
        if (envelope.size() != expected_length) {
            throw std::runtime_error("length mismatch: expected " + std::to_string(expected_length) +
                                     ", got " + std::to_string(envelope.size()));
        }

        std::cout << "C++ protocol vector PASS: " << envelope.size() << " bytes\n";
        std::cout << envelope_hex << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "C++ protocol vector FAIL: " << error.what() << '\n';
        return 1;
    }
}
