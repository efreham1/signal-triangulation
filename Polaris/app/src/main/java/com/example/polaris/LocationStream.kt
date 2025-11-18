package com.example.polaris

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.location.Location
import androidx.core.content.ContextCompat
import com.google.android.gms.location.*
import kotlin.math.abs

object LocationStream {
    private val buffer = ArrayDeque<Location>()
    private const val MAX_BUFFER_SIZE = 180 // ~3 minutes @1Hz
    private var fused: FusedLocationProviderClient? = null
    private var callback: LocationCallback? = null
    private var started = false

    fun start(context: Context) {
        if (started) return
        val hasPerm =
            ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED ||
            ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED
        if (!hasPerm) return

        val appCtx = context.applicationContext
        val client = LocationServices.getFusedLocationProviderClient(appCtx)
        val req = LocationRequest.Builder(Priority.PRIORITY_HIGH_ACCURACY, 1000L)
            .setMinUpdateIntervalMillis(250L)
            .build()

        val cb = object : LocationCallback() {
            override fun onLocationResult(result: LocationResult) {
                val loc = result.lastLocation ?: return
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

    fun latest(): Location? = synchronized(buffer) { buffer.firstOrNull() }

    fun nearestByWallClock(targetTimeMs: Long, maxDeltaMs: Long = 7000L): Location? =
        synchronized(buffer) {
            val best = buffer.minByOrNull { abs(it.time - targetTimeMs) } ?: return null
            val delta = abs(best.time - targetTimeMs)
            if (delta <= maxDeltaMs) best else null
        }
}