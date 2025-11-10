package com.example.testapp

import android.Manifest
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.location.Location
import android.net.wifi.WifiManager
import android.os.Bundle
import android.os.Looper
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.Spinner
import android.widget.TextView
import android.widget.AdapterView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import androidx.room.*
import com.google.android.gms.location.*
import kotlinx.coroutines.launch

// --- DATABASE SETUP ---

@Entity(tableName = "signal_records")
data class SignalRecord(
    @PrimaryKey(autoGenerate = true) val id: Int = 0,
    val latitude: Double,
    val longitude: Double,
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

@Database(entities = [SignalRecord::class], version = 1, exportSchema = false)
abstract class AppDatabase : RoomDatabase() {
    abstract fun signalDao(): SignalDao
}

// --- MAIN ACTIVITY ---

class MainActivity : AppCompatActivity() {

    private lateinit var db: AppDatabase
    private lateinit var signalDao: SignalDao

    private lateinit var fusedLocationClient: FusedLocationProviderClient
    private lateinit var locationRequest: LocationRequest
    private lateinit var locationCallback: LocationCallback

    private lateinit var coordinatesText: TextView
    private lateinit var wifiText: TextView
    private lateinit var getLocationBtn: Button
    private lateinit var viewAllBtn: Button
    private lateinit var cleanRecordsBtn: Button
    private lateinit var allRecordsText: TextView

    private lateinit var ssidSpinner: Spinner
    private lateinit var ssidAdapter: ArrayAdapter<String>
    private val ssidList = mutableListOf<String>()
    private var selectedSSID: String? = null

    private val locationPermissionRequestCode = 1001

    private val wifiScanReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            val success = intent?.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false) ?: false
            if (success) {
                // Update spinner with latest SSIDs
                val wifiManager = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
                val scanResults = wifiManager.scanResults
                ssidList.clear()
                // keep unique SSIDs, ignore empty SSIDs
                scanResults.mapNotNull { it.SSID.takeIf { ss -> ss.isNotBlank() } }
                    .distinct()
                    .sorted()
                    .forEach { ssidList.add(it) }
                runOnUiThread {
                    ssidAdapter.notifyDataSetChanged()
                    // if nothing selected yet, pick first
                    if (selectedSSID == null && ssidList.isNotEmpty()) {
                        ssidSpinner.setSelection(0)
                    }
                    // optionally refresh displayed RSSI for currently selected SSID
                    showWifiSignalStrength()
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Initialize Room
        db = Room.databaseBuilder(applicationContext, AppDatabase::class.java, "signal-db").build()
        signalDao = db.signalDao()

        // Initialize UI
        coordinatesText = findViewById(R.id.coordinatesText)
        wifiText = findViewById(R.id.wifiText)
        getLocationBtn = findViewById(R.id.getLocationBtn)
        viewAllBtn = findViewById(R.id.viewAllBtn)
        cleanRecordsBtn = findViewById(R.id.cleanRecordsBtn)
        allRecordsText = findViewById(R.id.allRecordsText)
        ssidSpinner = findViewById(R.id.ssidSpinner)

        // Location client
        fusedLocationClient = LocationServices.getFusedLocationProviderClient(this)
        locationRequest = LocationRequest.Builder(Priority.PRIORITY_HIGH_ACCURACY, 2000)
            .build()

        locationCallback = object : LocationCallback() {
            override fun onLocationResult(result: LocationResult) {
                val location: Location? = result.lastLocation
                if (location != null) {
                    val lat = location.latitude
                    val lon = location.longitude
                    coordinatesText.text = "Latitude: $lat\nLongitude: $lon"

                    val rssi = showWifiSignalStrength()
                    if (rssi != null) {
                        val record = SignalRecord(latitude = lat, longitude = lon, rssi = rssi)
                        lifecycleScope.launch {
                            signalDao.insert(record)
                        }
                    }
                } else {
                    coordinatesText.text = "Unable to get new location"
                }
            }
        }

        // Spinner adapter
        ssidAdapter = ArrayAdapter(this, android.R.layout.simple_spinner_item, ssidList)
        ssidAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        ssidSpinner.adapter = ssidAdapter
        ssidSpinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>, view: android.view.View?, position: Int, id: Long) {
                selectedSSID = if (position >= 0 && position < ssidList.size) ssidList[position] else null
                // update display to show currently selected SSID and its current RSSI (if available)
                showWifiSignalStrength()
            }

            override fun onNothingSelected(parent: AdapterView<*>) {
                selectedSSID = null
            }
        }
        // request an initial wifi scan to populate spinner
        val wifiManagerInit = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        wifiManagerInit.startScan()

        // Get location button
        getLocationBtn.setOnClickListener { getLocationUpdate() }

        // View all records button
        viewAllBtn.setOnClickListener {
            lifecycleScope.launch {
                val allRecords = signalDao.getAll()
                if (allRecords.isEmpty()) {
                    allRecordsText.text = "No records found"
                } else {
                    val builder = StringBuilder()
                    allRecords.forEach {
                        builder.append("Lat: ${it.latitude}, Lon: ${it.longitude}, RSSI: ${it.rssi}, Time: ${it.timestamp}\n")
                    }
                    allRecordsText.text = builder.toString()
                }
            }
        }

        // Clean records button
        cleanRecordsBtn.setOnClickListener {
            lifecycleScope.launch {
                signalDao.deleteAll()
                allRecordsText.text = "All records cleared"
            }
        }
    }

    override fun onResume() {
        super.onResume()
        registerReceiver(wifiScanReceiver, IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION))
        // trigger a scan when resuming so spinner is updated quickly
        val wifiManager = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        wifiManager.startScan()
    }

    override fun onPause() {
        super.onPause()
        unregisterReceiver(wifiScanReceiver)
        fusedLocationClient.removeLocationUpdates(locationCallback)
    }

    private fun getLocationUpdate() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
            != PackageManager.PERMISSION_GRANTED
        ) {
            ActivityCompat.requestPermissions(
                this,
                arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_WIFI_STATE),
                locationPermissionRequestCode
            )
        } else {
            coordinatesText.text = "Fetching new location..."
            fusedLocationClient.requestLocationUpdates(locationRequest, locationCallback, Looper.getMainLooper())
        }
    }

    private fun showWifiSignalStrength(): Int? {
        val wifiManager = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager

        // Start scan (optional - receiver will update list when scan completes)
        wifiManager.startScan()

        val scanResults = wifiManager.scanResults
        val targetNetwork = selectedSSID?.let { ssid -> scanResults.firstOrNull { it.SSID == ssid } }

        return if (targetNetwork != null) {
            val rssi = targetNetwork.level
            wifiText.text = "SSID: ${targetNetwork.SSID}\nRSSI: $rssi dBm"
            rssi
        } else {
            val display = selectedSSID?.let { "SSID $it not found nearby" } ?: "No SSID selected"
            wifiText.text = display
            null
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int, permissions: Array<out String>, grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == locationPermissionRequestCode) {
            if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                // start a wifi scan to populate spinner now that permission is granted
                val wifiManager = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
                wifiManager.startScan()
                getLocationUpdate()
            } else {
                coordinatesText.text = "Permission denied"
            }
        }
    }
}
