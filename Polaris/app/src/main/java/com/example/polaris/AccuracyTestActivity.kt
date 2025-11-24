package com.example.polaris

import android.Manifest
import android.content.pm.PackageManager
import android.graphics.Typeface
import android.os.Bundle
import android.text.InputType
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.delay
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Suppress("DEPRECATION")
class AccuracyTestActivity : AppCompatActivity() {

    // Start position (in-memory for testing)
    private var startLat: Double? = null
    private var startLon: Double? = null

    private var lastPosLat: Double? = null
    private var lastPosLon: Double? = null

    private var cumulativeUserM: Double = 0.0
    private var cumulativeGpsM: Double = 0.0

    private lateinit var summaryBtn: Button

    // Sampling config
    private val startSampleCount = 10
    private val sampleIntervalMs = 1000L
    private val retryDelayMs = 250L
    private val perSampleMaxRetries = 8

    private data class MovementTestSample(
        val timestampMs: Long,
        val refLat: Double,
        val refLon: Double,
        val measLat: Double,
        val measLon: Double,
        val userDistanceM: Double,
        val gpsDistanceM: Double,
        val errorM: Double,
        val cumUserM: Double,
        val cumGpsM: Double,
        val directFromStartGpsM: Double?
    )
    private val testSamples = mutableListOf<MovementTestSample>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_accuracy_test)

        val info = findViewById<TextView>(R.id.body)
        findViewById<Button>(R.id.closeBtn).setOnClickListener { finish() }

        val samplesTv = TextView(this).apply {
            typeface = Typeface.MONOSPACE
            textSize = 12f
            setTextIsSelectable(true)
            text = getString(R.string.accuracy_samples_header)
        }

        // Button: Measure Start (averages N samples at the same spot)
        val measureStartBtn = Button(this).apply {
            text = getString(R.string.measure_start_button, startSampleCount)
            setOnClickListener {
                if (!hasLocationPerm()) {
                    Toast.makeText(this@AccuracyTestActivity, getString(R.string.location_perm_required), Toast.LENGTH_SHORT).show()
                    return@setOnClickListener
                }
                lifecycleScope.launchWhenStarted {
                    samplesTv.text = getString(R.string.accuracy_samples_header)
                    info.text = getString(R.string.measuring_start_collecting, startSampleCount)
                    val avg = measureStartPosition(info, samplesTv)
                    if (avg != null) {
                        startLat = avg.first
                        startLon = avg.second
                        lastPosLat = startLat
                        lastPosLon = startLon
                        cumulativeUserM = 0.0
                        cumulativeGpsM = 0.0
                        testSamples.clear()
                        summaryBtn.isEnabled = false

                        info.text = getString(R.string.start_set, startLat, startLon)
                        Toast.makeText(this@AccuracyTestActivity, getString(R.string.measurement_saved), Toast.LENGTH_SHORT).show()
                    } else {
                        info.text = getString(R.string.failed_collect_samples)
                        Toast.makeText(this@AccuracyTestActivity, getString(R.string.no_location_samples), Toast.LENGTH_SHORT).show()
                    }
                }
            }
        }

        // Button: Single measurement (simulates RSSI trigger)
        val triggerBtn = Button(this).apply {
            text = getString(R.string.measure_current_location)
            setOnClickListener {
                val best = LocationStream.nearestByWallClock(System.currentTimeMillis(), 7000)
                if (best == null) {
                    info.text = getString(R.string.no_recent_location_match)
                    return@setOnClickListener
                }

                // Establish reference: last position if set; else use start; else set current as reference and wait for next
                val refLat: Double
                val refLon: Double
                when {
                    lastPosLat != null && lastPosLon != null -> {
                        refLat = lastPosLat!!
                        refLon = lastPosLon!!
                    }
                    startLat != null && startLon != null -> {
                        refLat = startLat!!
                        refLon = startLon!!
                    }
                    else -> {
                        lastPosLat = best.latitude
                        lastPosLon = best.longitude
                        info.text = getString(R.string.reference_set_take_another)
                        return@setOnClickListener
                    }
                }

                // Ask the user for the distance moved (meters)
                promptDistanceMeters { userMeters ->
                    val gpsMeters = distanceMeters(refLat, refLon, best.latitude, best.longitude)
                    val error = gpsMeters - userMeters

                    cumulativeUserM += userMeters
                    cumulativeGpsM += gpsMeters

                    val directFromStartGpsM = if (startLat != null && startLon != null)
                        distanceMeters(startLat!!, startLon!!, best.latitude, best.longitude)
                    else null

                    val sb = StringBuilder()
                        .appendLine(getString(R.string.user_distance_line, userMeters))
                        .appendLine(getString(R.string.gps_distance_line, gpsMeters))
                        .appendLine(getString(R.string.error_line, error))
                        .appendLine(getString(R.string.from_line, refLat, refLon))
                        .appendLine(getString(R.string.to_line, best.latitude, best.longitude))
                        .appendLine(getString(R.string.cumulative_user_line, cumulativeUserM))
                        .append(getString(R.string.cumulative_gps_line, cumulativeGpsM))
                    if (directFromStartGpsM != null) {
                        sb.appendLine().append(getString(R.string.direct_from_start_line, directFromStartGpsM))
                    }
                    info.text = sb.toString()

                    // Advance the last position to the current fix for the next run
                    lastPosLat = best.latitude
                    lastPosLon = best.longitude

                    testSamples += MovementTestSample(
                        timestampMs = System.currentTimeMillis(),
                        refLat = refLat,
                        refLon = refLon,
                        measLat = best.latitude,
                        measLon = best.longitude,
                        userDistanceM = userMeters,
                        gpsDistanceM = gpsMeters,
                        errorM = error,
                        cumUserM = cumulativeUserM,
                        cumGpsM = cumulativeGpsM,
                        directFromStartGpsM = directFromStartGpsM
                    )
                    summaryBtn.isEnabled = testSamples.isNotEmpty()
                }
            }
        }

        summaryBtn = Button(this).apply {
            text = getString(R.string.show_summary)
            isEnabled = false
            setOnClickListener {
                val summary = buildSummary()
                info.text = summary
            }
        }

        val root = findViewById<LinearLayout>(R.id.accuracyTestRoot)
        root.addView(measureStartBtn)
        root.addView(triggerBtn)
        root.addView(summaryBtn)
        root.addView(samplesTv)
    }

    private fun hasLocationPerm(): Boolean {
        return ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED ||
               ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED
    }

    private suspend fun measureStartPosition(info: TextView, samplesTv: TextView): Pair<Double, Double>? {
        val seenFixTimesNs = mutableSetOf<Long>() // enforce uniqueness by monotonic clock
        val sdf = SimpleDateFormat("HH:mm:ss", Locale.getDefault())
        val startCollectionTime = System.currentTimeMillis()

        var collected = 0
        while (collected < startSampleCount) {
            var uniqueLoc: android.location.Location? = null
            var tries = 0

            // keep trying until we get a unique fix (by elapsedRealtimeNanos) or we exhaust retries
            while (tries < perSampleMaxRetries) {
                val loc = LocationStream.latest()
                if (loc != null) {
                    val tNs = loc.elapsedRealtimeNanos
                    if (tNs !in seenFixTimesNs) {
                        uniqueLoc = loc
                        break
                    }
                }
                tries++
                delay(retryDelayMs)
            }

            if (uniqueLoc != null) {
                seenFixTimesNs.add(uniqueLoc.elapsedRealtimeNanos)
                collected++

                val accStr = if (uniqueLoc.hasAccuracy()) "%s".format(Locale.getDefault(), "%.1f".format(uniqueLoc.accuracy)) else "n/a"
                val tStr = sdf.format(Date(uniqueLoc.time))
                samplesTv.append(
                    getString(
                        R.string.sample_line,
                        collected,
                        uniqueLoc.latitude,
                        uniqueLoc.longitude,
                        accStr,
                        tStr
                    )
                )
                samplesTv.append("\n")
                info.text = getString(R.string.measuring_collected, collected, startSampleCount)

                // wait 1s before attempting the next unique sample
                delay(sampleIntervalMs)
            } else {
                // we couldn't obtain a unique fix within retries; keep looping until we do
                info.text = getString(R.string.waiting_unique_fix)
            }
        }

        val durationMs = System.currentTimeMillis() - startCollectionTime
        // Use LocationStream's weighted averaging. Add a small buffer to ensure we cover the start time.
        val avgLoc = LocationStream.getAveragedLocation(durationMs + 1000)

        if (avgLoc == null) return null

        samplesTv.append(getString(R.string.mean_line, avgLoc.latitude, avgLoc.longitude))
        return avgLoc.latitude to avgLoc.longitude
    }

    override fun onResume() {
        super.onResume()
        val ok = hasLocationPerm()
        if (ok) LocationStream.start(this)
    }

    override fun onPause() {
        super.onPause()
        LocationStream.stop()
    }

    private fun promptDistanceMeters(onSubmit: (Double) -> Unit) {
        val input = EditText(this).apply {
            inputType = InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_DECIMAL
            hint = getString(R.string.distance_moved_hint)
        }
        AlertDialog.Builder(this)
            .setTitle(getString(R.string.distance_moved_title))
            .setView(input)
            .setPositiveButton(android.R.string.ok) { _, _ ->
                val meters = input.text.toString().replace(',', '.').toDoubleOrNull()
                if (meters == null) {
                    Toast.makeText(this, getString(R.string.invalid_number), Toast.LENGTH_SHORT).show()
                } else onSubmit(meters)
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    // Haversine distance in meters
    private fun distanceMeters(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
        val radius = 6362475.0
        val dLat = Math.toRadians(lat2 - lat1)
        val dLon = Math.toRadians(lon2 - lon1)
        val a = kotlin.math.sin(dLat / 2) * kotlin.math.sin(dLat / 2) +
                kotlin.math.cos(Math.toRadians(lat1)) * kotlin.math.cos(Math.toRadians(lat2)) *
                kotlin.math.sin(dLon / 2) * kotlin.math.sin(dLon / 2)
        val c = 2 * kotlin.math.atan2(kotlin.math.sqrt(a), kotlin.math.sqrt(1 - a))
        return radius * c
    }

    private fun buildSummary(): String {
        if (testSamples.isEmpty()) return getString(R.string.summary_no_samples)
        val n = testSamples.size

        val meanUser = testSamples.map { it.userDistanceM }.average()
        val meanGps = testSamples.map { it.gpsDistanceM }.average()
        val meanErr = testSamples.map { it.errorM }.average()
        val meanAbsErr = testSamples.map { kotlin.math.abs(it.errorM) }.average()
        val rmse = kotlin.math.sqrt(testSamples.map { it.errorM * it.errorM }.average())
        val maxErr = testSamples.maxByOrNull { kotlin.math.abs(it.errorM) }!!
        val finalCumUser = testSamples.last().cumUserM
        val finalCumGps = testSamples.last().cumGpsM
        val finalCumErr = finalCumGps - finalCumUser
        val directFinal = testSamples.last().directFromStartGpsM

        val sb = StringBuilder()
        sb.appendLine(getString(R.string.summary_header, n))
        sb.appendLine(getString(R.string.summary_per_step_header))
        sb.appendLine(getString(R.string.summary_mean_user, meanUser))
        sb.appendLine(getString(R.string.summary_mean_gps, meanGps))
        sb.appendLine(getString(R.string.summary_mean_err, meanErr))
        sb.appendLine(getString(R.string.summary_mean_abs_err, meanAbsErr))
        sb.appendLine(getString(R.string.summary_rmse, rmse))
        sb.appendLine(getString(R.string.summary_max_abs_err, kotlin.math.abs(maxErr.errorM)))
        sb.appendLine()
        sb.appendLine(getString(R.string.summary_cum_header))
        sb.appendLine(getString(R.string.summary_cum_user, finalCumUser))
        sb.appendLine(getString(R.string.summary_cum_gps, finalCumGps))
        sb.appendLine(getString(R.string.summary_cum_err, finalCumErr))
        if (directFinal != null) {
            sb.appendLine(getString(R.string.summary_direct_from_start, directFinal))
        }
        sb.appendLine()
        sb.appendLine(getString(R.string.summary_samples_header))
        testSamples.forEachIndexed { idx, s ->
            val directSuffix = s.directFromStartGpsM?.let { getString(R.string.summary_direct_suffix, it) } ?: ""
            sb.appendLine(
                getString(
                    R.string.summary_sample_line,
                    idx + 1,
                    s.userDistanceM,
                    s.gpsDistanceM,
                    s.errorM,
                    s.cumUserM,
                    s.cumGpsM,
                    directSuffix,
                    s.refLat,
                    s.refLon,
                    s.measLat,
                    s.measLon
                )
            )
        }
        return sb.toString()
    }
}