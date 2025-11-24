package com.example.polaris

import android.Manifest
import android.annotation.SuppressLint
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.provider.Settings
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import androidx.room.Dao
import androidx.room.Database
import androidx.room.Entity
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.PrimaryKey
import androidx.room.Query
import androidx.room.Room
import androidx.room.RoomDatabase
import com.google.gson.GsonBuilder
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import androidx.activity.result.contract.ActivityResultContracts
import kotlin.coroutines.cancellation.CancellationException

@Entity(tableName = "signal_records")
data class SignalRecord(
    @PrimaryKey(autoGenerate = true) val id: Int = 0,
    val latitude: Double,
    val longitude: Double,
    val ssid: String,
    val rssi: Int,
    val timestamp: Long = System.currentTimeMillis(),
    val deviceID: String
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

@SuppressLint("HardwareIds")
@Suppress("DEPRECATION")
class MainActivity : AppCompatActivity() {

    // Room
    private lateinit var db: AppDatabase
    private lateinit var signalDao: SignalDao
    private lateinit var sourcePositionDao: SourcePositionDao

    // UI
    private lateinit var cleanRecordsBtn: Button
    private lateinit var exportBtn: Button
    private lateinit var selectSsidBtn: Button
    private lateinit var allRecordsText: TextView
    private lateinit var takeMeasurementBtn: Button
    private lateinit var statusText: TextView
    private lateinit var measureSourceBtn: Button
    private lateinit var accuracyTestBtn: Button
    private lateinit var sourceNameText: TextView

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

    private var isMeasuring = false
    private var timerJob: Job? = null // Tracks the coroutine responsible for timing the measurement window
    private var measurementStartTime = 0L
    private var measurementTargetSsid: String? = null
    private val timeFormat = java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.getDefault())

    private val measurementWindowMs = 20_000L

    private val rssiListener: (RSSIStream.ScanBatch) -> Unit = { batch ->
        handleScanBatch(batch)
    }

    // Session-only name for the source position
    private var sessionSourceName: String? = null
    private var isMeasuringSource = false
    private var measurementOffsetMs = 5_000L

    private val deviceID: String by lazy {
        Settings.Secure.getString(contentResolver, Settings.Secure.ANDROID_ID) ?: "unknown"
    }

    private fun handleScanBatch(batch: RSSIStream.ScanBatch) {
        val newSsids = batch.results.mapNotNull { it.SSID.takeIf { ss -> ss.isNotBlank() } }
        if (newSsids.isNotEmpty()) {
            val merged = (ssidList + newSsids).filter { it.isNotBlank() }.distinct().sorted()
            if (merged.size != ssidList.size || !ssidList.containsAll(merged)) {
                ssidList.clear()
                ssidList.addAll(merged)
                runOnUiThread { updateSelectedSsidButton() }
            }
        }
    }

    private fun startManualMeasurement() {
        val target = selectedSSID
        if (target.isNullOrEmpty()) {
            statusText.text = getString(R.string.status_no_ssid)
            Toast.makeText(this, getString(R.string.prompt_select_ssid_first), Toast.LENGTH_SHORT).show()
            return
        }

        if (!hasAllPermissions()) {
            ActivityCompat.requestPermissions(this, requiredPermissions, locationPermissionRequestCode)
            return
        }

        if (isMeasuring) return

        isMeasuring = true
        measurementTargetSsid = target
        measurementStartTime = System.currentTimeMillis()
        takeMeasurementBtn.isEnabled = false
        statusText.text = getString(R.string.status_waiting_rssi)
        RSSIStream.requestImmediateScan()
        
        startMeasurementLoop() // Start the smooth timer and measurement logic
    }

    private fun startMeasurementLoop() {
        timerJob?.cancel()
        timerJob = lifecycleScope.launch {
            statusText.text = getString(R.string.status_waiting_rssi)
            try {
                val target = measurementTargetSsid ?: return@launch
                val (rssi, timestamp) = RSSIStream.awaitFirstRSSIAfter(target, measurementStartTime + measurementOffsetMs)
                completeMeasurement(target, rssi, timestamp, timestamp - measurementStartTime)
            } catch (_: CancellationException) {
                // cancelled
            }
        }
    }

    private fun completeMeasurement(ssid: String, rssi: Int, seenTimeMs: Long, measurementDurationMs: Long) {
        if (!isMeasuring) return
        isMeasuring = false
        
        lifecycleScope.launch {
            val avgPair = withContext(Dispatchers.Default) {
                LocationStream.getAveragedLocationAndSamplesFrom(seenTimeMs, measurementDurationMs)
            }

            if (avgPair == null) {
                withContext(Dispatchers.Main) {
                    statusText.text = getString(R.string.status_measurement_failed)
                    takeMeasurementBtn.isEnabled = true
                    Toast.makeText(this@MainActivity, getString(R.string.no_location), Toast.LENGTH_SHORT).show()
                }
                measurementTargetSsid = null
                return@launch
            }

            val avgLoc = avgPair.first
            val record = SignalRecord(
                latitude = avgLoc.latitude,
                longitude = avgLoc.longitude,
                ssid = ssid,
                rssi = rssi,
                timestamp = seenTimeMs,
                deviceID = deviceID
            )
            signalDao.insert(record)

            withContext(Dispatchers.Main) {
                statusText.text = getString(R.string.status_measurement_taken, avgPair.second.size)
                takeMeasurementBtn.isEnabled = true
                Toast.makeText(
                    this@MainActivity,
                    getString(R.string.rssi_updated, rssi, timeFormat.format(java.util.Date(seenTimeMs))),
                    Toast.LENGTH_SHORT
                ).show()
                refreshRecordsView()
            }
            measurementTargetSsid = null
        }
    }

    private fun resetMeasurementState() {
        isMeasuring = false
        timerJob?.cancel() // Stop the timer
        measurementTargetSsid = null
        runOnUiThread {
            takeMeasurementBtn.isEnabled = true
            statusText.text = getString(R.string.status_ready)
        }
    }

    private fun hasAllPermissions(): Boolean {
        return requiredPermissions.all { perm ->
            ContextCompat.checkSelfPermission(this, perm) == PackageManager.PERMISSION_GRANTED
        }
    }

    // Define the launcher to handle the result from MeasureSourceActivity
    private val measureSourceLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
        if (result.resultCode == RESULT_OK) {
            isMeasuringSource = true
            // Refresh UI to show the new source position immediately
            refreshRecordsView()
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
        takeMeasurementBtn = findViewById(R.id.takeMeasurementBtn)
        statusText = findViewById(R.id.statusText)
        measureSourceBtn = findViewById(R.id.measureSourceBtn)
        accuracyTestBtn = findViewById(R.id.accuracyTestBtn)
        sourceNameText = findViewById(R.id.source_name_text)
        statusText.text = getString(R.string.status_ready)

        // UI actions
        selectSsidBtn.setOnClickListener { showSsidChoiceDialog() }

        takeMeasurementBtn.setOnClickListener { startManualMeasurement() }

        exportBtn.setOnClickListener { promptFreeTextAndExport() }

        cleanRecordsBtn.setOnClickListener {
            AlertDialog.Builder(this)
                .setTitle(getString(R.string.confirm_delete_title))
                .setMessage(getString(R.string.confirm_delete_message))
                .setPositiveButton(getString(R.string.confirm_delete_positive)) { _, _ ->
                    lifecycleScope.launch {
                        // Only delete signal records, preserve source position
                        signalDao.deleteAll()
                        refreshRecordsView()
                        Toast.makeText(this@MainActivity, "Recordings deleted", Toast.LENGTH_SHORT).show()
                    }
                }
                .setNegativeButton(getString(R.string.confirm_delete_negative), null)
                .show()
        }

        measureSourceBtn.setOnClickListener {
            val intent = Intent(this, MeasureSourceActivity::class.java)
            measureSourceLauncher.launch(intent)
        }

        accuracyTestBtn.setOnClickListener {
            val intent = Intent(this, AccuracyTestActivity::class.java)
            startActivity(intent)
        }

        val missing = requiredPermissions.filter { ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED }.toTypedArray()
        if (missing.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, missing, locationPermissionRequestCode)
        }

        refreshRecordsView()
    }

    override fun onResume() {
        super.onResume()
        if (hasAllPermissions()) {
            LocationStream.start(this)
            RSSIStream.start(this)
            RSSIStream.addListener(rssiListener)
            RSSIStream.latest()?.let { handleScanBatch(it) }
        } else {
            ActivityCompat.requestPermissions(this, requiredPermissions, locationPermissionRequestCode)
        }
        refreshRecordsView()
        checkAndRefreshSource()
    }

    private fun checkAndRefreshSource() {
        lifecycleScope.launch {
            val pos = sourcePositionDao.get()
            
            // If we just came back from measuring and we have a new measurement, prompt for a name
            if (isMeasuringSource) {
                isMeasuringSource = false
                if (pos != null) {
                    promptForSourceName()
                }
            }
            
            updateSourceUi(pos)
        }
    }

    private fun promptForSourceName() {
        val input = EditText(this).apply {
            hint = getString(R.string.enter_source_name_hint)
        }
        AlertDialog.Builder(this)
            .setTitle(getString(R.string.enter_source_name_title))
            .setView(input)
            .setPositiveButton(getString(R.string.set_name)) { _, _ ->
                val name = input.text.toString().trim()
                if (name.isNotEmpty()) {
                    sessionSourceName = name
                    // Refresh UI to show new name
                    lifecycleScope.launch { updateSourceUi(sourcePositionDao.get()) }
                }
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    private suspend fun updateSourceUi(pos: SourcePosition?) {
        withContext(Dispatchers.Main) {
            if (pos != null) {
                val name = sessionSourceName ?: getString(R.string.source_default_name)
                sourceNameText.text = getString(R.string.selected_source, name)
            } else {
                sourceNameText.text = getString(R.string.placeholder_source)
            }
        }
    }

    override fun onPause() {
        super.onPause()
        RSSIStream.removeListener(rssiListener)
        RSSIStream.stop()
        LocationStream.stop()
        resetMeasurementState()
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
                resetMeasurementState()
                dialog.dismiss()
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    @SuppressLint("SetTextI18n")
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
                        getString(R.string.record_item, it.ssid, it.latitude, it.longitude, it.rssi, java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.getDefault()).format(java.util.Date(it.timestamp)))
                    }
                    allRecordsText.text = header + body
                }
            }
        }
    }

    private fun promptFreeTextAndExport() {
        val input = EditText(this).apply {
            hint = getString(R.string.free_text_hint)
        }

        AlertDialog.Builder(this)
            .setTitle(getString(R.string.free_text_title))
            .setView(input)
            .setPositiveButton(getString(R.string.free_text_positive)) { dialog, _ ->
                val sanitized = sanitizeFreeText(input.text?.toString().orEmpty())
                exportDatabaseToJson(sanitized)
                dialog.dismiss()
            }
            .setNegativeButton(android.R.string.cancel) { dialog, _ -> dialog.dismiss() }
            .show()
    }

    private fun sanitizeFreeText(raw: String): String {
        val sanitized = raw.trim().replace(Regex("[^A-Za-z0-9_-]"), "").take(50)
        return sanitized.ifBlank {
            "noTag_" + raw.hashCode().toUInt().toString(16)
        }
    }

    private fun exportDatabaseToJson(freeText: String) {
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
            val fileName = "${System.currentTimeMillis()}_${deviceID}_${timeStr}_${freeText}.json"
            val file = java.io.File(getExternalFilesDir(null)?.absolutePath ?: filesDir.absolutePath, fileName)
            file.writeText(jsonString)
            runOnUiThread { allRecordsText.text = getString(R.string.exported_to, file.absolutePath) }
        }
    }
}
