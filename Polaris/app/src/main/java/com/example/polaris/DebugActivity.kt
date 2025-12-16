package com.example.polaris

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

class DebugActivity : AppCompatActivity() {
    private var sessionSourceName: String? = null
    private lateinit var sourceNameText: TextView
    private lateinit var sourcePositionDao: SourcePositionDao

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

        findViewById<Button>(R.id.sourceLocationBtn).setOnClickListener {
            measureSourceLauncher.launch(Intent(this, MeasureSourceActivity::class.java))
        }
        findViewById<Button>(R.id.locationAccuracyBtn).setOnClickListener {
            startActivity(Intent(this, AccuracyTestActivity::class.java))
        }
        findViewById<Button>(R.id.closeDebugBtn).setOnClickListener {
            finish()
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