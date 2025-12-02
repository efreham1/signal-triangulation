package com.example.polaris

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.location.Location
import androidx.core.content.ContextCompat
import com.google.android.gms.location.*
import kotlin.math.abs
import kotlin.math.sqrt

object LocationStream {
    private val buffer = ArrayDeque<Location>()
    private const val MAX_BUFFER_SIZE = 180 // ~3 minutes @1Hz
    private var fused: FusedLocationProviderClient? = null
    private var callback: LocationCallback? = null
    private var started = false

    // Accuracy improvements
    private var startTimeMs = 0L
    private const val WARMUP_MS = 2000L // Ignore first 2 seconds of GPS data
    private const val MIN_ACCURACY_THRESHOLD = 20f // Ignore points worse than 20m off

    fun start(context: Context) {
        if (started) return
        val hasPerm =
            ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED ||
            ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED
        if (!hasPerm) return

        startTimeMs = System.currentTimeMillis()

        val appCtx = context.applicationContext
        val client = LocationServices.getFusedLocationProviderClient(appCtx)
        // Increased frequency: 500ms interval
        val req = LocationRequest.Builder(Priority.PRIORITY_HIGH_ACCURACY, 500L)
            .setMinUpdateIntervalMillis(100L)
            .build()

        val cb = object : LocationCallback() {
            override fun onLocationResult(result: LocationResult) {
                // 1. Warm-up: discard initial unstable fixes
                if (System.currentTimeMillis() - startTimeMs < WARMUP_MS) return

                val loc = result.lastLocation ?: return

                // 2. Filter: discard very poor accuracy fixes
                if (loc.hasAccuracy() && loc.accuracy > MIN_ACCURACY_THRESHOLD) return

                synchronized(buffer) {
                    buffer.addFirst(loc)
                    while (buffer.size > MAX_BUFFER_SIZE) buffer.removeLast()
                }
            }
        }

        client.requestLocationUpdates(req, cb, appCtx.mainLooper)
        fused = client
        callback = cb
        started = true
    }

    fun stop() {
        fused?.let { c -> callback?.let { cb -> c.removeLocationUpdates(cb) } }
        callback = null
        fused = null
        started = false
    }

    fun isStarted(): Boolean = started

    // Currently unused
    fun latest(): Location? = synchronized(buffer) { buffer.firstOrNull() }

    // Currently unused
    fun nearestByWallClock(targetTimeMs: Long, maxDeltaMs: Long = 7000L): Location? =
        synchronized(buffer) {
            val best = buffer.minByOrNull { abs(it.time - targetTimeMs) } ?: return null
            val delta = abs(best.time - targetTimeMs)
            if (delta <= maxDeltaMs) best else null
        }

    /**
     * Calculates a weighted average location from samples collected in the last [durationMs].
     * Weights are inversely proportional to the square of the accuracy (1/acc^2).
     */
    // Currently unused
    fun getAveragedLocation(durationMs: Long): Location? {
        return getAveragedLocationAndSamples(durationMs)?.first
    }

    /**
     * Calculates a weighted average location from samples collected in the last [durationMs].
     * Returns the average location and the list of samples used.
     */
    fun getAveragedLocationAndSamples(durationMs: Long): Pair<Location, List<Location>>? {
        return getAveragedLocationAndSamplesFrom(System.currentTimeMillis(), durationMs)
    }

    /**
     * Calculates a weighted average location from samples looking back [durationMs] from [fromTimeMs].
     * Returns the average location and the list of samples used.
     */
    fun getAveragedLocationAndSamplesFrom(fromTimeMs: Long, durationMs: Long): Pair<Location, List<Location>>? {
        val samples = synchronized(buffer) {
            buffer.filter { it.time <= fromTimeMs && (fromTimeMs - it.time) <= durationMs }
        }

        if (samples.isEmpty()) return null

        var sumLat = 0.0
        var sumLon = 0.0
        var sumWeights = 0.0
        var bestAcc = Float.MAX_VALUE

        for (loc in samples) {
            // Use 1.0m as floor for accuracy to prevent infinite weights
            val acc = if (loc.hasAccuracy()) loc.accuracy.coerceAtLeast(1.0f) else 20.0f
            val weight = 1.0 / (acc * acc)

            sumLat += loc.latitude * weight
            sumLon += loc.longitude * weight
            sumWeights += weight
            
            if (acc < bestAcc) bestAcc = acc
        }

        if (sumWeights == 0.0) return samples.first() to samples

        val avgLat = sumLat / sumWeights
        val avgLon = sumLon / sumWeights

        val result = Location("weighted_avg")
        result.latitude = avgLat
        result.longitude = avgLon
        result.time = fromTimeMs
        // Estimate resulting accuracy (heuristic: best sample accuracy / sqrt(N))
        result.accuracy = (bestAcc / sqrt(samples.size.toFloat()))

        return result to samples
    }
}