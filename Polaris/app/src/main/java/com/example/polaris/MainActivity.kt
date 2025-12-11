package com.example.polaris

import android.Manifest
import android.annotation.SuppressLint
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.provider.Settings
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.ActionBarDrawerToggle
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.drawerlayout.widget.DrawerLayout
import androidx.lifecycle.lifecycleScope
import androidx.room.Dao
import androidx.room.Database
import androidx.room.Entity
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.PrimaryKey
import androidx.room.Query
import androidx.room.RoomDatabase
import com.google.android.material.navigation.NavigationView
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.osmdroid.config.Configuration
import org.osmdroid.tileprovider.tilesource.TileSourceFactory
import org.osmdroid.util.GeoPoint
import org.osmdroid.views.MapView
import org.osmdroid.views.overlay.Marker
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

// persistent source position (single-row, id==0)
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
    private lateinit var signalDao: SignalDao
    private lateinit var sourcePositionDao: SourcePositionDao

    // UI
    private lateinit var cleanRecordsBtn: Button
    private lateinit var selectSsidBtn: Button
    private lateinit var takeMeasurementBtn: Button
    private lateinit var statusText: TextView

    private lateinit var mapView: MapView
    private var currentLocationMarker: Marker? = null
    private val measurementMarkers = mutableListOf<Marker>()

    private lateinit var drawerLayout: DrawerLayout
    private lateinit var navView: NavigationView
    private lateinit var toolbar: Toolbar

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

    // Auto-measurement state
    private var isAutoMeasuring = false
    private var autoMeasureJob: Job? = null
    private var autoMeasureDelayMs = 5000L // Default 5 seconds between measurements
    private lateinit var autoBtn: Button

    // Location updates job
    private var locationUpdatesJob: Job? = null
    private var hasInitiallyZoomedToLocation = false

    private val rssiListener: (RSSIStream.ScanBatch) -> Unit = { batch ->
        handleScanBatch(batch)
    }

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
            } catch (_: TimeoutCancellationException) {
                // Timeout - allow retry instead of getting stuck
                withContext(Dispatchers.Main) {
                    statusText.text = getString(R.string.status_ready)
                    if (!isAutoMeasuring) {
                        takeMeasurementBtn.isEnabled = true
                    }
                    Toast.makeText(this@MainActivity, getString(R.string.measurement_timeout), Toast.LENGTH_LONG).show()
                }
                resetMeasurementState()
            } catch (_: CancellationException) {
                // cancelled by user or activity lifecycle
            }
        }
    }

    @SuppressLint("StringFormatInvalid")
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
                    if (!isAutoMeasuring) {
                        takeMeasurementBtn.isEnabled = true
                    }
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
                if (!isAutoMeasuring) {
                    takeMeasurementBtn.isEnabled = true
                }
                Toast.makeText(
                    this@MainActivity,
                    getString(R.string.rssi_updated, rssi, timeFormat.format(java.util.Date(seenTimeMs))),
                    Toast.LENGTH_SHORT
                ).show()
                addMeasurementMarker(record)
            }
            measurementTargetSsid = null
        }
    }

    private fun promptAutoMeasurementDelay() {
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

        val input = EditText(this).apply {
            inputType = android.text.InputType.TYPE_CLASS_NUMBER
            hint = getString(R.string.auto_delay_hint)
            setText("5")
        }

        AlertDialog.Builder(this)
            .setTitle(getString(R.string.auto_delay_title))
            .setView(input)
            .setPositiveButton(android.R.string.ok) { _, _ ->
                val seconds = input.text.toString().toIntOrNull()
                if (seconds == null || seconds < 1) {
                    Toast.makeText(this, getString(R.string.invalid_number), Toast.LENGTH_SHORT).show()
                } else {
                    autoMeasureDelayMs = seconds * 1000L
                    startAutoMeasurement()
                }
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    private fun startAutoMeasurement() {
        if (isAutoMeasuring) return

        isAutoMeasuring = true
        autoBtn.text = getString(R.string.auto_stop)
        takeMeasurementBtn.isEnabled = false

        autoMeasureJob = lifecycleScope.launch {
            while (isActive && isAutoMeasuring) {
                
                // Trigger a measurement
                triggerAutoMeasurement()

                // Wait for measurement to complete
                while (isActive && isMeasuring) {
                    delay(100)
                }

                if (!isAutoMeasuring) break

                // Wait the configured delay before next measurement
                var remainingMs = autoMeasureDelayMs
                while (remainingMs > 0 && isAutoMeasuring) {
                    val secondsLeft = (remainingMs / 1000).toInt() + if (remainingMs % 1000 > 0) 1 else 0
                    statusText.text = getString(R.string.auto_next_in, secondsLeft)
                    vibrate()
                    val step = if (remainingMs >= 1000) 1000L else remainingMs
                    delay(step)
                    remainingMs -= step
                }
                vibrate(300L)
            }
        }
    }

    private fun triggerAutoMeasurement() {
        val target = selectedSSID ?: return

        if (isMeasuring) return

        isMeasuring = true
        measurementTargetSsid = target
        measurementStartTime = System.currentTimeMillis()

        startMeasurementLoop()
    }

    private fun stopAutoMeasurement() {
        isAutoMeasuring = false
        autoMeasureJob?.cancel()
        autoMeasureJob = null
        autoBtn.text = getString(R.string.auto_measurement)
        if (!isMeasuring) {
            takeMeasurementBtn.isEnabled = true
            statusText.text = getString(R.string.status_ready)
        }
    }

    private fun resetMeasurementState() {
        isMeasuring = false
        timerJob?.cancel()
        measurementTargetSsid = null
        runOnUiThread {
            if (!isAutoMeasuring) {
                takeMeasurementBtn.isEnabled = true
                statusText.text = getString(R.string.status_ready)
            }
        }
    }

    private fun hasAllPermissions(): Boolean {
        return requiredPermissions.all { perm ->
            ContextCompat.checkSelfPermission(this, perm) == PackageManager.PERMISSION_GRANTED
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        Configuration.getInstance().load(this, getSharedPreferences("osmdroid", MODE_PRIVATE))
        Configuration.getInstance().userAgentValue = packageName

        setContentView(R.layout.activity_main)

        // Room database init
        val app = application as PolarisApp
        signalDao = app.database.signalDao()
        sourcePositionDao = app.database.sourcePositionDao()

        // UI bindings

        cleanRecordsBtn = findViewById(R.id.cleanRecordsBtn)
        selectSsidBtn = findViewById(R.id.selectSsidBtn)
        takeMeasurementBtn = findViewById(R.id.takeMeasurementBtn)
        statusText = findViewById(R.id.statusText)
        statusText.text = getString(R.string.status_ssid_missing)

        drawerLayout = findViewById(R.id.drawer_layout)
        navView = findViewById(R.id.nav_view)
        toolbar = findViewById(R.id.toolbar)
        setSupportActionBar(toolbar)

        val toggle = ActionBarDrawerToggle(
            this, drawerLayout, toolbar,
            R.string.navigation_drawer_open,
            R.string.navigation_drawer_close
        )
        drawerLayout.addDrawerListener(toggle)
        toggle.syncState()

        navView.setNavigationItemSelectedListener { menuItem ->
            when (menuItem.itemId) {
                R.id.nav_debug -> {
                    val intent = Intent(this, DebugActivity::class.java)
                    startActivity(intent)
                    drawerLayout.closeDrawers()
                    true
                }
                R.id.nav_export -> {
                    val intent = Intent(this, ExportActivity::class.java)
                    startActivity(intent)
                    drawerLayout.closeDrawers()
                    true
                }
                else -> false
            }
        }

        // UI actions
        selectSsidBtn.setOnClickListener { showSsidChoiceDialog() }

        takeMeasurementBtn.setOnClickListener { startManualMeasurement() }

        autoBtn = findViewById(R.id.autoBtn)
        autoBtn.setOnClickListener {
            if (isAutoMeasuring) {
                stopAutoMeasurement()
            } else {
                promptAutoMeasurementDelay()
            }
        }

        cleanRecordsBtn.setOnClickListener {
            AlertDialog.Builder(this)
                .setTitle(getString(R.string.confirm_delete_title))
                .setMessage(getString(R.string.confirm_delete_message))
                .setPositiveButton(getString(R.string.confirm_delete_positive)) { _, _ ->
                    lifecycleScope.launch {
                        // Only delete signal records, preserve source position
                        signalDao.deleteAll()
                        clearMeasurementMarkers()
                        Toast.makeText(this@MainActivity, getString(R.string.recordings_deleted), Toast.LENGTH_SHORT).show()
                    }
                }
                .setNegativeButton(getString(R.string.confirm_delete_negative), null)
                .show()
        }

        val missing = requiredPermissions.filter { ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED }.toTypedArray()
        if (missing.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, missing, locationPermissionRequestCode)
        }

        mapView = findViewById(R.id.mapView)
        mapView.setTileSource(TileSourceFactory.MAPNIK)
        mapView.setMultiTouchControls(true)
        mapView.controller.setZoom(19.0)

        loadMeasurementMarkersFromDb()
    }

    private fun addMeasurementMarker(measurement: SignalRecord) {
        val marker = Marker(mapView).apply {
            position = GeoPoint(measurement.latitude, measurement.longitude)
            setAnchor(Marker.ANCHOR_CENTER, Marker.ANCHOR_CENTER)
            title = "${measurement.rssi} dBm - ${timeFormat.format(java.util.Date(measurement.timestamp))}"
            icon = createMeasurementIcon(android.graphics.Color.RED)
        }
        measurementMarkers.add(marker)
        mapView.overlays.add(marker)
        mapView.invalidate()
    }

    private fun createMeasurementIcon(color: Int = android.graphics.Color.RED): android.graphics.drawable.Drawable {
        val density = resources.displayMetrics.density
        val sizePx = (16 * density).toInt()
        
        return android.graphics.drawable.GradientDrawable().apply {
            shape = android.graphics.drawable.GradientDrawable.OVAL
            setColor(color)
            setStroke((2 * density).toInt(), android.graphics.Color.WHITE)
            setSize(sizePx, sizePx)
        }
    }

    private fun clearMeasurementMarkers() {
        measurementMarkers.forEach { mapView.overlays.remove(it) }
        measurementMarkers.clear()
        mapView.invalidate()
    }

    private fun loadMeasurementMarkersFromDb() {
        lifecycleScope.launch {
            val records = signalDao.getAll()
            withContext(Dispatchers.Main) {
                clearMeasurementMarkers()
                records.forEach { record ->
                    addMeasurementMarker(record)
                }
            }
        }
    }

    private fun startLocationUpdates() {
        // Cancel any existing job before starting a new one
        locationUpdatesJob?.cancel()
        // Update map location every second
        locationUpdatesJob = lifecycleScope.launch {
            while (isActive) {
                val location = LocationStream.lastKnownLocation()
                if (location != null) {
                    withContext(Dispatchers.Main) {
                        updateCurrentLocationOnMap(location.latitude, location.longitude)
                    }
                }
                delay(1000)
            }
        }
    }

    private fun updateCurrentLocationOnMap(lat: Double, lng: Double) {
        val position = GeoPoint(lat, lng)

        if (currentLocationMarker == null) {
            currentLocationMarker = Marker(mapView).apply {
                setAnchor(Marker.ANCHOR_CENTER, Marker.ANCHOR_CENTER)
                title = getString(R.string.your_location)
                icon = createMeasurementIcon(android.graphics.Color.BLUE)
            }
            mapView.overlays.add(currentLocationMarker)
        }

        currentLocationMarker?.position = position

        // Only re-center the map the first time
        if (!hasInitiallyZoomedToLocation) {
            hasInitiallyZoomedToLocation = true
            mapView.controller.setCenter(position)
        }

        mapView.invalidate()
    }

    override fun onResume() {
        super.onResume()
        mapView.onResume()

        if (hasAllPermissions()) {
            // Always ensure streams are started
            if (!LocationStream.isStarted()) {
                LocationStream.start(this)
            }
            RSSIStream.start(this)
            RSSIStream.addListener(rssiListener)
            RSSIStream.latest()?.let { handleScanBatch(it) }

            startLocationUpdates()
            loadMeasurementMarkersFromDb()
        } else {
            ActivityCompat.requestPermissions(this, requiredPermissions, locationPermissionRequestCode)
        }
    }

    override fun onPause() {
        super.onPause()
        mapView.onPause()
        locationUpdatesJob?.cancel()
        locationUpdatesJob = null
        RSSIStream.removeListener(rssiListener)
        stopAutoMeasurement()
        resetMeasurementState()
    }

    override fun onDestroy() {
        super.onDestroy()
        // Only stop streams when activity is actually being destroyed
        RSSIStream.stop()
        LocationStream.stop()
    }

    private fun updateSelectedSsidButton() {
        selectSsidBtn.text = selectedSSID ?: getString(R.string.choose_ssid)
    }

    private fun showSsidChoiceDialog() {
        // Take a snapshot of the current SSID list
        var snapshotList = ssidList.toList()

        // Create an ArrayAdapter that we can update (even if empty initially)
        val adapter = android.widget.ArrayAdapter(
            this,
            android.R.layout.simple_list_item_single_choice,
            snapshotList.toMutableList()
        )

        val dialog = AlertDialog.Builder(this)
            .setTitle(getString(R.string.choose_ssid))
            .setSingleChoiceItems(adapter, selectedSSID?.let { snapshotList.indexOf(it) } ?: -1) { dlg, which ->
                selectedSSID = adapter.getItem(which)
                updateSelectedSsidButton()
                resetMeasurementState()
                dlg.dismiss()
            }
            .setNegativeButton(android.R.string.cancel, null)
            .setNeutralButton(getString(R.string.refresh), null)
            .create()

        dialog.setOnShowListener {
            // Show empty message if no SSIDs
            if (snapshotList.isEmpty()) {
                dialog.listView.emptyView = TextView(this).apply {
                    text = getString(R.string.no_ssids_found)
                    setPadding(48, 48, 48, 48)
                    (dialog.listView.parent as? android.view.ViewGroup)?.addView(this)
                }
            }

            // Override the neutral button to NOT dismiss the dialog
            dialog.getButton(AlertDialog.BUTTON_NEUTRAL).setOnClickListener {
                lifecycleScope.launch {
                    delay(2000)

                    // Update the snapshot with fresh data
                    snapshotList = ssidList.toList()

                    // Update the adapter in place
                    withContext(Dispatchers.Main) {
                        adapter.clear()
                        adapter.addAll(snapshotList)
                        adapter.notifyDataSetChanged()

                        // Restore checked position after adapter update
                        val listView = dialog.listView
                        val newCheckedPosition = selectedSSID?.let { snapshotList.indexOf(it) } ?: -1
                        if (newCheckedPosition >= 0) {
                            // Post to ensure ListView has processed the adapter change
                            listView.post {
                                listView.setItemChecked(newCheckedPosition, true)
                                listView.setSelection(newCheckedPosition)
                            }
                        } else {
                            // Clear all checked states if selected SSID no longer exists
                            listView.post {
                                listView.clearChoices()
                            }
                        }
                    }
                }
            }
        }

        dialog.show()
    }

    private fun vibrate(durationMs: Long = 100L) {
        val vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val manager = getSystemService(VIBRATOR_MANAGER_SERVICE) as VibratorManager
            manager.defaultVibrator
        } else {
            @Suppress("DEPRECATION")
            getSystemService(VIBRATOR_SERVICE) as Vibrator
        }

        if (vibrator.hasVibrator()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                vibrator.vibrate(VibrationEffect.createOneShot(durationMs, VibrationEffect.DEFAULT_AMPLITUDE))
            } else {
                @Suppress("DEPRECATION")
                vibrator.vibrate(durationMs)
            }
        }
    }
}
