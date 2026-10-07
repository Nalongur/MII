package com.mii.protocol

import com.google.protobuf.ByteString
import com.mii.protocol.v1.*
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

private fun String.authString(name: String): String =
    Regex("\"${Regex.escape(name)}\"\\s*:\\s*\"([^\"]*)\"").find(this)!!.groupValues[1]
private fun String.authLong(name: String): Long =
    Regex("\"${Regex.escape(name)}\"\\s*:\\s*([0-9]+)").find(this)!!.groupValues[1].toLong()
private fun String.unhex(): ByteArray = chunked(2).map { it.toInt(16).toByte() }.toByteArray()
private fun ByteArray.authHex() = joinToString("") { "%02x".format(it.toInt() and 255) }
private fun hmac(key: ByteArray, input: ByteArray): ByteArray = Mac.getInstance("HmacSHA256").run {
    init(SecretKeySpec(key, "HmacSHA256")); doFinal(input)
}

fun checkAuthenticationVectors(path: File) {
    val json = path.readText(Charsets.UTF_8)
    fun b(name: String) = ByteString.copyFrom(json.authString(name + "_hex").unhex())
    val generation = json.authLong("binding_generation")
    val key = b("owner_key").toByteArray()
    val transcript = "MIIAUTH1C".toByteArray(Charsets.US_ASCII) + b("device_id").toByteArray() +
        b("owner_id").toByteArray() + ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(generation).array() +
        b("client_nonce").toByteArray() + b("device_nonce").toByteArray()
    check(transcript.authHex() == json.authString("client_transcript_hex"))
    for ((role, name) in listOf('D' to "device_proof", 'C' to "client_proof", 'K' to "session_key", 'I' to "session_id_mac")) {
        transcript[8] = role.code.toByte()
        check(hmac(key, transcript).authHex() == json.authString(name + "_hex"))
    }
    val inner = Envelope.newBuilder().setProtocolVersion(ProtocolVersion.newBuilder().setMajor(1))
        .setMessageId(1).setDeviceId(json.authString("device_id_hex")).setSequenceNumber(1)
        .setPayloadType(PayloadType.PAYLOAD_TYPE_DEVICE_STATUS_REQUEST)
        .setPayload(DeviceStatusRequest.newBuilder().setIncludeRuntimeCounters(true).build().toByteString()).build()
    check(inner.toByteArray().authHex() == json.authString("authenticated_envelope_hex"))
    val innerBytes = inner.toByteArray()
    val macInput = "MIICTL1C".toByteArray(Charsets.US_ASCII) + b("session_id").toByteArray() +
        byteArrayOf(innerBytes.size.toByte(), (innerBytes.size ushr 8).toByte()) + innerBytes
    check(hmac(b("session_key").toByteArray(), macInput).authHex() == json.authString("control_mac_hex"))
    fun packet(request: Long) = AuthenticationPacket.newBuilder().setAuthenticationVersion(1).setRequestId(request)
    val packets = listOf(
        "binding_proposal" to packet(1).setBindingProposal(BindingProposal.newBuilder().setOwnerId(b("owner_id"))
            .setOwnerKey(b("owner_key")).setHttpsToken(ByteString.copyFrom(ByteArray(32) { (65 + it).toByte() }))
            .setCertificateSha256(ByteString.copyFrom(ByteArray(32) { (97 + it).toByte() }))).build(),
        "binding_accepted" to packet(2).setBindingAccepted(BindingAccepted.newBuilder().setOwnerId(b("owner_id"))
            .setBindingGeneration(generation)).build(),
        "session_hello" to packet(3).setSessionHello(SessionHello.newBuilder().setOwnerId(b("owner_id"))
            .setClientNonce(b("client_nonce"))).build(),
        "session_challenge" to packet(4).setSessionChallenge(SessionChallenge.newBuilder().setDeviceNonce(b("device_nonce"))
            .setDeviceProof(b("device_proof")).setBindingGeneration(generation)).build(),
        "session_proof" to packet(5).setSessionProof(SessionProof.newBuilder().setClientProof(b("client_proof"))).build(),
        "session_ready" to packet(6).setSessionReady(SessionReady.newBuilder().setSessionId(b("session_id"))
            .setBindingGeneration(generation)).build(),
        "authenticated" to packet(7).setAuthenticatedEnvelope(AuthenticatedEnvelope.newBuilder().setSessionId(b("session_id"))
            .setCanonicalEnvelope(inner.toByteString()).setAuthenticationTag(b("control_mac"))).build(),
        "failure" to packet(8).setFailure(AuthenticationFailure.newBuilder()
            .setCode(AuthenticationFailureCode.AUTHENTICATION_FAILURE_REJECTED)).build(),
    )
    for ((name, message) in packets) {
        val wire = message.toByteArray()
        check(wire.size <= 604 && wire.authHex() == json.authString("packet_${name}_hex"))
        check(AuthenticationPacket.parseFrom(wire) == message)
    }
    println("KOTLIN_AUTH_VECTOR_PASS: eight generated packets and Python/CNG/mbedTLS/JVM role/control HMAC equality")
}
