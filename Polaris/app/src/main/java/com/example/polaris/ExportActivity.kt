package com.example.polaris

import android.annotation.SuppressLint
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.Dispatchers
import java.io.File
import org.json.JSONArray
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import com.google.gson.GsonBuilder
import java.net.URLEncoder
import android.app.Dialog
import androidx.fragment.app.DialogFragment
import org.osmdroid.config.Configuration
import org.osmdroid.util.GeoPoint
import org.osmdroid.views.MapView
import org.osmdroid.views.overlay.Marker
import androidx.core.content.ContextCompat

class EstimateMapDialogFragment(
    private val latitude: Double,
    private val longitude: Double
) : DialogFragment() {

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val ctx = requireContext()
        Configuration.getInstance().load(ctx, ctx.getSharedPreferences("osmdroid", 0))
        val view = requireActivity().layoutInflater.inflate(R.layout.dialog_estimate_map, null)
        val mapView = view.findViewById<MapView>(R.id.estimateMapView)
        mapView.setTileSource(org.osmdroid.tileprovider.tilesource.TileSourceFactory.MAPNIK)
        mapView.setMultiTouchControls(true)
        val point = GeoPoint(latitude, longitude)
        mapView.controller.setZoom(20.0)
        mapView.controller.setCenter(point)

        val marker = Marker(mapView).apply {
            position = GeoPoint(latitude, longitude)
            setAnchor(Marker.ANCHOR_CENTER, Marker.ANCHOR_BOTTOM)
            title = "Estimated Position"
            icon = ContextCompat.getDrawable(requireContext(), R.drawable.ic_marker_drop)
        }
        mapView.overlays.add(marker)

        return AlertDialog.Builder(ctx)
            .setTitle("Estimated Position")
            .setView(view)
            .setPositiveButton("Close", null)
            .create()
    }

    override fun onResume() {
        super.onResume()
        view?.findViewById<MapView>(R.id.estimateMapView)?.onResume()
    }
    override fun onPause() {
        view?.findViewById<MapView>(R.id.estimateMapView)?.onPause()
        super.onPause()
    }
    override fun onDestroyView() {
        view?.findViewById<MapView>(R.id.estimateMapView)?.onDetach()
        super.onDestroyView()
    }
}

@SuppressLint("HardwareIds")
class ExportActivity : AppCompatActivity() {
    private lateinit var exportBtn: Button
    private lateinit var statusText: TextView
    private lateinit var closeExportBtn: Button
    private lateinit var sendWifiBtn: Button

    private lateinit var signalDao: SignalDao
    private lateinit var sourcePositionDao: SourcePositionDao
    private var lastExportedFile: File? = null
    private val deviceID: String by lazy { android.provider.Settings.Secure.getString(contentResolver, android.provider.Settings.Secure.ANDROID_ID) ?: "unknown" }

    private var serverHost: String = "192.168.1.238"
    private var serverPort: Int = 8080

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_export)

        val app = application as PolarisApp
        signalDao = app.database.signalDao()
        sourcePositionDao = app.database.sourcePositionDao()

        val serverConfigBtn = findViewById<Button>(R.id.serverConfigBtn)
    serverConfigBtn.setOnClickListener { showServerConfigDialog() }

        exportBtn = findViewById(R.id.exportBtn)
        statusText = findViewById(R.id.statusText)
        closeExportBtn = findViewById(R.id.closeExportBtn)
        sendWifiBtn = findViewById(R.id.sendWifiBtn)

        exportBtn.setOnClickListener {
            promptFreeTextAndExport()
        }

        sendWifiBtn.setOnClickListener {
            triggerWifiSend()
        }

        closeExportBtn.setOnClickListener {
            finish()
        }

        findViewById<Button>(R.id.showServerFilesBtn).setOnClickListener {
            showServerFilesDialog()
        }
    }

    private fun showServerConfigDialog() {
        val dialogView = layoutInflater.inflate(R.layout.dialog_server_config, null)
        val hostInput = dialogView.findViewById<EditText>(R.id.dialogHostInput)
        val portInput = dialogView.findViewById<EditText>(R.id.dialogPortInput)
        hostInput.setText(serverHost)
        portInput.setText(serverPort.toString())

        AlertDialog.Builder(this)
            .setTitle("Server Config")
            .setView(dialogView)
            .setPositiveButton("OK") { _, _ ->
                val host = hostInput.text.toString().trim()
                val portVal = portInput.text.toString().toIntOrNull()
                if (host.isNotEmpty() && portVal != null && portVal in 1..65535) {
                    serverHost = host
                    serverPort = portVal
                } else {
                    Toast.makeText(this, "Invalid host or port", Toast.LENGTH_SHORT).show()
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
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
            val file = File(getExternalFilesDir(null)?.absolutePath ?: filesDir.absolutePath, fileName)
            file.writeText(jsonString)
            lastExportedFile = file
            withContext(Dispatchers.Main) {
                statusText.text = getString(R.string.exported_to, file.absolutePath)
            }
        }
    }

    private fun sanitizeFreeText(raw: String): String {
        val sanitized = raw.trim().replace(Regex("[^A-Za-z0-9_-]"), "").take(50)
        return sanitized.ifBlank {
            "noTag_" + raw.hashCode().toUInt().toString(16)
        }
    }

    private fun triggerWifiSend() {
        val host = serverHost
        val portVal = serverPort
        when {
            host.isEmpty() -> {
                Toast.makeText(this, getString(R.string.wifi_host_required), Toast.LENGTH_SHORT).show()
                return
            }
            portVal !in 1..65535 -> {
                Toast.makeText(this, getString(R.string.wifi_port_invalid), Toast.LENGTH_SHORT).show()
                return
            }
        }
        sendLastExportedFile(host, portVal)
    }

    private fun sendLastExportedFile(host: String, port: Int) {
        val file = lastExportedFile
        if (file == null || !file.exists()) {
            Toast.makeText(this, getString(R.string.wifi_send_missing_export), Toast.LENGTH_SHORT).show()
            return
        }

        lifecycleScope.launch {
            statusText.text = getString(R.string.wifi_sending_status)
            try {
                withContext(Dispatchers.IO) {
                    val url = URL("http://$host:$port/upload")
                    val conn = (url.openConnection() as HttpURLConnection).apply {
                        doOutput = true
                        requestMethod = "POST"
                        connectTimeout = 5000
                        readTimeout = 5000
                        setRequestProperty("Content-Type", "application/json")
                        setRequestProperty("X-Filename", file.name)
                    }
                    file.inputStream().use { input ->
                        conn.outputStream.use { output -> input.copyTo(output) }
                    }
                    val code = conn.responseCode
                    if (code !in 200..299) throw IOException("HTTP $code")
                    conn.disconnect()
                }
                statusText.text = getString(R.string.wifi_send_success)
                Toast.makeText(this@ExportActivity, getString(R.string.wifi_send_success), Toast.LENGTH_SHORT).show()
            } catch (ex: Exception) {
                val msg = ex.message ?: "unknown"
                statusText.text = getString(R.string.wifi_send_error, msg)
                Toast.makeText(this@ExportActivity, getString(R.string.wifi_send_error, msg), Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun showServerFilesDialog() {
        val host = serverHost
        val portVal = serverPort
        if (host.isEmpty() || portVal !in 1..65535) {
            Toast.makeText(this, getString(R.string.wifi_host_required), Toast.LENGTH_SHORT).show()
            return
        }

        lifecycleScope.launch {
            val files = fetchServerFiles(host, portVal)
            withContext(Dispatchers.Main) {
                if (files.isEmpty()) {
                    Toast.makeText(this@ExportActivity, getString(R.string.server_missing_files), Toast.LENGTH_SHORT).show()
                    return@withContext
                }
                val selected = BooleanArray(files.size)
                AlertDialog.Builder(this@ExportActivity)
                    .setTitle(getString(R.string.server_select_files))
                    .setMultiChoiceItems(files.toTypedArray(), selected) { _, which, isChecked ->
                        selected[which] = isChecked
                    }
                    .setPositiveButton(getString(R.string.server_run_algorithm)) { _, _ ->
                        val chosenFiles = files.filterIndexed { idx, _ -> selected[idx] }
                        if (chosenFiles.isNotEmpty()) {
                            runAlgorithmOnServer(host, portVal, chosenFiles)
                        }
                    }
                    .setNegativeButton(android.R.string.cancel, null)
                    .show()
            }
        }
    }

    private suspend fun fetchServerFiles(host: String, port: Int): List<String> = withContext(Dispatchers.IO) {
        try {
            val url = URL("http://$host:$port/files")
            val conn = url.openConnection() as HttpURLConnection
            conn.requestMethod = "GET"
            conn.connectTimeout = 5000
            conn.readTimeout = 5000
            val response = conn.inputStream.bufferedReader().readText()
            conn.disconnect()
            val jsonArray = JSONArray(response)
            List(jsonArray.length()) { i -> jsonArray.getString(i) }
        } catch (ex: Exception) {
            ex.printStackTrace()
            emptyList()
        }
    }

    private fun runAlgorithmOnServer(host: String, port: Int, files: List<String>) {
        lifecycleScope.launch {
            statusText.text = getString(R.string.server_running_algorithm)
            val encodedFilesParam = files.joinToString(",") { URLEncoder.encode(it, "UTF-8") }
            val url = URL("http://$host:$port/run-algorithm?files=$encodedFilesParam")
            try {
                val response = withContext(Dispatchers.IO) {
                    val conn = url.openConnection() as HttpURLConnection
                    conn.requestMethod = "GET"
                    conn.connectTimeout = 10000
                    conn.readTimeout = 60000
                    val resp = conn.inputStream.bufferedReader().readText()
                    conn.disconnect()
                    resp
                }
                statusText.text = getString(R.string.server_algorithm_result, response)
                val resultObj = org.json.JSONObject(response)
                val estLat = resultObj.getDouble("latitude")
                val estLon = resultObj.getDouble("longitude")

                EstimateMapDialogFragment(estLat, estLon).show(supportFragmentManager, "estimateMapDialog")
            } catch (ex: Exception) {
                statusText.text = getString(R.string.server_algorithm_error, ex.message ?: "unknown error")
            }
        }
    }
}