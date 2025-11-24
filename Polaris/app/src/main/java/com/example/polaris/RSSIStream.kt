package com.example.polaris

import android.Manifest
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.net.wifi.ScanResult
import android.net.wifi.WifiManager
import androidx.core.content.ContextCompat
import kotlinx.coroutines.*
import kotlin.math.abs

object RSSIStream {
    data class ScanBatch(val timestamp: Long, val results: List<ScanResult>)

    private val buffer = ArrayDeque<ScanBatch>()
    private const val MAX_BUFFER_SIZE = 50 // Keep last 50 scans
    private var wifiManager: WifiManager? = null
    private var scanReceiver: BroadcastReceiver? = null
    private var started = false
    private var context: Context? = null

    // Scanning loop
    private var scanJob: Job? = null
    private val scope = CoroutineScope(Dispatchers.Main)
    private const val SCAN_INTERVAL_MS = 10000L // 10s throttle

    // Listeners for new data
    private val listeners = mutableListOf<(ScanBatch) -> Unit>()

    fun start(ctx: Context) {
        if (started) return
        
        val hasPerm = ContextCompat.checkSelfPermission(ctx, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED ||
                      ContextCompat.checkSelfPermission(ctx, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED
        if (!hasPerm) return

        context = ctx.applicationContext
        wifiManager = context?.getSystemService(Context.WIFI_SERVICE) as? WifiManager

        scanReceiver = object : BroadcastReceiver() {
            override fun onReceive(c: Context, intent: Intent) {
                val success = intent.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false)
                if (success) {
                    handleScanResults()
                }
            }
        }

        val filter = IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION)
        context?.registerReceiver(scanReceiver, filter)
        started = true

        startScanningLoop()
    }

    fun stop() {
        if (!started) return
        scanJob?.cancel()
        try {
            context?.unregisterReceiver(scanReceiver)
        } catch (e: Exception) {
            // ignore
        }
        scanReceiver = null
        wifiManager = null
        context = null
        started = false
        synchronized(buffer) { buffer.clear() }
    }

    fun addListener(listener: (ScanBatch) -> Unit) {
        synchronized(listeners) { listeners.add(listener) }
    }

    fun removeListener(listener: (ScanBatch) -> Unit) {
        synchronized(listeners) { listeners.remove(listener) }
    }

    private fun startScanningLoop() {
        scanJob?.cancel()
        scanJob = scope.launch {
            while (isActive && started) {
                @Suppress("DEPRECATION")
                wifiManager?.startScan()
                delay(SCAN_INTERVAL_MS)
            }
        }
    }

    private fun handleScanResults() {
        val mgr = wifiManager ?: return
        try {
            val results = mgr.scanResults ?: return
            val now = System.currentTimeMillis()
            val batch = ScanBatch(now, results)

            synchronized(buffer) {
                buffer.addFirst(batch)
                while (buffer.size > MAX_BUFFER_SIZE) buffer.removeLast()
            }

            synchronized(listeners) {
                listeners.forEach { it(batch) }
            }
        } catch (e: SecurityException) {
            // Permission might have been revoked
        }
    }

    fun latest(): ScanBatch? = synchronized(buffer) { buffer.firstOrNull() }

    fun nearestByWallClock(targetTimeMs: Long, maxDeltaMs: Long = 5000L): ScanBatch? =
        synchronized(buffer) {
            val best = buffer.minByOrNull { abs(it.timestamp - targetTimeMs) } ?: return null
            val delta = abs(best.timestamp - targetTimeMs)
            if (delta <= maxDeltaMs) best else null
        }
}