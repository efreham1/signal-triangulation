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
import android.widget.Button
import android.widget.TextView
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

    private val locationPermissionRequestCode = 1001
    private val targetSSID = "Forslund_5G"

    private val wifiScanReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            val success = intent?.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false) ?: false
            if (success) {
                // Scan results updated, display Wi-Fi info
                showWifiSignalStrength()
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

        // Location client
        fusedLocationClient = LocationServices.getFusedLocationProviderClient(this)
        locationRequest = LocationRequest.Builder(Priority.PRIORITY_HIGH_ACCURACY, 2000)
            .setMaxUpdates(1)
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

        // Start scan
        wifiManager.startScan()

        val scanResults = wifiManager.scanResults
        val targetNetwork = scanResults.firstOrNull { it.SSID == targetSSID }

        return if (targetNetwork != null) {
            val rssi = targetNetwork.level
            wifiText.text = "SSID: $targetSSID\nRSSI: $rssi dBm"
            rssi
        } else {
            wifiText.text = "SSID $targetSSID not found nearby"
            null
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int, permissions: Array<out String>, grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == locationPermissionRequestCode) {
            if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                getLocationUpdate()
            } else {
                coordinatesText.text = "Permission denied"
            }
        }
    }
}
