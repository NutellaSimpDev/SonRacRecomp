package com.sonrac.game

import android.content.Context
import android.hardware.input.InputManager
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.widget.Toast
import android.view.InputDevice
import android.view.ViewGroup
import android.system.Os
import android.util.Log
import android.view.WindowManager
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import org.libsdl.app.SDLActivity
import java.io.File

/**
 * Hosts the recompiled game.
 *
 * SDL owns the window, the event loop and the native thread: SDLActivity loads
 * libSDL3.so and libmain.so (recompiled game + ReXGlue runtime) and calls
 * SDL_main with [getArguments]. This class only prepares what the runtime
 * reads before that happens (data folders, the game path and log settings).
 */
class GameActivity : SDLActivity() {

    private var arguments = arrayOf<String>()
    private var pad: VirtualPadView? = null
    private var padEnabled = true

    private val inputListener = object : InputManager.InputDeviceListener {
        override fun onInputDeviceAdded(deviceId: Int) = updatePadVisibility()
        override fun onInputDeviceRemoved(deviceId: Int) = updatePadVisibility()
        override fun onInputDeviceChanged(deviceId: Int) = updatePadVisibility()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        prepareEnvironment()
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        addVirtualPad()
        startThermalGuard()
    }

    // ---- Thermal protection ----
    // The emulated Xbox 360 GPU keeps the phone's GPU fully loaded. Ask Android for
    // steady (sustained) clocks, warn at moderate heat and step aside at severe heat:
    // in the background SDL blocks the game thread, so the GPU stops working.
    private var thermalListener: PowerManager.OnThermalStatusChangedListener? = null
    private var lastThermalStatus = PowerManager.THERMAL_STATUS_NONE

    private fun startThermalGuard() {
        val power = getSystemService(Context.POWER_SERVICE) as PowerManager
        if (power.isSustainedPerformanceModeSupported) {
            window.setSustainedPerformanceMode(true)
            Log.i(TAG, "sustained performance mode on")
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return
        val listener = PowerManager.OnThermalStatusChangedListener { status -> onThermalStatus(status) }
        power.addThermalStatusListener(mainExecutor, listener)
        thermalListener = listener
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.decorView.postDelayed(headroomCheck, HEADROOM_INTERVAL_MS)
        }
    }

    // Thermal status changes arrive late (once the device already throttles). The
    // headroom forecast (1.0 = throttling starts) lets the game step aside earlier.
    private val headroomCheck = object : Runnable {
        override fun run() {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R || isFinishing) return
            val power = getSystemService(Context.POWER_SERVICE) as PowerManager
            val headroom = power.getThermalHeadroom(10)  // forecast 10 s ahead
            if (!headroom.isNaN()) {
                Log.i(TAG, "thermal headroom %.2f".format(headroom))
                if (headroom >= HEADROOM_PAUSE && hasWindowFocus()) {
                    Toast.makeText(this@GameActivity,
                        "SonRac: el teléfono está muy caliente. Pausado para que se enfríe.",
                        Toast.LENGTH_LONG).show()
                    moveTaskToBack(true)
                }
            }
            window.decorView.postDelayed(this, HEADROOM_INTERVAL_MS)
        }
    }

    private fun onThermalStatus(status: Int) {
        Log.w(TAG, "thermal status $status")
        if (status >= PowerManager.THERMAL_STATUS_SEVERE) {
            Toast.makeText(this, "SonRac: el teléfono está muy caliente. Pausado para que se enfríe.",
                Toast.LENGTH_LONG).show()
            moveTaskToBack(true)
        } else if (status >= PowerManager.THERMAL_STATUS_MODERATE && lastThermalStatus < PowerManager.THERMAL_STATUS_MODERATE) {
            Toast.makeText(this, "SonRac: el teléfono se está calentando.", Toast.LENGTH_SHORT).show()
        }
        lastThermalStatus = status
    }

    /** On-screen Xbox controller over SDL's surface; it feeds an SDL virtual gamepad. */
    private fun addVirtualPad() {
        padEnabled = intent?.getBooleanExtra("virtual_pad_enabled", true) ?: true
        if (!padEnabled) return
        val view = VirtualPadView(this).apply {
            customHapticsEnabled = intent?.getBooleanExtra("haptic_vibration", true) ?: true
            padAlpha = intent?.getFloatExtra("pad_opacity", 0.75f) ?: 0.75f
            listener = VirtualPadView.Listener { buttons, lx, ly, rx, ry, lt, rt ->
                try {
                    nativeSetPadState(buttons, lx, ly, rx, ry, lt, rt)
                } catch (e: UnsatisfiedLinkError) {
                    Log.w(TAG, "pad bridge not loaded yet", e)
                }
            }
        }
        addContentView(view, ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT))
        pad = view
        (getSystemService(Context.INPUT_SERVICE) as InputManager).registerInputDeviceListener(inputListener, null)
        updatePadVisibility()
    }

    /** The touch pad steps aside while a physical controller is connected. */
    private fun updatePadVisibility() {
        val view = pad ?: return
        val hasController = InputDevice.getDeviceIds().any { id ->
            val sources = InputDevice.getDevice(id)?.sources ?: 0
            sources and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
                sources and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
        }
        val visible = !hasController
        if ((view.visibility == android.view.View.VISIBLE) != visible) {
            if (!visible) view.releaseAll()
            view.visibility = if (visible) android.view.View.VISIBLE else android.view.View.GONE
        }
    }

    override fun onDestroy() {
        window.decorView.removeCallbacks(headroomCheck)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            thermalListener?.let { (getSystemService(Context.POWER_SERVICE) as PowerManager).removeThermalStatusListener(it) }
        }
        if (pad != null) {
            (getSystemService(Context.INPUT_SERVICE) as InputManager).unregisterInputDeviceListener(inputListener)
        }
        super.onDestroy()
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

    private fun prepareEnvironment() {
        val dataRoot = getExternalFilesDir(null) ?: filesDir
        val gameDir = intent?.getStringExtra(EXTRA_GAME_DIR)?.let(::File)
            ?.takeIf { File(it, "default.xex").isFile }
            ?: File(dataRoot, "assets")

        setenv("XDG_DATA_HOME", dataRoot.absolutePath)
        setenv("HOME", dataRoot.absolutePath)

        val userDir = File(dataRoot, "user").apply { mkdirs() }
        val cacheDir = File(dataRoot, "cache").apply { mkdirs() }

        val args = mutableListOf(
            "--game_data_root=${gameDir.absolutePath}",
            "--user_data_root=${userDir.absolutePath}",
            "--cache_root=${cacheDir.absolutePath}",
            // Xbox 360 GPU emulation (librexgpu-xenos.so, Vulkan).
            "--gpu_plugin=xenos",
            // The game reads resolved render targets back on the CPU (auto exposure);
            // without readback the 3D scene comes out blown out to white.
            "--readback_resolve=fast",
            "--vulkan_readback_resolve=true",
            // Only the small luminance resolves (64x64 down to 1x1, <= 16 KB) are read
            // on the CPU; copying all ~34 resolves per frame (up to 5 MB each) is wasted.
            "--vulkan_readback_resolve_max_bytes=65536",
            // Skinning results stay on the GPU. Reading them back made every
            // memexport draw wait for the whole GPU queue.
            "--readback_memexport=false",
            // Each guest occlusion query result waited for the whole GPU queue; answer
            // "visible" instead (lens flares may show behind objects).
            "--occlusion_query_enable=false",
            // Driver calls (command buffer replay + submit) on a worker thread while the
            // GPU thread records the next submission.
            "--vulkan_threaded_submission=true",
            // Keep the EDRAM copies between surfaces that share EDRAM: the HDR scene
            // (64bpp) reuses the G-buffer's EDRAM, and skipping the copies left it empty
            // except for the last tile column (black 3D with a strip on the right).
            "--edram_skip_overlap_transfers=false",
            // The Adreno loads 16_16_16_16 textures (the HDR scene) wrongly through the
            // 64bpb untiling path: only the last 32-pixel tile column came out right and
            // the 3D scene was white. Load norm16 as float instead (same result on PC).
            "--vulkan_norm16_textures_as_float=true",
            // The precompiled 64bpb untiling shader came out wrong on the Qualcomm driver
            // (white 3D, only the last tile column right); a runtime-compiled one instead.
            "--vulkan_native_load_64bpb=false",
            // GPU skinning (vertex shader memexport) ended the render pass twice per
            // skinned mesh - hundreds of full attachment stores/reloads per frame on
            // Adreno. Keep the pass open with a barrier inside it; that needs classic
            // render passes (with a self-dependency), not dynamic rendering.
            "--vulkan_dynamic_rendering=false",
            "--vulkan_memexport_render_pass_barriers=true",
            // No barrier between each skinning draw and the next draws of the same pass
            // (~280 per frame in the cinematics, each one drains the Adreno).
            "--vulkan_memexport_relaxed_sync=true",
            // CPU-written data (bone matrices, every frame) was uploaded with a copy that
            // ended the render pass; record those copies before the pass instead.
            "--vulkan_hoist_render_pass_uploads=true",
            // Keep the complete guest render target until the Adreno output crop is
            // understood. Scissor-union render areas are an optimization and can make
            // the scene appear clipped if any pass writes outside its tracked scissors.
            "--vulkan_shrink_render_area=false",
            // 32bpp color resolves straight from the render target image to the guest
            // texture layout (no EDRAM buffer dump first).
            "--vulkan_native_resolve=true",
            // GPU skinning point lists: keep their memory export, don't rasterize their
            // placeholder points (~58 ms of GPU per frame in the intro cinematic).
            "--vulkan_discard_memexport_points=true",
            // Native renderer N1: don't draw the scene hidden behind an opaque full-screen
            // cover (the boot logos render a whole 3D scene behind them).
            "--vulkan_skip_covered_draws=true",
            // 60 FPS: the game presents every second vblank (30 FPS on 60 Hz) and times its
            // logic in real time (Xbox Series FPS Boost runs it at 60), so raise the guest
            // vblanks to 120 Hz. At 30 FPS the Adreno clocked itself down to just fit the
            // frame (~25 ms of GPU); asked for 60 it clocks up and the same frame takes ~13.
            "--guest_vblank_multiplier=1",
            // GPU time per frame / resolves / RT transfers in the session log ([GpuTiming]),
            // to see where the frame goes without separate tests.
            "--vulkan_gpu_timing=true",
            // Deeper guest audio queue (Android default 32): at low frame rates the
            // game delivers audio in bursts, and 32 blocks ran dry (silent gaps).
            "--audio_maxqframes=64",
            // Runtime log to logcat; categories need diagnostics on or the runtime refuses to start.
            "--diagnostics=true",
            "--diagnostics_categories=logging",
            "--log_level=info",
        )
        // args.txt next to assets/: extra cvars, one per line, no rebuild needed.
        // A cvar given there replaces the default above (the parser rejects repeats).
        val extra = File(dataRoot, "args.txt").takeIf { it.isFile }?.readLines()
            ?.map { it.trim().removePrefix("﻿") }
            ?.filter { it.isNotEmpty() && !it.startsWith("#") }
            .orEmpty()
        val overridden = extra.map { it.substringBefore('=') }.toSet()
        args.removeAll { it.substringBefore('=') in overridden }
        args.addAll(extra)
        arguments = args.toTypedArray()
        Log.i(TAG, "game=$gameDir args=${arguments.joinToString(" ")}")
    }

    override fun getArguments(): Array<String> = arguments

    // SDL only reads the orientation hint from application-level metadata, so
    // force landscape here or phones held upright rotate the game to portrait.
    override fun setOrientationBis(w: Int, h: Int, resizable: Boolean, hint: String?) {
        super.setOrientationBis(w, h, resizable, "LandscapeLeft LandscapeRight")
    }

    private fun setenv(name: String, value: String) {
        try {
            Os.setenv(name, value, true)
        } catch (e: Exception) {
            Log.e(TAG, "setenv $name failed", e)
        }
    }

    companion object {
        private const val TAG = "SonRac"
        const val EXTRA_GAME_DIR = "game_dir"
        private const val HEADROOM_INTERVAL_MS = 10_000L
        /** Pause before Android's own throttling point (1.0). */
        private const val HEADROOM_PAUSE = 0.9f

        /** src/android_virtual_pad.cpp, in libmain.so. */
        @JvmStatic
        external fun nativeSetPadState(buttons: Int, lx: Float, ly: Float, rx: Float, ry: Float, lt: Float, rt: Float)
    }
}
