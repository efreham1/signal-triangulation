package com.example.polaris

import android.annotation.SuppressLint
import android.content.Intent
import android.os.Bundle
import android.widget.Button
import androidx.appcompat.app.AppCompatActivity
import android.widget.EditText
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.Dispatchers
import android.widget.TextView
import androidx.core.content.edit

class DebugActivity : AppCompatActivity() {
    private var sessionSourceName: String? = null
    private lateinit var sourceNameText: TextView
    private lateinit var sourcePositionDao: SourcePositionDao

    private val prefsName = "debug_prefs"
    private val keySourceName = "source_name"

    private val measureSourceLauncher = registerForActivityResult(
        androidx.activity.result.contract.ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == RESULT_OK) {
            // Source measurement succeeded, prompt for name
            lifecycleScope.launch {
                val pos = sourcePositionDao.get()
                promptForSourceName()
                updateSourceUi(pos)
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_debug)

        sourcePositionDao = (application as PolarisApp).database.sourcePositionDao()
        sourceNameText = findViewById(R.id.source_name_text)

        sessionSourceName = getSharedPreferences(prefsName, MODE_PRIVATE).getString(keySourceName, null)

        lifecycleScope.launch {
            val pos = sourcePositionDao.get()
            updateSourceUi(pos)
        }

        findViewById<Button>(R.id.sourceLocationBtn).setOnClickListener {
            measureSourceLauncher.launch(Intent(this, MeasureSourceActivity::class.java))
        }
        findViewById<Button>(R.id.locationAccuracyBtn).setOnClickListener {
            startActivity(Intent(this, AccuracyTestActivity::class.java))
        }
        findViewById<Button>(R.id.loadMeasurementsBtn).setOnClickListener {
            loadMeasurements()
        }
        findViewById<Button>(R.id.closeDebugBtn).setOnClickListener {
            finish()
        }
    }

    private fun formatFileDisplayName(fileName: String): String {
        // Example: 1767439085877_bad2f21127106f2a_12_18_05_test.json
        val parts = fileName.removeSuffix(".json").split("_")
        if (parts.size < 6) return fileName

        val timeStr = "${parts[2]}:${parts[3]}:${parts[4]}"
        val tag = parts.subList(5, parts.size).joinToString("_")

        return "$tag - $timeStr"
    }

    @SuppressLint("StringFormatMatches")
    private fun loadMeasurements() {
        // Prompt user that current measurements will be overwritten
        AlertDialog.Builder(this)
            .setTitle(getString(R.string.import_measurements))
            .setMessage(getString(R.string.import_measurements_warning))
            .setPositiveButton(getString(R.string.yes)) { _, _ ->
                lifecycleScope.launch {
                    try {
                        // List exported JSON files
                        val (files, displayNames) = withContext(Dispatchers.IO) {
                            val exportDir = getExternalFilesDir(null)
                            val files = exportDir?.listFiles { file -> file.extension == "json" } ?: emptyArray()
                            val fileNames = files.map { it.name }
                            val displayNames = fileNames.map { formatFileDisplayName(it) }
                            files to displayNames
                        }

                        withContext(Dispatchers.Main) {
                            if (files.isEmpty()) {
                                AlertDialog.Builder(this@DebugActivity)
                                    .setMessage(getString(R.string.no_exported_files_found))
                                    .setPositiveButton(getString(R.string.ok), null)
                                    .show()
                                return@withContext
                            }
                            AlertDialog.Builder(this@DebugActivity)
                                .setTitle(getString(R.string.select_file_to_import))
                                .setItems(displayNames.toTypedArray()) { _, which ->
                                    val selectedFile = files[which]
                                    importMeasurementsFromFile(selectedFile)
                                }
                                .setNegativeButton(getString(R.string.cancel), null)
                                .show()
                        }
                    } catch (e: Exception) {
                        withContext(Dispatchers.Main) {
                            AlertDialog.Builder(this@DebugActivity)
                                .setMessage(getString(R.string.measurements_import_failed, e.message))
                                .setPositiveButton(getString(R.string.ok), null)
                                .show()
                        }
                    }
                }
            }
            .setNegativeButton(getString(R.string.cancel), null)
            .show()
    }

    private fun importMeasurementsFromFile(file: java.io.File) {
        lifecycleScope.launch {
            try {
                val app = application as PolarisApp
                val signalDao = app.database.signalDao()

                // Clear existing measurements
                signalDao.deleteAll()

                val records: List<SignalRecord> = withContext(Dispatchers.IO) {
                    val json = file.readText()
                    val gson = com.google.gson.Gson()
                    val jsonObj = com.google.gson.JsonParser.parseString(json).asJsonObject
                    // Extract the "measurements" array
                    val measurementsJson = jsonObj.getAsJsonArray("measurements")
                    val type = com.google.gson.reflect.TypeToken.getParameterized(List::class.java, SignalRecord::class.java).type
                    gson.fromJson(measurementsJson, type)
                }

                var count = 0
                records.forEach { record ->
                    signalDao.insert(record)
                    count++
                }

                withContext(Dispatchers.Main) {
                    AlertDialog.Builder(this@DebugActivity)
                        .setMessage(getString(R.string.measurements_imported, count))
                        .setPositiveButton(getString(R.string.ok), null)
                        .show()
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    AlertDialog.Builder(this@DebugActivity)
                        .setMessage(getString(R.string.measurements_import_failed, e.message))
                        .setPositiveButton(getString(R.string.ok), null)
                        .show()
                }
            }
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

                    // Save to preferences
                    getSharedPreferences(prefsName, MODE_PRIVATE).edit {
                        putString(keySourceName, name)
                    }
                    // Refresh UI to show new name
                    lifecycleScope.launch {
                        val pos = sourcePositionDao.get()
                        updateSourceUi(pos)
                    }
                }
            }
            .setNegativeButton(getString(android.R.string.cancel), null)
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
}