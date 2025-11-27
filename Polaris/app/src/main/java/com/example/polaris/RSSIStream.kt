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
import java.lang.ref.WeakReference
import kotlin.coroutines.resume
import kotlin.math.abs

@Suppress("DEPRECATION")
object RSSIStream {
    data class ScanBatch(val timestamp: Long, val results: List<ScanResult>)

    private val buffer = ArrayDeque<ScanBatch>()
    private const val MAX_BUFFER_SIZE = 50 // Keep last 50 scans
    private var wifiManager: WifiManager? = null
    private var scanReceiver: BroadcastReceiver? = null
    private var started = false
    private var contextRef: WeakReference<Context>? = null

    // Scanning loop
    private var scanJob: Job? = null
    private var scope = CoroutineScope(Dispatchers.Main + SupervisorJob())
    private const val SCAN_INTERVAL_MS = 100L // request scans aggressively

    // Listeners for new data
    private val listeners = mutableListOf<(ScanBatch) -> Unit>()

    fun start(ctx: Context) {
        if (started) return
        
        if (!scope.isActive) {
            scope = CoroutineScope(Dispatchers.Main + SupervisorJob())
        }
        
        val hasPerm = ContextCompat.checkSelfPermission(ctx, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED ||
                      ContextCompat.checkSelfPermission(ctx, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED
        if (!hasPerm) return

        val appContext = ctx.applicationContext
        contextRef = WeakReference(appContext)
        wifiManager = appContext.getSystemService(Context.WIFI_SERVICE) as? WifiManager

        scanReceiver = object : BroadcastReceiver() {
            override fun onReceive(c: Context, intent: Intent) {
                val success = intent.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false)
                if (success) {
                    handleScanResults()
                }
            }
        }

        val filter = IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION)
        appContext.registerReceiver(scanReceiver, filter)
        started = true

        startScanningLoop()
    }

    fun stop() {
        if (!started) return
        scanJob?.cancel()
        scope.cancel()
        try {
            contextRef?.get()?.unregisterReceiver(scanReceiver)
        } catch (_: Exception) {
            // ignore
        }
        scanReceiver = null
        wifiManager = null
        contextRef = null
        started = false
        synchronized(buffer) { buffer.clear() }
        synchronized(listeners) { listeners.clear() }
    }

    fun addListener(listener: (ScanBatch) -> Unit) {
        synchronized(listeners) { listeners.add(listener) }
    }

    fun removeListener(listener: (ScanBatch) -> Unit) {
        synchronized(listeners) { listeners.remove(listener) }
    }

    fun requestImmediateScan() {
        if (!started) return
        scope.launch {
            @Suppress("DEPRECATION")
            wifiManager?.startScan()
        }
    }

    private fun startScanningLoop() {
        scanJob?.cancel()
        scanJob = scope.launch {
            while (isActive && started) {
                @Suppress("DEPRECATION")
                val success = wifiManager?.startScan() ?: false
                
                // If the scan request failed (e.g. throttling), wait a bit longer before retrying
                // otherwise wait the minimal interval
                if (success) {
                    delay(SCAN_INTERVAL_MS)
                } else {
                    delay(2000L) // Back off slightly if throttled
                }
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
                listeners.toList().forEach { it(batch) }
            }
        } catch (_: SecurityException) {
            // Permission might have been revoked
        }
    }

    fun latest(): ScanBatch? = synchronized(buffer) { buffer.firstOrNull() }

    /**
     * Currently unused
     */
    fun nearestByWallClock(targetTimeMs: Long, maxDeltaMs: Long = 5000L): ScanBatch? =
        synchronized(buffer) {
            val best = buffer.minByOrNull { abs(it.timestamp - targetTimeMs) } ?: return null
            val delta = abs(best.timestamp - targetTimeMs)
            if (delta <= maxDeltaMs) best else null
        }

    /**
     * Returns the first RSSI measurement for the given SSID that occurs after [afterTimeMs].
     * This is used to wait for a fresh RSSI reading after the user presses the measurement button.
     */
    fun getFirstRSSIAfter(ssid: String, afterTimeMs: Long): Pair<Int, Long>? =
        synchronized(buffer) {
            buffer.firstOrNull { batch ->
                batch.timestamp >= afterTimeMs && batch.results.any { it.SSID == ssid }
            }?.let { batch ->
                val rssi = batch.results.first { it.SSID == ssid }.level
                rssi to batch.timestamp
            }
        }

    /**
     * Suspends until a fresh RSSI reading for [ssid] is available after [afterTimeMs].
     */
    suspend fun awaitFirstRSSIAfter(
        ssid: String,
        afterTimeMs: Long,
        timeoutMs: Long = 10_000L // default timeout 10 seconds
    ): Pair<Int, Long> = withTimeout(timeoutMs) {
        suspendCancellableCoroutine { cont ->
            // 1. Check if we already have it in buffer
            val existing = getFirstRSSIAfter(ssid, afterTimeMs)
            if (existing != null) {
                cont.resume(existing)
                return@suspendCancellableCoroutine
            }

            // 2. If not, listen for new batches
            val listener = object : (ScanBatch) -> Unit {
                override fun invoke(batch: ScanBatch) {
                    if (batch.timestamp >= afterTimeMs) {
                        val match = batch.results.firstOrNull { it.SSID == ssid }
                        if (match != null) {
                            removeListener(this)
                            if (cont.isActive) {
                                cont.resume(match.level to batch.timestamp)
                            }
                        }
                    }
                }
            }
            addListener(listener)

            cont.invokeOnCancellation {
                removeListener(listener)
            }
        }
    }
}