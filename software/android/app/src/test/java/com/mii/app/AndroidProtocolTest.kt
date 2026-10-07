package com.mii.app

import com.mii.protocol.checkAuthenticationVectors
import com.mii.protocol.v1.Envelope
import com.mii.protocol.v1.PayloadType
import java.io.File
import org.junit.Assert.*
import org.junit.Test

class AndroidProtocolTest {
    @Test fun eightCanonicalAuthenticationVectorsAndHmac() {
        val fixture = requireNotNull(javaClass.classLoader!!.getResource("authentication_v1.json"))
        checkAuthenticationVectors(File(fixture.toURI()))
    }

    @Test fun legacyStatusVectorAndUnknownFieldRemainCompatible() {
        val json = javaClass.classLoader!!.getResource("device_status_request_v1.json")!!.readText()
        val hex = Regex("\"expected_wire_hex\"\\s*:\\s*\"([^\"]+)\"").find(json)!!.groupValues[1]
        val bytes = hex.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
        val decoded = Envelope.parseFrom(bytes)
        assertEquals(PayloadType.PAYLOAD_TYPE_DEVICE_STATUS_REQUEST, decoded.payloadType)
        assertEquals(31, bytes.size)
        assertArrayEquals(bytes, decoded.toByteArray())
        val futureBytes = bytes + byteArrayOf(0xf8.toByte(), 0x07, 0x01)
        val future = Envelope.parseFrom(futureBytes)
        assertEquals(decoded.deviceId, future.deviceId)
        assertArrayEquals(futureBytes, future.toByteArray())
    }
}
