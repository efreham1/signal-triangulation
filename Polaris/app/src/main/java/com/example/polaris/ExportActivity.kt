package com.example.polaris

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

class ExportActivity : AppCompatActivity() {
    private lateinit var exportBtn: Button
    private lateinit var wifiHostInput: EditText
    private lateinit var wifiPortInput: EditText
    private lateinit var statusText: TextView
    private lateinit var closeExportBtn: Button
    private lateinit var sendWifiBtn: Button

    private lateinit var signalDao: SignalDao
    private lateinit var sourcePositionDao: SourcePositionDao
    private var lastExportedFile: File? = null
    private val deviceID: String by lazy { android.provider.Settings.Secure.getString(contentResolver, android.provider.Settings.Secure.ANDROID_ID) ?: "unknown" }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_export)

        val app = application as PolarisApp
        signalDao = app.database.signalDao()
        sourcePositionDao = app.database.sourcePositionDao()

        exportBtn = findViewById(R.id.exportBtn)
        wifiHostInput = findViewById(R.id.wifiHostInput)
        wifiPortInput = findViewById(R.id.wifiPortInput)
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
        val host = wifiHostInput.text.toString().trim()
        val portVal = wifiPortInput.text.toString().toIntOrNull()
        when {
            host.isEmpty() -> {
                Toast.makeText(this, getString(R.string.wifi_host_required), Toast.LENGTH_SHORT).show()
                return
            }
            portVal == null || portVal !in 1..65535 -> {
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
        val host = wifiHostInput.text.toString().trim()
        val portVal = wifiPortInput.text.toString().toIntOrNull()
        if (host.isEmpty() || portVal == null || portVal !in 1..65535) {
            Toast.makeText(this, getString(R.string.wifi_host_required), Toast.LENGTH_SHORT).show()
            return
        }

        lifecycleScope.launch {
            val files = fetchServerFiles(host, portVal)
            withContext(Dispatchers.Main) {
                if (files.isEmpty()) {
                    Toast.makeText(this@ExportActivity, "No files found on server.", Toast.LENGTH_SHORT).show()
                    return@withContext
                }
                val selected = BooleanArray(files.size)
                AlertDialog.Builder(this@ExportActivity)
                    .setTitle("Select files to run algorithm")
                    .setMultiChoiceItems(files.toTypedArray(), selected) { _, which, isChecked ->
                        selected[which] = isChecked
                    }
                    .setPositiveButton("Run Algorithm") { _, _ ->
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
            statusText.text = "Running algorithm..."
            val encodedFilesParam = files.joinToString(",") { URLEncoder.encode(it, "UTF-8") }
            val url = URL("http://$host:$port/run-algorithm?files=$encodedFilesParam")
            try {
                val response = withContext(Dispatchers.IO) {
                    val conn = url.openConnection() as HttpURLConnection
                    conn.requestMethod = "GET"
                    conn.connectTimeout = 5000
                    conn.readTimeout = 10000
                    val resp = conn.inputStream.bufferedReader().readText()
                    conn.disconnect()
                    resp
                }
                statusText.text = "Algorithm result:\n$response"
            } catch (ex: Exception) {
                statusText.text = "Error running algorithm: ${ex.message}"
            }
        }
    }
}