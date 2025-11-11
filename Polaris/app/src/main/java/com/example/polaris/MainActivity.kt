@file:Suppress("AndroidUnresolvedRoomSqlReference")

package com.example.polaris

import android.Manifest
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.location.Location
import android.net.wifi.WifiManager
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AlertDialog
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import androidx.room.*
import com.google.android.gms.location.*
import kotlinx.coroutines.launch
import kotlinx.coroutines.async
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

// --- DATABASE SETUP ---

@Entity(tableName = "signal_records")
data class SignalRecord(
    @PrimaryKey(autoGenerate = true) val id: Int = 0,
    val latitude: Double,
    val longitude: Double,
    val ssid: String,
    val rssi: Int,
    val timestamp: Long = System.currentTimeMillis()
)

@Dao
interface SignalDao {
    @Insert
    suspend fun insert(record: SignalRecord)

    @Query("SELECT * FROM signal_records ORDER BY timestamp DESC")
    suspend fun getAll(): List<SignalRecord>

    @Query("DELETE FROM signal_records")
    suspend fun deleteAll()
}

@Database(entities = [SignalRecord::class], version = 2, exportSchema = false)
abstract class AppDatabase : RoomDatabase() {
    abstract fun signalDao(): SignalDao
}

// --- MAIN ACTIVITY ---

@Suppress("DEPRECATION")
class MainActivity : AppCompatActivity() {

    private lateinit var db: AppDatabase
    private lateinit var signalDao: SignalDao

    private lateinit var fusedLocationClient: FusedLocationProviderClient
    private lateinit var locationRequest: LocationRequest
    private lateinit var locationCallback: LocationCallback

    private lateinit var coordinatesText: TextView
    private lateinit var getLocationBtn: Button
    private lateinit var cleanRecordsBtn: Button
    private lateinit var exportBtn: Button
    private lateinit var selectSsidBtn: Button
    private lateinit var allRecordsText: TextView

    private val ssidList = mutableListOf<String>()
    private var selectedSSID: String? = null

    private val locationPermissionRequestCode = 1001
    private val requiredPermissions = arrayOf(
        Manifest.permission.ACCESS_FINE_LOCATION,
        Manifest.permission.ACCESS_COARSE_LOCATION,
        Manifest.permission.ACCESS_WIFI_STATE
    )

    private val wifiScanReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            val success = intent?.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false) ?: false
            if (success) {
                val wifiManager = applicationContext.getSystemService(WIFI_SERVICE) as WifiManager
                if (ContextCompat.checkSelfPermission(this@MainActivity, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED &&
                    ContextCompat.checkSelfPermission(this@MainActivity, Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                    // Request required permissions and skip processing scan results for now
                    ActivityCompat.requestPermissions(
                        this@MainActivity,
                        arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION, Manifest.permission.ACCESS_WIFI_STATE),
                        locationPermissionRequestCode
                    )
                    return
                }
                val scanResults = wifiManager.scanResults
                // preserve currently selected SSID so we don't auto-replace it
                val previouslySelectedSSID = selectedSSID

                // Build a new list from scan results (unique, non-empty, sorted)
                val scannedSsids = scanResults.mapNotNull { it.SSID.takeIf { ss -> ss.isNotBlank() } }
                    .distinct()
                    .sorted()

                // Merge: keep chosen SSID if missing from scan results by adding it to the list
                val mergedList = mutableListOf<String>().apply {
                    addAll(scannedSsids)
                    if (previouslySelectedSSID != null && previouslySelectedSSID !in this) {
                        // add at top so user sees their chosen SSID even if currently not scanned
                        add(0, previouslySelectedSSID)
                    }
                }

                ssidList.clear()
                ssidList.addAll(mergedList)

                runOnUiThread {
                    // If the previously chosen SSID has disappeared from the current scan, alert the user
                    if (previouslySelectedSSID != null && previouslySelectedSSID !in scannedSsids) {
                        AlertDialog.Builder(this@MainActivity)
                            .setTitle(getString(R.string.network_unavailable_title))
                            .setMessage(getString(R.string.network_unavailable_message, previouslySelectedSSID))
                            .setPositiveButton(android.R.string.ok, null)
                            .show()
                        // Do NOT clear the selection — keep it so the user can retry reconnecting later.
                    } else {
                        // Restore a previously chosen SSID (if still available), but do NOT auto-select the first scanned SSID.
                        if (previouslySelectedSSID != null) {
                            selectedSSID = previouslySelectedSSID
                        }
                    }
                    updateSelectedSsidButton()
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Initialize Room
        db = Room.databaseBuilder(applicationContext, AppDatabase::class.java, "signal-db")
            .fallbackToDestructiveMigration() // handle schema change during development
            .build()
        signalDao = db.signalDao()

        // Initialize UI
        coordinatesText = findViewById(R.id.coordinatesText)
        getLocationBtn = findViewById(R.id.getLocationBtn)
        cleanRecordsBtn = findViewById(R.id.cleanRecordsBtn)
        allRecordsText = findViewById(R.id.allRecordsText)
        selectSsidBtn = findViewById(R.id.selectSsidBtn)
        exportBtn = findViewById(R.id.exportBtn)

        // Location client
        fusedLocationClient = LocationServices.getFusedLocationProviderClient(this)
        // TODO: Adjust location request interval for more frequent updates if you use continuous updates
        locationRequest = LocationRequest.Builder(Priority.PRIORITY_HIGH_ACCURACY, 1000)
            .build()

        locationCallback = object : LocationCallback() {
            override fun onLocationResult(result: LocationResult) {
                // kept for compatibility if you later use continuous updates; no DB insert here
            }
        }

        // selectSsidBtn will call the class-level showSsidChoiceDialog() which updates the button text

        // ensure permissions at startup so the app is ready to use immediately
        checkAndRequestPermissions()

        refreshRecordsView()

        // Get location button — start coordinated single-shot measurement
        getLocationBtn.setOnClickListener {
            lifecycleScope.launch {
                performMeasurement()
            }
        }

        // Clean records button
        cleanRecordsBtn.setOnClickListener {
            AlertDialog.Builder(this)
                .setTitle(getString(R.string.confirm_delete_title))
                .setMessage(getString(R.string.confirm_delete_message))
                .setPositiveButton(getString(R.string.confirm_delete_positive)) { _, _ ->
                    lifecycleScope.launch {
                        signalDao.deleteAll()
                        refreshRecordsView()
                    }
                }
                .setNegativeButton(getString(R.string.confirm_delete_negative), null)
                .show()
        }

        exportBtn.setOnClickListener {
            exportDatabaseToJson()
        }

        selectSsidBtn.setOnClickListener { showSsidChoiceDialog() }
    }

    override fun onResume() {
        super.onResume()
        registerReceiver(wifiScanReceiver, IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION))
        val wifiManager = applicationContext.getSystemService(WIFI_SERVICE) as WifiManager
        wifiManager.startScan()
    }

    override fun onPause() {
        super.onPause()
        try { unregisterReceiver(wifiScanReceiver) } catch (_: Exception) { }
        fusedLocationClient.removeLocationUpdates(locationCallback)
    }

    // Suspend until a Wi-Fi scan result is available (one-shot). Returns the scanResults or null on timeout.
    private suspend fun awaitWifiScan(timeoutMs: Long = 10_000L): List<android.net.wifi.ScanResult>? {
        return withTimeoutOrNull(timeoutMs) {
            suspendCancellableCoroutine<List<android.net.wifi.ScanResult>> { cont ->
                val wifiManager = applicationContext.getSystemService(WIFI_SERVICE) as WifiManager
                val receiver = object : BroadcastReceiver() {
                    override fun onReceive(ctx: Context?, intent: Intent?) {
                        if (ContextCompat.checkSelfPermission(this@MainActivity, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED &&
                            ContextCompat.checkSelfPermission(this@MainActivity, Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                            ActivityCompat.requestPermissions(
                                this@MainActivity,
                                arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION, Manifest.permission.ACCESS_WIFI_STATE),
                                locationPermissionRequestCode
                            )
                            if (!cont.isCompleted) cont.resume(emptyList())
                            try { unregisterReceiver(this) } catch (_: Exception) { }
                            return
                        }
                        val results = wifiManager.scanResults
                        if (!cont.isCompleted) cont.resume(results)
                        try { unregisterReceiver(this) } catch (_: Exception) { }
                    }
                }

                // Register the receiver and start an async scan. We intentionally register a one-shot receiver here.
                registerReceiver(receiver, IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION))
                wifiManager.startScan()

                cont.invokeOnCancellation {
                    try { unregisterReceiver(receiver) } catch (_: Exception) { }
                }
            }
        }
    }

    // Suspend wrapper for getCurrentLocation with timeout
    private suspend fun getCurrentLocationSuspend(timeoutMs: Long = 10_000L): Location? {
        return withTimeoutOrNull(timeoutMs) {
            suspendCancellableCoroutine { cont ->
                // Ensure we have runtime location permission before requesting current location.
                if (ContextCompat.checkSelfPermission(this@MainActivity, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED &&
                    ContextCompat.checkSelfPermission(this@MainActivity, Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                    // Request required permissions; resume null for this call — caller should retry after grant.
                    ActivityCompat.requestPermissions(
                        this@MainActivity,
                        arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION, Manifest.permission.ACCESS_WIFI_STATE),
                        locationPermissionRequestCode
                    )
                    if (!cont.isCompleted) cont.resume(null)
                    return@suspendCancellableCoroutine
                }

                val task = fusedLocationClient.getCurrentLocation(Priority.PRIORITY_HIGH_ACCURACY, null)
                task.addOnSuccessListener { loc -> if (!cont.isCompleted) cont.resume(loc) }
                task.addOnFailureListener { ex -> if (!cont.isCompleted) cont.resumeWithException(ex) }
                cont.invokeOnCancellation { /* nothing to cancel explicitly */ }
            }
        }
    }

    // Coordinated measurement: start a scan and a single-shot location concurrently and commit one record when both arrive
    private suspend fun performMeasurement() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
            != PackageManager.PERMISSION_GRANTED
        ) {
            ActivityCompat.requestPermissions(
                this,
                arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_WIFI_STATE),
                locationPermissionRequestCode
            )
            return
        }

        coordinatesText.text = getString(R.string.measuring_location)

        val wifiDeferred = lifecycleScope.async { awaitWifiScan(10_000L) }
        val locDeferred = lifecycleScope.async { getCurrentLocationSuspend(10_000L) }

        val scanResults = wifiDeferred.await()
        val location = locDeferred.await()

        val currentSsid = selectedSSID
        if (currentSsid.isNullOrEmpty()) {
            coordinatesText.text = getString(R.string.prompt_select_ssid_first)
            return
        }

        val rssi = scanResults?.firstOrNull { it.SSID == currentSsid }?.level

        if (location != null && rssi != null) {
            val lat = location.latitude
            val lon = location.longitude
            val record = SignalRecord(latitude = lat, longitude = lon, ssid = currentSsid, rssi = rssi)
            lifecycleScope.launch {
                signalDao.insert(record)
                // Show coordinates + RSSI together
                coordinatesText.text = getString(R.string.coords_rssi, lat, lon, rssi)
                refreshRecordsView()
            }
        } else {
            if (location == null && scanResults == null) {
                coordinatesText.text = getString(R.string.measurement_failed_loc_or_to)
            } else if (location == null) {
                coordinatesText.text = getString(R.string.measurement_failed_or_unavail)
            } else {
                coordinatesText.text = getString(R.string.measurement_failed_no_ssid)
            }
        }
    }

    private fun refreshRecordsView() {
        lifecycleScope.launch {
            val all = signalDao.getAll()
            runOnUiThread {
                if (all.isEmpty()) {
                    allRecordsText.text = getString(R.string.placeholder_db)
                } else {
                    allRecordsText.text = all.joinToString("\n") {
                        // Use string resource for consistent localization/formatting
                        getString(R.string.record_item, it.ssid, it.latitude, it.longitude, it.rssi, it.timestamp)
                    }
                }
            }
        }
    }

    private fun exportDatabaseToJson() {
        lifecycleScope.launch {
            val allRecords = signalDao.getAll()

            if (allRecords.isEmpty()) {
                runOnUiThread {
                    allRecordsText.text = getString(R.string.no_data_export)
                }
                return@launch
            }

            // Convert list to JSON
            val gson = com.google.gson.GsonBuilder().setPrettyPrinting().create()
            val jsonString = gson.toJson(allRecords)

            // Save to Downloads folder
            val fileName = "signal_records_${System.currentTimeMillis()}.json"
            val file = java.io.File(
                getExternalFilesDir(null)?.absolutePath ?: filesDir.absolutePath,
                fileName
            )

            file.writeText(jsonString)

            runOnUiThread {
                allRecordsText.text = getString(R.string.exported_to, file.absolutePath)
            }
        }
    }

    private fun updateSelectedSsidButton() {
        selectSsidBtn.text = selectedSSID ?: getString(R.string.choose_ssid)
    }

    private fun showSsidChoiceDialog() {
        if (ssidList.isEmpty()) {
            AlertDialog.Builder(this)
                .setTitle(getString(R.string.choose_ssid))
                .setMessage(getString(R.string.no_ssids_found))
                .setPositiveButton(android.R.string.ok, null)
                .show()
            return
        }

        val items = ssidList.toTypedArray()
        val checkedIndex = selectedSSID?.let { ssidList.indexOf(it) } ?: -1
        AlertDialog.Builder(this)
            .setTitle(getString(R.string.choose_ssid))
            .setSingleChoiceItems(items, checkedIndex) { dialog, which ->
                selectedSSID = ssidList.getOrNull(which)
                updateSelectedSsidButton()
                dialog.dismiss()
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    private fun checkAndRequestPermissions() {
        val missing = requiredPermissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }.toTypedArray()

        if (missing.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, missing, locationPermissionRequestCode)
        } else {
            // Permissions already granted — start an initial Wi‑Fi scan
            val wifiManager = applicationContext.getSystemService(WIFI_SERVICE) as WifiManager
            wifiManager.startScan()
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == locationPermissionRequestCode) {
            // If any relevant permission was granted, trigger a scan so UI becomes usable immediately.
            if (grantResults.isNotEmpty() && grantResults.any { it == PackageManager.PERMISSION_GRANTED }) {
                try {
                    val wifiManager = applicationContext.getSystemService(WIFI_SERVICE) as WifiManager
                    wifiManager.startScan()
                } catch (_: Exception) { }
            }
        }
    }
}