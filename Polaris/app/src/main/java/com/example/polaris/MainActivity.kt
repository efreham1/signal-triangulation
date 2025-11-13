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
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AlertDialog
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import androidx.room.*
import com.google.android.gms.location.*
import com.google.gson.GsonBuilder
import kotlinx.coroutines.launch
import kotlin.math.abs

@Entity(tableName = "signal_records")
data class SignalRecord(
    @PrimaryKey(autoGenerate = true) val id: Int = 0,
    val latitude: Double,
    val longitude: Double,
    val ssid: String,
    val rssi: Int,
    val timestamp: Long = System.currentTimeMillis()
)

// new: persistent source position (single-row, id==0)
@Entity(tableName = "source_position")
data class SourcePosition(
    @PrimaryKey val id: Int = 0,
    val latitude: Double?,
    val longitude: Double?
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

@Dao
interface SourcePositionDao {
    @Query("SELECT * FROM source_position WHERE id = 0")
    suspend fun get(): SourcePosition?

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsert(pos: SourcePosition)

    @Query("DELETE FROM source_position")
    suspend fun deleteAll()
}

@Database(entities = [SignalRecord::class, SourcePosition::class], version = 3, exportSchema = false)
abstract class AppDatabase : RoomDatabase() {
    abstract fun signalDao(): SignalDao
    abstract fun sourcePositionDao(): SourcePositionDao
}

@Suppress("DEPRECATION")
class MainActivity : AppCompatActivity() {

    // Room
    private lateinit var db: AppDatabase
    private lateinit var signalDao: SignalDao
    private lateinit var sourcePositionDao: SourcePositionDao

    // Location
    private lateinit var fusedLocationClient: FusedLocationProviderClient
    private lateinit var locationRequest: LocationRequest
    private lateinit var locationCallback: LocationCallback

    // UI
    private lateinit var cleanRecordsBtn: Button
    private lateinit var exportBtn: Button
    private lateinit var selectSsidBtn: Button
    private lateinit var allRecordsText: TextView
    private lateinit var startAutoBtn: Button
    private lateinit var stopAutoBtn: Button
    private lateinit var measureSourceBtn: Button

    // SSID list & selection
    private val ssidList = mutableListOf<String>()
    private var selectedSSID: String? = null

    // Permissions
    private val locationPermissionRequestCode = 1001
    private val requiredPermissions = arrayOf(
        Manifest.permission.ACCESS_FINE_LOCATION,
        Manifest.permission.ACCESS_COARSE_LOCATION,
        Manifest.permission.ACCESS_WIFI_STATE
    )

    // location buffer for timestamp matching
    private val locationBuffer = ArrayDeque<Location>()
    private val maxLocationBufferSize = 50

    // track last scan timestamps per BSSID/SSID to detect updates
    private val lastScanTimestamps = mutableMapOf<String, Long>()

    // automatic mode flag
    @Volatile
    private var isAutoRunning = false

    private val wifiScanReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            val resultsUpdated = intent?.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false) ?: false
            val wifiManager = applicationContext.getSystemService(WIFI_SERVICE) as WifiManager

            // ensure permissions before reading scanResults
            if (ContextCompat.checkSelfPermission(this@MainActivity, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED &&
                ContextCompat.checkSelfPermission(this@MainActivity, Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(
                    this@MainActivity,
                    requiredPermissions,
                    locationPermissionRequestCode
                )
                return
            }

            val scanResults = wifiManager.scanResults ?: emptyList()

            // Build SSID list (unique, non-empty, sorted) and preserve previous selection
            val previouslySelectedSSID = selectedSSID
            val scannedSsids = scanResults.mapNotNull { it.SSID.takeIf { ss -> ss.isNotBlank() } }.distinct().sorted()
            val mergedList = mutableListOf<String>().apply {
                addAll(scannedSsids)
                if (previouslySelectedSSID != null && previouslySelectedSSID !in this) add(0, previouslySelectedSSID)
            }
            ssidList.clear()
            ssidList.addAll(mergedList)

            runOnUiThread { updateSelectedSsidButton() }

            // detect update for selected SSID (even if RSSI value didn't change)
            val sel = selectedSSID
            if (!sel.isNullOrEmpty()) {
                val found = scanResults.firstOrNull { it.SSID == sel }
                if (found != null) {
                    // Use arrival wall-clock time for matching locations (keeps units consistent with Location.time)
                    val seenTimeMs = System.currentTimeMillis()
                    val key = found.BSSID ?: found.SSID
                    val prev = lastScanTimestamps[key]
                    // treat as new if we haven't seen it before or it's updated
                    if (prev == null || seenTimeMs > prev || resultsUpdated) {
                        lastScanTimestamps[key] = seenTimeMs
                        // only perform automatic capture when auto mode is running
                        if (isAutoRunning) {
                            onRssiMeasurementUpdated(found, seenTimeMs)
                        } 
                    }
                } else {
                    // selected SSID not found in current scan
                    runOnUiThread {
                        Toast.makeText(
                            this@MainActivity,
                            getString(R.string.network_unavailable_message, sel),
                            Toast.LENGTH_SHORT
                        ).show()
                    }
                }
            }
        }
    }

    private fun onRssiMeasurementUpdated(scanResult: android.net.wifi.ScanResult, seenTimeMs: Long) {
        val now = System.currentTimeMillis()
        val timeStr = java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.getDefault()).format(java.util.Date(now))

        // UI feedback (toast)
        runOnUiThread {
            Toast.makeText(this@MainActivity, getString(R.string.rssi_updated, scanResult.level, timeStr), Toast.LENGTH_SHORT).show()
        }

        // capture matched location and insert into DB asynchronously
        lifecycleScope.launch {
            // try to find the buffered location closest to seenTimeMs
            val bestLoc = synchronized(locationBuffer) {
                if (locationBuffer.isEmpty()) null
                else locationBuffer.minByOrNull { abs(it.time - seenTimeMs) }
            }

            if (bestLoc == null) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, getString(R.string.no_location), Toast.LENGTH_SHORT).show()
                }
            } else {
                val record = SignalRecord(
                        latitude = bestLoc.latitude,
                        longitude = bestLoc.longitude,
                        ssid = scanResult.SSID,
                        rssi = scanResult.level,
                        timestamp = seenTimeMs
                    )
                signalDao.insert(record)
                runOnUiThread { refreshRecordsView() }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Room database init
        db = Room.databaseBuilder(applicationContext, AppDatabase::class.java, "signal-db")
            .fallbackToDestructiveMigration()
            .build()
        signalDao = db.signalDao()
        sourcePositionDao = db.sourcePositionDao()

        // UI bindings
        cleanRecordsBtn = findViewById(R.id.cleanRecordsBtn)
        allRecordsText = findViewById(R.id.allRecordsText)
        selectSsidBtn = findViewById(R.id.selectSsidBtn)
        exportBtn = findViewById(R.id.exportBtn)
        startAutoBtn = findViewById(R.id.startAutoBtn)
        stopAutoBtn = findViewById(R.id.stopAutoBtn)
        measureSourceBtn = findViewById(R.id.measureSourceBtn)

        // initial auto state
        isAutoRunning = false
        updateAutoButtons()

        // Location client & continuous buffer callback
        fusedLocationClient = LocationServices.getFusedLocationProviderClient(this)
        locationRequest = LocationRequest.Builder(Priority.PRIORITY_HIGH_ACCURACY, 1000)
            .setMinUpdateIntervalMillis(250)
            .build()

        locationCallback = object : LocationCallback() {
            override fun onLocationResult(result: LocationResult) {
                val loc = result.lastLocation ?: return
                synchronized(locationBuffer) {
                    locationBuffer.addFirst(loc)
                    while (locationBuffer.size > maxLocationBufferSize) locationBuffer.removeLast()
                }
            }
        }

        // UI actions
        selectSsidBtn.setOnClickListener { showSsidChoiceDialog() }

        startAutoBtn.setOnClickListener { startMeasurements() }
        stopAutoBtn.setOnClickListener { stopMeasurements() }

        cleanRecordsBtn.setOnClickListener {
            AlertDialog.Builder(this)
                .setTitle(getString(R.string.confirm_delete_title))
                .setMessage(getString(R.string.confirm_delete_message))
                .setPositiveButton(getString(R.string.confirm_delete_positive)) { _, _ ->
                    lifecycleScope.launch {
                        signalDao.deleteAll()
                        sourcePositionDao.deleteAll()
                        refreshRecordsView()
                    }
                }
                .setNegativeButton(getString(R.string.confirm_delete_negative), null)
                .show()
        }

        exportBtn.setOnClickListener { exportDatabaseToJson() }

        measureSourceBtn.setOnClickListener {
            val intent = android.content.Intent(this, MeasureSourceActivity::class.java)
            startActivity(intent)
        }

        val missing = requiredPermissions.filter { ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED }.toTypedArray()
        if (missing.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, missing, locationPermissionRequestCode)
        }

        refreshRecordsView()
    }

    private fun updateAutoButtons() {
        runOnUiThread {
            startAutoBtn.isEnabled = !isAutoRunning
            stopAutoBtn.isEnabled = isAutoRunning
        }
    }

    private fun startMeasurements() {
        if (selectedSSID.isNullOrEmpty()) {
            Toast.makeText(this, getString(R.string.prompt_select_ssid_first), Toast.LENGTH_SHORT).show()
            return
        }
        isAutoRunning = true
        updateAutoButtons()
        Toast.makeText(this, getString(R.string.recording_started), Toast.LENGTH_SHORT).show()
    }

    private fun stopMeasurements() {
        isAutoRunning = false
        updateAutoButtons()
        Toast.makeText(this, getString(R.string.recording_stopped), Toast.LENGTH_SHORT).show()
    }

    override fun onResume() {
        super.onResume()
        registerReceiver(wifiScanReceiver, IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION))
        val wifiManager = applicationContext.getSystemService(WIFI_SERVICE) as WifiManager
        wifiManager.startScan()

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED ||
            ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED) {
            fusedLocationClient.requestLocationUpdates(locationRequest, locationCallback, mainLooper)
        }

        refreshRecordsView()
    }

    override fun onPause() {
        super.onPause()
        try { unregisterReceiver(wifiScanReceiver) } catch (_: Exception) { }
        fusedLocationClient.removeLocationUpdates(locationCallback)
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

    private fun refreshRecordsView() {
        lifecycleScope.launch {
            val source = sourcePositionDao.get()
            val all = signalDao.getAll()
            runOnUiThread {
                val header = if (source?.latitude != null && source.longitude != null) {
                    getString(R.string.source_position_label, source.latitude, source.longitude)
                } else {
                    getString(R.string.source_position_placeholder)
                }

                if (all.isEmpty()) {
                    allRecordsText.text = header + getString(R.string.placeholder_db)
                } else {
                    val body = all.joinToString("\n") {
                        // Use string resource for consistent localization/formatting
                        getString(R.string.record_item, it.ssid, it.latitude, it.longitude, it.rssi, it.timestamp)
                    }
                    allRecordsText.text = header + body
                }
            }
        }
    }

    private fun exportDatabaseToJson() {
        lifecycleScope.launch {
            val source = sourcePositionDao.get()
            val allRecords = signalDao.getAll()

            val exportObj = mapOf(
                "source_pos" to if (source?.latitude != null && source.longitude != null) {
                    mapOf("x" to source.latitude, "y" to source.longitude)
                } else null,
                "measurements" to allRecords
            )

            val gson = GsonBuilder().setPrettyPrinting().create()
            val jsonString = gson.toJson(exportObj)
            val timeStr = java.text.SimpleDateFormat("HH_mm_ss", java.util.Locale.getDefault()).format(java.util.Date())
            val fileName = "signal_records_${timeStr}.json"
            val file = java.io.File(getExternalFilesDir(null)?.absolutePath ?: filesDir.absolutePath, fileName)
            file.writeText(jsonString)
            runOnUiThread { allRecordsText.text = getString(R.string.exported_to, file.absolutePath) }
        }
    }
}