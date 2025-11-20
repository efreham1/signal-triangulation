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
import com.google.android.gms.location.LocationServices
import com.google.android.gms.location.Priority
import androidx.lifecycle.lifecycleScope
import androidx.room.Room
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlin.coroutines.resume
import kotlin.math.sqrt
import kotlinx.coroutines.isActive

@Suppress("DEPRECATION")
class MeasureSourceActivity : AppCompatActivity() {

    private lateinit var startBtn: Button
    private lateinit var cancelBtn: Button
    private lateinit var progressTv: TextView
    private lateinit var resultTv: TextView

    private val fusedLocationClient by lazy { LocationServices.getFusedLocationProviderClient(this) }

    private val locationPermissionRequestCode = 1001
    private val requiredPermissions = arrayOf(
        Manifest.permission.ACCESS_FINE_LOCATION,
        Manifest.permission.ACCESS_COARSE_LOCATION,
        Manifest.permission.ACCESS_WIFI_STATE
    )

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

    private fun startMeasurements() {
        if (!hasLocationPermission()) {
            ActivityCompat.requestPermissions(
                this,
                arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION),
                locationPermissionRequestCode
            )
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

    private suspend fun getCurrentLocationSuspend(timeoutMs: Long = 3000L): Location? {
        return withTimeoutOrNull(timeoutMs) {
            suspendCancellableCoroutine { cont ->
                if (!hasLocationPermission()) {
                    cont.resume(null)
                    return@suspendCancellableCoroutine
                }

                if (ContextCompat.checkSelfPermission(this@MeasureSourceActivity, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED &&
                    ContextCompat.checkSelfPermission(this@MeasureSourceActivity, Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                    ActivityCompat.requestPermissions(
                        this@MeasureSourceActivity,
                        requiredPermissions,
                        locationPermissionRequestCode
                    )
                    cont.resume(null)
                    return@suspendCancellableCoroutine
                }
                val task = fusedLocationClient.getCurrentLocation(Priority.PRIORITY_HIGH_ACCURACY, null)
                task.addOnSuccessListener { loc -> if (!cont.isCompleted) cont.resume(loc) }
                task.addOnFailureListener { _ -> if (!cont.isCompleted) cont.resume(null) }
                cont.invokeOnCancellation { }
            }
        }
    }

    private fun hasLocationPermission(): Boolean {
        return (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
                || ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED)
    }
}