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
        cancelBtn = findViewById(R.id.cancelMeasureBtn)
        progressTv = findViewById(R.id.measureProgress)
        resultTv = findViewById(R.id.measureResult)

        startBtn.setOnClickListener { startMeasurements() }
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

            val result = LocationStream.getAveragedLocationAndSamples(durationMs)
            
            if (result == null) {
                progressTv.text = getString(R.string.no_location_samples)
            } else {
                val avgLoc = result.first
                val samples = result.second

                launch(Dispatchers.IO) { db.sourcePositionDao().upsert(SourcePosition(id = 0, latitude = avgLoc.latitude, longitude = avgLoc.longitude)) }

                progressTv.text = getString(R.string.measuring_done, samples.size)
                resultTv.text = getString(R.string.measurement_result, avgLoc.latitude, avgLoc.longitude)
                Toast.makeText(this@MeasureSourceActivity, getString(R.string.measurement_saved), Toast.LENGTH_SHORT).show()
                setResult(RESULT_OK)
            }

            startBtn.isEnabled = true
        }
    }

    private fun hasLocationPermission(): Boolean {
        return (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
                || ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED)
    }
}