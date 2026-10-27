package com.sonrac.game

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.hardware.input.InputManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.view.InputDevice
import android.view.View
import android.widget.*
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SwitchCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.sonrac.game.databinding.ActivityLauncherBinding
import java.io.File
import kotlin.concurrent.thread

/**
 * Sonic & All-Stars Racing Transformed Pre-Game Launcher & Configuration Menu.
 * Features Sonic Racing aesthetics, ISO / folder game source selection,
 * graphics configuration, custom Turnip driver support, controller detection,
 * and Xbox virtual pad with haptic vibration settings.
 */
class LauncherActivity : AppCompatActivity() {

    private lateinit var binding: ActivityLauncherBinding
    private lateinit var prefs: SharedPreferences

    private var selectedGameDir: File? = null
    private var customDrivers = mutableListOf<String>()

    private val selectIsoLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            result.data?.data?.let { uri ->
                processSelectedIso(uri)
            }
        }
    }

    private val selectFolderLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            result.data?.data?.let { uri ->
                processSelectedFolder(uri)
            }
        }
    }

    private val importDriverLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            result.data?.data?.let { uri ->
                importTurnipDriver(uri)
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityLauncherBinding.inflate(layoutInflater)
        setContentView(binding.root)
        hideSystemBars()

        prefs = getSharedPreferences("sonrac_config", Context.MODE_PRIVATE)

        setupHardwareBadge()
        setupGameSource()
        setupGraphicsAndDrivers()
        setupControllersAndVirtualPad()
        setupStartButton()
        saveNativeCrashReports()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars()
    }

    private fun hideSystemBars() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    private fun spinnerAdapter(items: List<String>) =
        ArrayAdapter(this, R.layout.spinner_item, items).apply {
            setDropDownViewResource(R.layout.spinner_dropdown_item)
        }

    /**
     * Copies the system's report (tombstone) of each native crash of the game
     * process to files/crashes/, so it can be read where logcat is not
     * available (some devices ship with logging silenced).
     */
    private fun saveNativeCrashReports() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return
        try {
            val am = getSystemService(Context.ACTIVITY_SERVICE) as android.app.ActivityManager
            val dir = File(getExternalFilesDir(null), "crashes").apply { mkdirs() }
            for (info in am.getHistoricalProcessExitReasons(packageName, 0, 10)) {
                if (info.reason != android.app.ApplicationExitInfo.REASON_CRASH_NATIVE) continue
                val out = File(dir, "tombstone_${info.timestamp}.pb")
                if (out.exists()) continue
                info.traceInputStream?.use { input -> out.outputStream().use { input.copyTo(it) } }
            }
        } catch (e: Exception) {
            android.util.Log.w("SonRac", "saving crash reports failed", e)
        }
    }

    private fun setupHardwareBadge() {
        val model = Build.MODEL ?: "Android"
        val chip = if (Build.HARDWARE.contains("qcom") || Build.SOC_MODEL.contains("8750") || Build.MODEL.contains("S931")) {
            "Snapdragon 8 Elite • Adreno 830"
        } else {
            "${Build.HARDWARE} • Vulkan"
        }
        binding.txtDeviceBadge.text = chip
    }

    private fun setupGameSource() {
        val savedPath = prefs.getString("game_dir", null)
        val defaultAssetsDir = File(getExternalFilesDir(null), "assets")
        defaultAssetsDir.mkdirs()

        selectedGameDir = if (savedPath != null && File(savedPath).exists()) {
            File(savedPath)
        } else {
            defaultAssetsDir
        }

        updateGameDirStatus()

        binding.btnSelectIso.setOnClickListener {
            val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "*/*"
                putExtra(Intent.EXTRA_MIME_TYPES, arrayOf("application/x-iso9660-image", "application/octet-stream"))
            }
            selectIsoLauncher.launch(intent)
        }

        binding.btnSelectFolder.setOnClickListener {
            val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
            selectFolderLauncher.launch(intent)
        }
    }

    private fun processSelectedIso(uri: Uri) {
        val progressDialog = AlertDialog.Builder(this)
            .setTitle("Extrayendo ISO de Xbox 360")
            .setMessage("Analizando particion XDVDFS...")
            .setCancelable(false)
            .create()
        progressDialog.show()

        val destDir = File(getExternalFilesDir(null), "game_data")

        thread {
            val success = IsoExtractor.extractIso(this, uri, destDir) { percent, msg ->
                runOnUiThread {
                    progressDialog.setMessage("$msg\nProgreso: $percent%")
                }
            }
            runOnUiThread {
                progressDialog.dismiss()
                if (success) {
                    selectedGameDir = destDir
                    prefs.edit().putString("game_dir", destDir.absolutePath).apply()
                    updateGameDirStatus()
                    Toast.makeText(this, "¡Juego extraido y listo para jugar!", Toast.LENGTH_LONG).show()
                } else {
                    Toast.makeText(this, "No se pudo extraer el ISO. Asegurate de que sea un dump valido de Xbox 360.", Toast.LENGTH_LONG).show()
                }
            }
        }
    }

    private fun processSelectedFolder(uri: Uri) {
        contentResolver.takePersistableUriPermission(
            uri,
            Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
        )
        prefs.edit().putString("game_tree_uri", uri.toString()).apply()
        binding.txtGameStatus.text = "Carpeta: ${uri.lastPathSegment}"
        Toast.makeText(this, "Carpeta de juego configurada", Toast.LENGTH_SHORT).show()
    }

    private fun updateGameDirStatus() {
        val hasXex = File(selectedGameDir, "default.xex").exists()
        if (hasXex) {
            binding.txtGameStatus.text = "Listo  •  default.xex en /${selectedGameDir?.name}"
            binding.txtGameStatus.setTextColor(getColor(R.color.sonic_cyan))
        } else {
            binding.txtGameStatus.text = "Falta default.xex  •  carga un ISO o una carpeta"
            binding.txtGameStatus.setTextColor(getColor(R.color.sonic_gold))
        }
    }

    private fun setupGraphicsAndDrivers() {
        // Resolutions
        val resolutions = listOf(
            "720p  •  original Xbox 360",
            "1080p  •  recomendado",
            "1440p  •  QHD",
            "2340×1080  •  nativa S25"
        )
        binding.spnResolution.adapter = spinnerAdapter(resolutions)
        binding.spnResolution.setSelection(prefs.getInt("res_idx", 1))

        // Drivers
        refreshDriverList()

        binding.btnImportDriver.setOnClickListener {
            val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "application/zip"
            }
            importDriverLauncher.launch(intent)
        }

        binding.swVsync.isChecked = prefs.getBoolean("vsync", true)
    }

    private fun refreshDriverList() {
        customDrivers.clear()
        customDrivers.add("Sistema (Adreno)")

        val driversDir = File(getExternalFilesDir(null), "custom_drivers")
        driversDir.mkdirs()
        driversDir.listFiles()?.forEach { file ->
            if (file.isDirectory || file.name.endsWith(".so") || file.name.endsWith(".zip")) {
                customDrivers.add("Turnip: ${file.nameWithoutExtension}")
            }
        }

        binding.spnDriver.adapter = spinnerAdapter(customDrivers)
        binding.spnDriver.setSelection(prefs.getInt("driver_idx", 0).coerceIn(0, customDrivers.size - 1))
    }

    private fun importTurnipDriver(uri: Uri) {
        val driversDir = File(getExternalFilesDir(null), "custom_drivers")
        val fileName = uri.lastPathSegment ?: "turnip_driver.zip"
        val destFile = File(driversDir, fileName)

        try {
            contentResolver.openInputStream(uri)?.use { input ->
                destFile.outputStream().use { output ->
                    input.copyTo(output)
                }
            }
            Toast.makeText(this, "Driver importado exitosamente.", Toast.LENGTH_SHORT).show()
            refreshDriverList()
        } catch (e: Exception) {
            Toast.makeText(this, "Error al importar driver: ${e.message}", Toast.LENGTH_LONG).show()
        }
    }

    private fun setupControllersAndVirtualPad() {
        // Physical Gamepad Detection
        val inputManager = getSystemService(Context.INPUT_SERVICE) as InputManager
        val deviceIds = inputManager.inputDeviceIds
        var gamepadsFound = 0
        for (id in deviceIds) {
            val dev = inputManager.getInputDevice(id)
            if (dev != null && (dev.sources and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
                dev.sources and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK)) {
                gamepadsFound++
            }
        }
        binding.txtControllersDetected.text = if (gamepadsFound == 1) "1 mando" else "$gamepadsFound mandos"

        binding.btnConfigureGamepad.setOnClickListener {
            AlertDialog.Builder(this)
                .setTitle("Configuracion de Mandos Fisicos")
                .setMessage("Mandos compatibles detectados: $gamepadsFound.\n\nCompatibles con Bluetooth y USB:\n• Mando Xbox Series / One\n• PlayStation DualSense / DualShock\n• Mandos telescopicos Razer Kishi / Gamesir.\n\nLos botones se asignan automaticamente al layout de Xbox 360.")
                .setPositiveButton("Aceptar", null)
                .show()
        }

        // Virtual Touch Pad Settings
        binding.swVirtualPad.isChecked = prefs.getBoolean("virtual_pad_enabled", true)
        binding.swHapticVibration.isChecked = prefs.getBoolean("haptic_vibration", true)
        binding.sbPadOpacity.progress = prefs.getInt("pad_opacity", 75)
    }

    private fun setupStartButton() {
        binding.btnStartGame.setOnClickListener {
            // Save preferences
            prefs.edit().apply {
                putInt("res_idx", binding.spnResolution.selectedItemPosition)
                putInt("driver_idx", binding.spnDriver.selectedItemPosition)
                putBoolean("vsync", binding.swVsync.isChecked)
                putBoolean("virtual_pad_enabled", binding.swVirtualPad.isChecked)
                putBoolean("haptic_vibration", binding.swHapticVibration.isChecked)
                putInt("pad_opacity", binding.sbPadOpacity.progress)
                apply()
            }

            // Launch GameActivity
            val intent = Intent(this, GameActivity::class.java).apply {
                putExtra("game_dir", selectedGameDir?.absolutePath)
                putExtra("res_idx", binding.spnResolution.selectedItemPosition)
                putExtra("virtual_pad_enabled", binding.swVirtualPad.isChecked)
                putExtra("haptic_vibration", binding.swHapticVibration.isChecked)
                putExtra("pad_opacity", binding.sbPadOpacity.progress / 100f)
            }
            startActivity(intent)
        }
    }
}
