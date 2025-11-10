package com.example.testapp

import android.Manifest
import android.content.Context
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
import com.google.android.gms.location.*

class MainActivity : AppCompatActivity() {

    private lateinit var fusedLocationClient: FusedLocationProviderClient
    private lateinit var locationRequest: LocationRequest
    private lateinit var locationCallback: LocationCallback

    private lateinit var coordinatesText: TextView
    private lateinit var wifiText: TextView
    private lateinit var getLocationBtn: Button

    private val locationPermissionRequestCode = 1001
    private val targetSSID = "Forslund_5G" // replace with your Wi-Fi network name

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        coordinatesText = findViewById(R.id.coordinatesText)
        wifiText = findViewById(R.id.wifiText)
        getLocationBtn = findViewById(R.id.getLocationBtn)
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
                    showWifiSignalStrength()
                } else {
                    coordinatesText.text = "Unable to get new location"
                }
            }
        }

        getLocationBtn.setOnClickListener {
            getLocationUpdate()
        }
    }

    private fun getLocationUpdate() {
        if (ContextCompat.checkSelfPermission(
                this,
                Manifest.permission.ACCESS_FINE_LOCATION
            ) != PackageManager.PERMISSION_GRANTED
        ) {
            ActivityCompat.requestPermissions(
                this,
                arrayOf(
                    Manifest.permission.ACCESS_FINE_LOCATION,
                    Manifest.permission.ACCESS_WIFI_STATE
                ),
                locationPermissionRequestCode
            )
        } else {
            coordinatesText.text = "Fetching new location..."
            fusedLocationClient.requestLocationUpdates(
                locationRequest,
                locationCallback,
                Looper.getMainLooper()
            )
        }
    }

    private fun showWifiSignalStrength() {
        val wifiManager = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        val wifiInfo = wifiManager.connectionInfo

        if (wifiInfo != null && wifiInfo.ssid.replace("\"", "") == targetSSID) {
            val rssi = wifiInfo.rssi // in dBm
            wifiText.text = "Connected to $targetSSID\nRSSI: $rssi dBm"
        } else {
            wifiText.text = "Not connected to $targetSSID"
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
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

    override fun onPause() {
        super.onPause()
        fusedLocationClient.removeLocationUpdates(locationCallback)
    }
}
