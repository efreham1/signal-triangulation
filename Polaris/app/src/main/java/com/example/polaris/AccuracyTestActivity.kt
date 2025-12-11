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
import kotlinx.coroutines.withContext
import kotlinx.coroutines.Dispatchers

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
    private val sampleDurationMs = 10000L

    private data class MovementTestSample(
        val timestampMs: Long,
        val refLat: Double,
        val refLon: Double,
        val measLat: Double,
        val measLon: Double,
        val measAcc: Float?,
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

        val samplesTv = TextView(this).apply {
            typeface = Typeface.MONOSPACE
            textSize = 12f
            setTextIsSelectable(true)
            text = getString(R.string.accuracy_samples_header)
        }

        // Button: Measure Start (averages N samples at the same spot)
        val measureStartBtn = Button(this).apply {
            val durationSeconds = sampleDurationMs / 1000
            text = getString(R.string.measure_start_button, durationSeconds)
            setOnClickListener {
                if (!hasLocationPerm()) {
                    Toast.makeText(this@AccuracyTestActivity, getString(R.string.location_perm_required), Toast.LENGTH_SHORT).show()
                    return@setOnClickListener
                }
                lifecycleScope.launchWhenStarted {
                    samplesTv.text = getString(R.string.accuracy_samples_header)
                    info.text = getString(R.string.measuring_start_collecting, durationSeconds)
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
                isEnabled = false // Disable to prevent double-clicks during wait
                lifecycleScope.launchWhenStarted {
                    // Wait for 7 seconds to collect samples
                    val waitMs = 7000L
                    val startTime = System.currentTimeMillis()
                    
                    while (System.currentTimeMillis() - startTime < waitMs) {
                        val remaining = ((waitMs - (System.currentTimeMillis() - startTime)) / 1000).coerceAtLeast(0)
                        info.text = getString(R.string.collecting_stream_samples_s, remaining)
                        delay(250)
                    }

                    val now = System.currentTimeMillis()
                    
                    val avgPair = withContext(Dispatchers.Default) {
                        LocationStream.getAveragedLocationAndSamplesFrom(now, waitMs)
                    }

                    if (avgPair == null) {
                        info.text = getString(R.string.no_recent_location_match)
                        isEnabled = true
                        return@launchWhenStarted
                    }
                    
                    val best = avgPair.first

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
                            isEnabled = true
                            return@launchWhenStarted
                        }
                    }

                    // Ask the user for the distance moved (meters)
                    promptDistanceMeters(
                        onCancel = { isEnabled = true },
                        onSubmit = { userMeters ->
                            val gpsMeters = distanceMeters(refLat, refLon, best.latitude, best.longitude)
                            val error = gpsMeters - userMeters

                            cumulativeUserM += userMeters
                            cumulativeGpsM += gpsMeters

                            val directFromStartGpsM = if (startLat != null && startLon != null)
                                distanceMeters(startLat!!, startLon!!, best.latitude, best.longitude)
                            else null

                            val accStr = if (best.hasAccuracy()) "%.1f".format(best.accuracy) else "n/a"

                            val sb = StringBuilder()
                                .appendLine(getString(R.string.user_distance_line, userMeters))
                                .appendLine(getString(R.string.gps_distance_line, gpsMeters))
                                .appendLine(getString(R.string.error_line, error))
                                .appendLine("Accuracy: $accStr m")
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
                                measAcc = if (best.hasAccuracy()) best.accuracy else null,
                                userDistanceM = userMeters,
                                gpsDistanceM = gpsMeters,
                                errorM = error,
                                cumUserM = cumulativeUserM,
                                cumGpsM = cumulativeGpsM,
                                directFromStartGpsM = directFromStartGpsM
                            )
                            summaryBtn.isEnabled = testSamples.isNotEmpty()
                            isEnabled = true
                        }
                    )
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
        findViewById<Button>(R.id.closeBtn).setOnClickListener { finish() }
    }

    private fun hasLocationPerm(): Boolean {
        return ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED ||
               ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED
    }

    private suspend fun measureStartPosition(info: TextView, samplesTv: TextView): Pair<Double, Double>? {
        val startTime = System.currentTimeMillis()

        // Wait for data to accumulate in the stream buffer
        while (System.currentTimeMillis() - startTime < sampleDurationMs) {
            val remaining = ((sampleDurationMs - (System.currentTimeMillis() - startTime)) / 1000).coerceAtLeast(0)
            info.text = getString(R.string.collecting_stream_samples_s, remaining)
            delay(250)
        }

        // Use LocationStream's weighted averaging.
        val result = LocationStream.getAveragedLocationAndSamples(sampleDurationMs) ?: return null
        val avgLoc = result.first
        val samples = result.second

        samplesTv.text = getString(R.string.accuracy_samples_header)
        val dateFormat = java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.getDefault())

        samples.forEachIndexed { i, loc ->
            val accStr = if (loc.hasAccuracy()) "%.1f".format(loc.accuracy) else "n/a"
            val timeStr = dateFormat.format(java.util.Date(loc.time))
            samplesTv.append(getString(R.string.sample_line, i + 1, loc.latitude, loc.longitude, accStr, timeStr))
            samplesTv.append("\n")
        }

        val accStr = if (avgLoc.hasAccuracy()) "%.1f".format(avgLoc.accuracy) else "n/a"
        samplesTv.append(getString(R.string.mean_line, avgLoc.latitude, avgLoc.longitude))
        samplesTv.append(" (Acc: ${accStr}m)")

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

    private fun promptDistanceMeters(onCancel: () -> Unit = {}, onSubmit: (Double) -> Unit) {
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
                    onCancel()
                } else onSubmit(meters)
            }
            .setNegativeButton(android.R.string.cancel) { _, _ -> onCancel() }
            .setOnCancelListener { onCancel() }
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
            val accStr = s.measAcc?.let { "%.1f".format(it) } ?: "n/a"
            sb.append(
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
            sb.append(" (Acc: ${accStr}m)")
            sb.appendLine()
        }
        return sb.toString()
    }
}