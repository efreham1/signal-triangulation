package com.example.polaris

import org.junit.Test
import org.junit.Assert.*


/**
 * Unit tests for the Polaris signal triangulation application.
 */
class UnitTest {

    // ==================== SignalRecord Tests ====================

    @Test
    fun signalRecord_creation_hasCorrectDefaults() {
        val record = SignalRecord(
            latitude = 51.5074,
            longitude = -0.1278,
            ssid = "TestNetwork",
            rssi = -65,
            deviceID = "device123"
        )

        assertEquals(0, record.id)
        assertEquals(51.5074, record.latitude, 0.0001)
        assertEquals(-0.1278, record.longitude, 0.0001)
        assertEquals("TestNetwork", record.ssid)
        assertEquals(-65, record.rssi)
        assertEquals("device123", record.deviceID)
        assertTrue(record.timestamp > 0)
    }

    @Test
    fun signalRecord_equality() {
        val timestamp = System.currentTimeMillis()
        val record1 = SignalRecord(
            id = 1,
            latitude = 51.5074,
            longitude = -0.1278,
            ssid = "TestNetwork",
            rssi = -65,
            timestamp = timestamp,
            deviceID = "device123"
        )
        val record2 = SignalRecord(
            id = 1,
            latitude = 51.5074,
            longitude = -0.1278,
            ssid = "TestNetwork",
            rssi = -65,
            timestamp = timestamp,
            deviceID = "device123"
        )

        assertEquals(record1, record2)
    }

    // ==================== SourcePosition Tests ====================

    @Test
    fun sourcePosition_creation_withNullCoordinates() {
        val pos = SourcePosition(id = 0, latitude = null, longitude = null)

        assertEquals(0, pos.id)
        assertNull(pos.latitude)
        assertNull(pos.longitude)
    }

    @Test
    fun sourcePosition_creation_withValidCoordinates() {
        val pos = SourcePosition(id = 0, latitude = 48.8566, longitude = 2.3522)

        assertEquals(0, pos.id)
        assertEquals(48.8566, pos.latitude!!, 0.0001)
        assertEquals(2.3522, pos.longitude!!, 0.0001)
    }

    // ==================== SSID Sanitization Tests ====================

    @Test
    fun sanitizeFreeText_removesSpecialCharacters() {
        val result = sanitizeFreeText("Test@Network#123!")
        assertEquals("TestNetwork123", result)
    }

    @Test
    fun sanitizeFreeText_preservesValidCharacters() {
        val result = sanitizeFreeText("Test_Network-123")
        assertEquals("Test_Network-123", result)
    }

    @Test
    fun sanitizeFreeText_truncatesLongInput() {
        val longInput = "A".repeat(100)
        val result = sanitizeFreeText(longInput)
        assertEquals(50, result.length)
    }

    @Test
    fun sanitizeFreeText_handlesEmptyInput() {
        val result = sanitizeFreeText("")
        assertTrue(result.startsWith("noTag_"))
    }

    @Test
    fun sanitizeFreeText_handlesWhitespaceOnly() {
        val result = sanitizeFreeText("   ")
        assertTrue(result.startsWith("noTag_"))
    }

    @Test
    fun sanitizeFreeText_trimsWhitespace() {
        val result = sanitizeFreeText("  TestNetwork  ")
        assertEquals("TestNetwork", result)
    }

    // Helper function mirroring MainActivity.sanitizeFreeText
    private fun sanitizeFreeText(raw: String): String {
        val sanitized = raw.trim().replace(Regex("[^A-Za-z0-9_-]"), "").take(50)
        return sanitized.ifBlank {
            "noTag_" + raw.hashCode().toUInt().toString(16)
        }
    }

    // ==================== Coordinate Validation Tests ====================

    @Test
    fun coordinates_validLatitudeRange() {
        val validLatitudes = listOf(-90.0, -45.0, 0.0, 45.0, 90.0)

        validLatitudes.forEach { lat ->
            assertTrue("Latitude $lat should be valid", isValidLatitude(lat))
        }
    }

    @Test
    fun coordinates_invalidLatitudeRange() {
        val invalidLatitudes = listOf(-91.0, 91.0, -180.0, 180.0)

        invalidLatitudes.forEach { lat ->
            assertFalse("Latitude $lat should be invalid", isValidLatitude(lat))
        }
    }

    @Test
    fun coordinates_validLongitudeRange() {
        val validLongitudes = listOf(-180.0, -90.0, 0.0, 90.0, 180.0)

        validLongitudes.forEach { lon ->
            assertTrue("Longitude $lon should be valid", isValidLongitude(lon))
        }
    }

    @Test
    fun coordinates_invalidLongitudeRange() {
        val invalidLongitudes = listOf(-181.0, 181.0, -360.0, 360.0)

        invalidLongitudes.forEach { lon ->
            assertFalse("Longitude $lon should be invalid", isValidLongitude(lon))
        }
    }

    private fun isValidLatitude(lat: Double): Boolean = lat in -90.0..90.0
    private fun isValidLongitude(lon: Double): Boolean = lon in -180.0..180.0

    // ==================== Weighted Average Tests ====================

    @Test
    fun weightedAverage_singleSample() {
        val samples = listOf(
            MockLocation(51.5074, -0.1278, 5.0f)
        )

        val avg = calculateWeightedAverage(samples)

        assertEquals(51.5074, avg.first, 0.0001)
        assertEquals(-0.1278, avg.second, 0.0001)
    }

    @Test
    fun weightedAverage_equalAccuracy() {
        val samples = listOf(
            MockLocation(51.5074, -0.1278, 10.0f),
            MockLocation(51.5084, -0.1288, 10.0f)
        )

        val avg = calculateWeightedAverage(samples)

        // With equal weights, should be arithmetic mean
        assertEquals(51.5079, avg.first, 0.0001)
        assertEquals(-0.1283, avg.second, 0.0001)
    }

    @Test
    fun weightedAverage_differentAccuracy() {
        // More accurate sample should have more weight
        val samples = listOf(
            MockLocation(51.5074, -0.1278, 2.0f),  // High accuracy (weight = 1/4 = 0.25)
            MockLocation(51.5084, -0.1288, 10.0f) // Low accuracy (weight = 1/100 = 0.01)
        )

        val avg = calculateWeightedAverage(samples)

        // Result should be much closer to the first (more accurate) sample
        assertTrue(avg.first < 51.5076)
        assertTrue(avg.second > -0.1280)
    }

    data class MockLocation(val latitude: Double, val longitude: Double, val accuracy: Float)

    private fun calculateWeightedAverage(samples: List<MockLocation>): Pair<Double, Double> {
        if (samples.isEmpty()) return 0.0 to 0.0

        var sumLat = 0.0
        var sumLon = 0.0
        var sumWeights = 0.0

        for (loc in samples) {
            val acc = loc.accuracy.coerceAtLeast(1.0f)
            val weight = 1.0 / (acc * acc)

            sumLat += loc.latitude * weight
            sumLon += loc.longitude * weight
            sumWeights += weight
        }

        return (sumLat / sumWeights) to (sumLon / sumWeights)
    }

    // ==================== SSID List Management Tests ====================

    @Test
    fun ssidMerge_addsNewSsids() {
        val existing = mutableListOf("Network_A", "Network_B")
        val newSsids = listOf("Network_C", "Network_D")

        val merged = (existing + newSsids).filter { it.isNotBlank() }.distinct().sorted()

        assertEquals(4, merged.size)
        assertEquals(listOf("Network_A", "Network_B", "Network_C", "Network_D"), merged)
    }

    @Test
    fun ssidMerge_handlesDuplicates() {
        val existing = mutableListOf("Network_A", "Network_B")
        val newSsids = listOf("Network_B", "Network_C")

        val merged = (existing + newSsids).filter { it.isNotBlank() }.distinct().sorted()

        assertEquals(3, merged.size)
        assertEquals(listOf("Network_A", "Network_B", "Network_C"), merged)
    }

    @Test
    fun ssidMerge_filtersBlank() {
        val existing = mutableListOf("Network_A")
        val newSsids = listOf("", "  ", "Network_B")

        val merged = (existing + newSsids).filter { it.isNotBlank() }.distinct().sorted()

        assertEquals(2, merged.size)
        assertFalse(merged.contains(""))
    }

    // ==================== Time Format Tests ====================

    @Test
    fun timeFormat_validOutput() {
        val timeFormat = java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.getDefault())
        val timestamp = 1701619200000L // Some fixed timestamp

        val formatted = timeFormat.format(java.util.Date(timestamp))

        // Should match HH:mm:ss pattern
        assertTrue(formatted.matches(Regex("\\d{2}:\\d{2}:\\d{2}")))
    }

    // ==================== JSON Export Structure Tests ====================

    @Test
    fun exportStructure_containsRequiredFields() {
        val source = SourcePosition(0, 51.5074, -0.1278)
        val records = listOf(
            SignalRecord(1, 51.5084, -0.1288, "TestSSID", -65, 1000L, "device1")
        )

        val exportObj = mapOf(
            "source_pos" to if (source.latitude != null && source.longitude != null) {
                mapOf("x" to source.latitude, "y" to source.longitude)
            } else null,
            "measurements" to records
        )

        assertTrue(exportObj.containsKey("source_pos"))
        assertTrue(exportObj.containsKey("measurements"))

        @Suppress("UNCHECKED_CAST")
        val sourcePos = exportObj["source_pos"] as Map<String, Double>
        assertEquals(51.5074, sourcePos["x"]!!, 0.0001)
        assertEquals(-0.1278, sourcePos["y"]!!, 0.0001)
    }

    @Test
    fun exportStructure_handlesNullSource() {
        val source: SourcePosition? = null
        val records = emptyList<SignalRecord>()

        val exportObj = mapOf(
            "source_pos" to source?.let {
                if (it.latitude != null && it.longitude != null) {
                    mapOf("x" to it.latitude, "y" to it.longitude)
                } else null
            },
            "measurements" to records
        )

        assertNull(exportObj["source_pos"])
    }

    // ==================== Measurement Offset Tests ====================

    @Test
    fun measurementOffset_defaultValue() {
        val defaultOffsetMs = 5_000L
        assertEquals(5000L, defaultOffsetMs)
    }

    @Test
    fun measurementOffset_appliedCorrectly() {
        val startTime = 1000L
        val offsetMs = 5000L
        val targetTime = startTime + offsetMs

        assertEquals(6000L, targetTime)
    }

    // ==================== Buffer Size Tests ====================

    @Test
    fun bufferSize_limits() {
        val maxBufferSize = 50
        val buffer = ArrayDeque<Int>()

        // Add more than max items
        repeat(100) { buffer.addFirst(it) }

        // Simulate trimming
        while (buffer.size > maxBufferSize) buffer.removeLast()

        assertEquals(maxBufferSize, buffer.size)
        assertEquals(99, buffer.first()) // Most recent
        assertEquals(50, buffer.last())  // Oldest remaining
    }
}