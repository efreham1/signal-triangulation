package com.example.polaris

import android.Manifest
import android.content.pm.PackageManager
import android.location.Location
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import androidx.room.Room
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlin.math.sqrt
import kotlinx.coroutines.isActive

@Suppress("DEPRECATION")
class MeasureSourceActivity : AppCompatActivity() {

    private lateinit var startBtn: Button
    private lateinit var startLegacyBtn: Button
    private lateinit var cancelBtn: Button
    private lateinit var progressTv: TextView
    private lateinit var resultTv: TextView

    private val locationPermissionRequestCode = 1001

    private data class ReceivedSample(
        val latitude: Double,
        val longitude: Double,
        val providerTimeMs: Long,
        val providerElapsedNs: Long,
    )

    // Sample count
    private val sampleCount = 10

    // Delay between samples in ms
    private val sampleDelayMs = 1500L
    private val singleTimeoutMs = 3000L

    // DB
    private lateinit var db: AppDatabase

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_measure_source)

        startBtn = findViewById(R.id.startMeasureBtn)
        startLegacyBtn = findViewById(R.id.startMeasureBtnLegacy)
        cancelBtn = findViewById(R.id.cancelMeasureBtn)
        progressTv = findViewById(R.id.measureProgress)
        resultTv = findViewById(R.id.measureResult)

        startBtn.setOnClickListener { startMeasurements() }
        startLegacyBtn.setOnClickListener { startMeasurementsLegacy() }
        cancelBtn.setOnClickListener { finish() }
        db = Room.databaseBuilder(applicationContext, AppDatabase::class.java, "signal-db").fallbackToDestructiveMigration().build()
    }

    override fun onStart() {
        super.onStart()
        if (hasLocationPermission()) {
            LocationStream.start(this)
        } else {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION), locationPermissionRequestCode)
        }
    }

    override fun onStop() {
        super.onStop()
        LocationStream.stop()
    }

    // New strategy: Uses LocationStream's weighted average
    private fun startMeasurements() {
        if (!hasLocationPermission()) {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION), locationPermissionRequestCode)
            return
        }

        startBtn.isEnabled = false
        resultTv.text = ""
        progressTv.text = getString(R.string.measuring)

        lifecycleScope.launchWhenStarted {
            val durationMs = sampleCount * sampleDelayMs
            val startTime = System.currentTimeMillis()

            // Wait for data to accumulate in the stream buffer
            while (isActive && System.currentTimeMillis() - startTime < durationMs) {
                val remaining = ((durationMs - (System.currentTimeMillis() - startTime)) / 1000).coerceAtLeast(0)
                progressTv.text = getString(R.string.collecting_stream_samples_s, remaining)
                delay(500)
            }

            val avgLoc = LocationStream.getAveragedLocation(durationMs)

            if (avgLoc != null) {
                launch(Dispatchers.IO) { db.sourcePositionDao().upsert(SourcePosition(id = 0, latitude = avgLoc.latitude, longitude = avgLoc.longitude)) }
                
                progressTv.text = getString(R.string.measuring_done, sampleCount)
                // Note: std dev is not calculated in the stream method currently, passing 0.0
                resultTv.text = getString(R.string.measurement_result, avgLoc.latitude, avgLoc.longitude, 0.0, 0.0)
                Toast.makeText(this@MeasureSourceActivity, getString(R.string.measurement_saved), Toast.LENGTH_SHORT).show()
                setResult(RESULT_OK)
            } else {
                progressTv.text = getString(R.string.no_location_samples)
            }
            startBtn.isEnabled = true
        }
    }

    // Old strategy: Manual sampling loop (Renamed, kept for comparison)
    private fun startMeasurementsLegacy() {
        if (!hasLocationPermission()) {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION), locationPermissionRequestCode)
            return
        }

        startBtn.isEnabled = false
        resultTv.text = ""
        progressTv.text = getString(R.string.measuring)

        lifecycleScope.launchWhenStarted {
            // Collect unique samples only; retry until we have exactly sampleCount
            val samples = mutableListOf<ReceivedSample>()
            val seenFixIds = mutableSetOf<Long>() // unique by location fix timestamp

            var attempts = 0
            val maxAttempts = sampleCount * 10 // avoid infinite loop

            while (isActive && samples.size < sampleCount && attempts < maxAttempts) {
                progressTv.text = getString(R.string.measuring_progress, samples.size + 1, sampleCount)

                val loc = waitForUniqueLocation(singleTimeoutMs, seenFixIds)
                if (loc != null) {
                    val fixId = if (loc.elapsedRealtimeNanos != 0L) loc.elapsedRealtimeNanos else loc.time * 1_000_000L
                    seenFixIds.add(fixId)
                    samples.add(
                        ReceivedSample(
                            latitude = loc.latitude,
                            longitude = loc.longitude,
                            providerTimeMs = loc.time,
                            providerElapsedNs = loc.elapsedRealtimeNanos
                        )
                    )
                } else {
                    progressTv.text = getString(R.string.sample_timed_out, samples.size + 1)
                }

                attempts++
                if (samples.size < sampleCount) {
                    delay(sampleDelayMs)
                }
            }

            if (!isActive) {
                startBtn.isEnabled = true
                return@launchWhenStarted
            }

            if (samples.isEmpty()) {
                progressTv.text = getString(R.string.no_location_samples)
                startBtn.isEnabled = true
                return@launchWhenStarted
            }

            // Compute stats from unique samples only
            val meanLat = samples.map { it.latitude }.average()
            val meanLon = samples.map { it.longitude }.average()

            val latVars = samples.map { (it.latitude - meanLat) * (it.latitude - meanLat) }
            val lonVars = samples.map { (it.longitude - meanLon) * (it.longitude - meanLon) }
            val stdLat = sqrt(latVars.average())
            val stdLon = sqrt(lonVars.average())

            launch { db.sourcePositionDao().upsert(SourcePosition(id = 0, latitude = meanLat, longitude = meanLon)) }

            withContext(Dispatchers.Main) {
                progressTv.text = getString(R.string.measuring_done, samples.size)
                resultTv.text = getString(R.string.measurement_result, meanLat, meanLon, stdLat, stdLon)
                startBtn.isEnabled = true

                setResult(RESULT_OK)

                Toast.makeText(this@MeasureSourceActivity, getString(R.string.measurement_saved), Toast.LENGTH_SHORT).show()
            }
        }
    }

    private suspend fun waitForUniqueLocation(timeoutMs: Long, seen: Set<Long>): Location? {
        val start = System.currentTimeMillis()
        var lastCandidateId: Long? = null
        while (System.currentTimeMillis() - start < timeoutMs) {
            val loc = LocationStream.latest()
            if (loc != null) {
                val fixId = if (loc.elapsedRealtimeNanos != 0L) loc.elapsedRealtimeNanos else loc.time * 1_000_000L
                if (fixId != lastCandidateId) {
                    lastCandidateId = fixId
                    if (!seen.contains(fixId)) {
                        return loc
                    }
                }
            }
            delay(250L)
        }
        return null
    }

    private fun hasLocationPermission(): Boolean {
        return (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
                || ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED)
    }
}