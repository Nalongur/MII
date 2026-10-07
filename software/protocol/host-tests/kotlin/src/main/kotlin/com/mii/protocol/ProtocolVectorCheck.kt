package com.mii.protocol

import com.google.protobuf.ByteString
import com.mii.protocol.v1.DeviceStatusRequest
import com.mii.protocol.v1.Envelope
import com.mii.protocol.v1.PayloadType
import com.mii.protocol.v1.ProtocolVersion
import java.io.File

private fun String.jsonString(name: String): String =
    Regex("\\\"${Regex.escape(name)}\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"")
        .find(this)
        ?.groupValues
        ?.get(1)
        ?: error("missing JSON string: $name")

private fun String.jsonLong(name: String): Long =
    Regex("\\\"${Regex.escape(name)}\\\"\\s*:\\s*([0-9]+)")
        .find(this)
        ?.groupValues
        ?.get(1)
        ?.toLong()
        ?: error("missing JSON integer: $name")

private fun String.jsonBoolean(name: String): Boolean =
    Regex("\\\"${Regex.escape(name)}\\\"\\s*:\\s*(true|false)")
        .find(this)
        ?.groupValues
        ?.get(1)
        ?.toBooleanStrict()
        ?: error("missing JSON boolean: $name")

private fun ByteArray.hex(): String =
    joinToString(separator = "") { "%02x".format(it.toInt() and 0xff) }

private fun assertDecodedFields(
    envelope: Envelope,
    expectedPayload: DeviceStatusRequest,
    json: String,
) {
    check(envelope.protocolVersion.major.toLong() == json.jsonLong("major"))
    check(envelope.protocolVersion.minor.toLong() == json.jsonLong("minor"))
    check(envelope.protocolVersion.patch.toLong() == json.jsonLong("patch"))
    check(envelope.messageId == json.jsonLong("message_id"))
    check(envelope.deviceId == json.jsonString("device_id"))
    check(envelope.sequenceNumber == json.jsonLong("sequence_number"))
    check(envelope.sentAtUnixMs == json.jsonLong("sent_at_unix_ms"))
    check(envelope.payloadTypeValue.toLong() == json.jsonLong("payload_type"))
    check(DeviceStatusRequest.parseFrom(envelope.payload) == expectedPayload)
}

fun main(args: Array<String>) {
    require(args.size == 2) { "usage: ProtocolVectorCheck <status-vector.json> <auth-vector.json>" }
    val json = File(args[0]).readText(Charsets.UTF_8)

    val major = json.jsonLong("major").toInt()
    val minor = json.jsonLong("minor").toInt()
    val patch = json.jsonLong("patch").toInt()
    val version = ProtocolVersion.newBuilder()
        .setMajor(major)
        .setMinor(minor)
        .setPatch(patch)
        .build()
    val payloadMessage = DeviceStatusRequest.newBuilder()
        .setIncludeRuntimeCounters(json.jsonBoolean("include_runtime_counters"))
        .build()
    val payload = payloadMessage.toByteArray()
    val payloadType = PayloadType.forNumber(json.jsonLong("payload_type").toInt())
        ?: error("unknown payload_type in test vector")
    val envelopeMessage = Envelope.newBuilder()
        .setProtocolVersion(version)
        .setMessageId(json.jsonLong("message_id"))
        .setDeviceId(json.jsonString("device_id"))
        .setSequenceNumber(json.jsonLong("sequence_number"))
        .setSentAtUnixMs(json.jsonLong("sent_at_unix_ms"))
        .setPayloadType(payloadType)
        .setPayload(ByteString.copyFrom(payload))
        .build()
    val envelope = envelopeMessage.toByteArray()

    val expectedPayload = json.jsonString("expected_payload_wire_hex").lowercase()
    val expectedEnvelope = json.jsonString("expected_wire_hex").lowercase()
    val expectedLength = json.jsonLong("expected_wire_length").toInt()

    check(payload.hex() == expectedPayload) {
        "payload mismatch: expected $expectedPayload, got ${payload.hex()}"
    }
    check(envelope.hex() == expectedEnvelope) {
        "envelope mismatch: expected $expectedEnvelope, got ${envelope.hex()}"
    }
    check(envelope.size == expectedLength) {
        "length mismatch: expected $expectedLength, got ${envelope.size}"
    }

    assertDecodedFields(Envelope.parseFrom(envelope), payloadMessage, json)

    // Field 127 (wire type 0), value 1. A v1 reader must safely retain and
    // ignore this synthetic future field while decoding all known fields.
    val futureWire = envelope + byteArrayOf(0xf8.toByte(), 0x07, 0x01)
    val futureEnvelope = Envelope.parseFrom(futureWire)
    assertDecodedFields(futureEnvelope, payloadMessage, json)
    check(futureEnvelope.unknownFields.asMap().containsKey(127)) {
        "generated parser discarded synthetic unknown field 127"
    }

    println("Kotlin generated-protobuf vector PASS: ${envelope.size} bytes")
    println("Generated parser unknown-field PASS: field 127 retained")
    println(envelope.hex())
    checkAuthenticationVectors(File(args[1]))
}
