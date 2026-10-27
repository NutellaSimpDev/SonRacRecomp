package com.sonrac.game

import android.content.Context
import android.graphics.*
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import kotlin.math.*

/**
 * On-screen Xbox 360 controller for SonRac: both sticks (with L3/R3), D-pad,
 * A/B/X/Y, LB/RB, analog LT/RT, Back and Start. Multi-touch; every press gives
 * a short haptic pulse. The whole state is reported through [listener] on every
 * change and forwarded to the game as an SDL virtual gamepad.
 */
class VirtualPadView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    /** Bit index = SDL_GamepadButton. */
    enum class Button(val bit: Int, val label: String) {
        A(0, "A"), B(1, "B"), X(2, "X"), Y(3, "Y"),
        BACK(4, "Back"), START(6, "Start"),
        L3(7, "L3"), R3(8, "R3"), LB(9, "LB"), RB(10, "RB"),
        UP(11, ""), DOWN(12, ""), LEFT(13, ""), RIGHT(14, "")
    }

    fun interface Listener {
        fun onPadState(buttons: Int, lx: Float, ly: Float, rx: Float, ry: Float, lt: Float, rt: Float)
    }

    var listener: Listener? = null
    var customHapticsEnabled = true
    var padAlpha = 0.75f
        set(value) {
            field = value.coerceIn(0.1f, 1f)
            invalidate()
        }

    private val vibrator: Vibrator? = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        (context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as? VibratorManager)?.defaultVibrator
    } else {
        @Suppress("DEPRECATION")
        context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
    }

    // ---- Layout (computed in onSizeChanged) ----
    private class Circle(var cx: Float = 0f, var cy: Float = 0f, var r: Float = 0f) {
        fun contains(x: Float, y: Float, slop: Float = 1.25f) = hypot(x - cx, y - cy) <= r * slop
    }

    private class Stick {
        val base = Circle()
        var knobX = 0f
        var knobY = 0f
        var pointer = -1
        var x = 0f   // -1..1
        var y = 0f
    }

    private val leftStick = Stick()
    private val rightStick = Stick()
    private val dpad = Circle()
    private val faceButtons = mapOf(Button.A to Circle(), Button.B to Circle(), Button.X to Circle(), Button.Y to Circle())
    private val smallButtons = mapOf(
        Button.LB to RectF(), Button.RB to RectF(), Button.BACK to RectF(), Button.START to RectF(),
        Button.L3 to RectF(), Button.R3 to RectF()
    )
    private val ltRect = RectF()
    private val rtRect = RectF()

    // ---- State ----
    private var buttons = 0
    private var lt = 0f
    private var rt = 0f
    /** pointer id -> what it holds: a Button, "LT"/"RT", "DPAD", or a Stick. */
    private val pointerTargets = HashMap<Int, Any>()

    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 3f
    }
    private val text = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
    }

    private val colors = mapOf(
        Button.A to Color.parseColor("#2DB84B"), Button.B to Color.parseColor("#E5323B"),
        Button.X to Color.parseColor("#2F7BEA"), Button.Y to Color.parseColor("#F2B800")
    )
    private val baseColor = Color.parseColor("#1A2236")
    private val pressedColor = Color.parseColor("#3A8DFF")
    private val outlineColor = Color.parseColor("#7F9AC8")

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        val u = h / 100f   // layout unit: 1% of the screen height
        text.textSize = 4.2f * u

        // Triggers and bumpers along the top corners.
        ltRect.set(3 * u, 3 * u, 3 * u + 22 * u, 3 * u + 11 * u)
        smallButtons.getValue(Button.LB).set(ltRect.right + 3 * u, 3 * u, ltRect.right + 25 * u, 3 * u + 11 * u)
        rtRect.set(w - 3 * u - 22 * u, 3 * u, w - 3 * u, 3 * u + 11 * u)
        smallButtons.getValue(Button.RB).set(rtRect.left - 25 * u, 3 * u, rtRect.left - 3 * u, 3 * u + 11 * u)

        // Left side: stick, L3 above it, D-pad below-right.
        leftStick.base.apply { cx = 20 * u; cy = 52 * u; r = 16 * u }
        dpad.apply { cx = 45 * u; cy = 80 * u; r = 14 * u }
        smallButtons.getValue(Button.L3).set(36 * u, 22 * u, 50 * u, 31 * u)

        // Right side: face buttons, stick below-left, R3 above it.
        val fx = w - 20 * u
        val fy = 52 * u
        val gap = 10.5f * u
        val fr = 6.8f * u
        faceButtons.getValue(Button.Y).apply { cx = fx; cy = fy - gap; r = fr }
        faceButtons.getValue(Button.A).apply { cx = fx; cy = fy + gap; r = fr }
        faceButtons.getValue(Button.X).apply { cx = fx - gap; cy = fy; r = fr }
        faceButtons.getValue(Button.B).apply { cx = fx + gap; cy = fy; r = fr }
        rightStick.base.apply { cx = w - 45 * u; cy = 80 * u; r = 14 * u }
        smallButtons.getValue(Button.R3).set(w - 50 * u, 22 * u, w - 36 * u, 31 * u)

        // Back / Start at the bottom centre.
        smallButtons.getValue(Button.BACK).set(w / 2f - 22 * u, 87 * u, w / 2f - 3 * u, 96 * u)
        smallButtons.getValue(Button.START).set(w / 2f + 3 * u, 87 * u, w / 2f + 22 * u, 96 * u)

        resetStick(leftStick)
        resetStick(rightStick)
    }

    private fun resetStick(s: Stick) {
        s.knobX = s.base.cx
        s.knobY = s.base.cy
        s.x = 0f
        s.y = 0f
        s.pointer = -1
    }

    // ---- Drawing ----
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val alpha = (padAlpha * 255).toInt()
        val u = height / 100f

        drawTrigger(canvas, ltRect, "LT", lt, alpha, u)
        drawTrigger(canvas, rtRect, "RT", rt, alpha, u)
        for ((b, r) in smallButtons) drawPill(canvas, r, b.label, isDown(b), alpha, u)

        drawStick(canvas, leftStick, alpha, u, "L")
        drawStick(canvas, rightStick, alpha, u, "R")
        drawDpad(canvas, alpha, u)

        for ((b, c) in faceButtons) {
            val down = isDown(b)
            fill.color = colors.getValue(b)
            fill.alpha = if (down) alpha else (alpha * 0.8f).toInt()
            canvas.drawCircle(c.cx, c.cy, if (down) c.r * 0.92f else c.r, fill)
            stroke.color = Color.WHITE
            stroke.alpha = (alpha * 0.6f).toInt()
            canvas.drawCircle(c.cx, c.cy, c.r, stroke)
            text.alpha = alpha
            canvas.drawText(b.label, c.cx, c.cy + text.textSize * 0.36f, text)
        }
    }

    private fun drawPill(canvas: Canvas, r: RectF, label: String, down: Boolean, alpha: Int, u: Float) {
        fill.color = if (down) pressedColor else baseColor
        fill.alpha = (alpha * 0.85f).toInt()
        canvas.drawRoundRect(r, 3 * u, 3 * u, fill)
        stroke.color = outlineColor
        stroke.alpha = (alpha * 0.7f).toInt()
        canvas.drawRoundRect(r, 3 * u, 3 * u, stroke)
        text.alpha = alpha
        canvas.drawText(label, r.centerX(), r.centerY() + text.textSize * 0.36f, text)
    }

    private fun drawTrigger(canvas: Canvas, r: RectF, label: String, value: Float, alpha: Int, u: Float) {
        drawPill(canvas, r, "", false, alpha, u)
        if (value > 0f) {
            fill.color = pressedColor
            fill.alpha = alpha
            val filled = RectF(r.left, r.top, r.left + r.width() * value, r.bottom)
            canvas.drawRoundRect(filled, 3 * u, 3 * u, fill)
        }
        text.alpha = alpha
        canvas.drawText(label, r.centerX(), r.centerY() + text.textSize * 0.36f, text)
    }

    private fun drawStick(canvas: Canvas, s: Stick, alpha: Int, u: Float, tag: String) {
        fill.color = baseColor
        fill.alpha = (alpha * 0.6f).toInt()
        canvas.drawCircle(s.base.cx, s.base.cy, s.base.r, fill)
        stroke.color = outlineColor
        stroke.alpha = (alpha * 0.7f).toInt()
        canvas.drawCircle(s.base.cx, s.base.cy, s.base.r, stroke)
        fill.color = if (s.pointer >= 0) pressedColor else Color.parseColor("#4A5A80")
        fill.alpha = alpha
        canvas.drawCircle(s.knobX, s.knobY, s.base.r * 0.45f, fill)
        text.alpha = (alpha * 0.8f).toInt()
        canvas.drawText(tag, s.knobX, s.knobY + text.textSize * 0.36f, text)
    }

    private fun drawDpad(canvas: Canvas, alpha: Int, u: Float) {
        val c = dpad
        val arm = c.r * 0.38f
        val dirs = listOf(
            Button.UP to RectF(c.cx - arm, c.cy - c.r, c.cx + arm, c.cy - arm),
            Button.DOWN to RectF(c.cx - arm, c.cy + arm, c.cx + arm, c.cy + c.r),
            Button.LEFT to RectF(c.cx - c.r, c.cy - arm, c.cx - arm, c.cy + arm),
            Button.RIGHT to RectF(c.cx + arm, c.cy - arm, c.cx + c.r, c.cy + arm)
        )
        fill.color = baseColor
        fill.alpha = (alpha * 0.85f).toInt()
        canvas.drawRect(c.cx - arm, c.cy - arm, c.cx + arm, c.cy + arm, fill)
        for ((b, r) in dirs) {
            fill.color = if (isDown(b)) pressedColor else baseColor
            fill.alpha = (alpha * 0.85f).toInt()
            canvas.drawRoundRect(r, 1.5f * u, 1.5f * u, fill)
            stroke.color = outlineColor
            stroke.alpha = (alpha * 0.6f).toInt()
            canvas.drawRoundRect(r, 1.5f * u, 1.5f * u, stroke)
        }
    }

    private fun isDown(b: Button) = buttons and (1 shl b.bit) != 0

    // ---- Touch ----
    override fun onTouchEvent(event: MotionEvent): Boolean {
        val before = buttons
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val i = event.actionIndex
                pointerDown(event.getPointerId(i), event.getX(i), event.getY(i))
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until event.pointerCount) {
                    pointerMove(event.getPointerId(i), event.getX(i), event.getY(i))
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> pointerUp(event.getPointerId(event.actionIndex))
            MotionEvent.ACTION_CANCEL -> pointerTargets.keys.toList().forEach(::pointerUp)
        }
        rebuildButtons()
        if (buttons and before.inv() != 0) pulse()
        publish()
        invalidate()
        return true
    }

    private fun pointerDown(id: Int, x: Float, y: Float) {
        val target: Any? = when {
            ltRect.contains(x, y) -> "LT"
            rtRect.contains(x, y) -> "RT"
            leftStick.base.contains(x, y, 1.4f) && leftStick.pointer < 0 -> leftStick
            rightStick.base.contains(x, y, 1.4f) && rightStick.pointer < 0 -> rightStick
            dpad.contains(x, y, 1.3f) -> "DPAD"
            else -> faceButtons.entries.firstOrNull { it.value.contains(x, y) }?.key
                ?: smallButtons.entries.firstOrNull { it.value.contains(x, y) }?.key
        }
        if (target == null) return
        if (target is Stick) {
            target.pointer = id
            pulse()
        }
        if (target == "LT" || target == "RT") pulse()
        pointerTargets[id] = target
        pointerMove(id, x, y)
    }

    private fun pointerMove(id: Int, x: Float, y: Float) {
        when (val t = pointerTargets[id]) {
            is Stick -> {
                val dx = x - t.base.cx
                val dy = y - t.base.cy
                val d = hypot(dx, dy)
                val k = if (d > t.base.r) t.base.r / d else 1f
                t.knobX = t.base.cx + dx * k
                t.knobY = t.base.cy + dy * k

                val normD = d / t.base.r
                val deadzone = 0.18f
                if (normD < deadzone) {
                    t.x = 0f
                    t.y = 0f
                } else {
                    val factor = ((normD - deadzone) / (1f - deadzone)).coerceIn(0f, 1f)
                    var rawX = (dx / d) * factor
                    var rawY = (dy / d) * factor

                    // For the left stick (steering), prevent accidental down/reverse input while steering
                    if (t === leftStick && abs(rawX) > 0.25f && abs(rawY) < 0.40f) {
                        rawY = 0f
                    } else if (t === leftStick && abs(rawX) > 0.25f) {
                        val signY = if (rawY > 0f) 1f else -1f
                        rawY = signY * ((abs(rawY) - 0.40f) / 0.60f).coerceIn(0f, 1f)
                    }

                    t.x = rawX.coerceIn(-1f, 1f)
                    t.y = rawY.coerceIn(-1f, 1f)
                }
            }
            // Analog triggers: pressing further right/left in the bar pushes harder.
            "LT" -> lt = triggerValue(ltRect, x)
            "RT" -> rt = triggerValue(rtRect, x)
        }
        dpadPos[id] = x to y
    }

    private fun triggerValue(r: RectF, x: Float): Float {
        val rel = ((x - r.left) / r.width()).coerceIn(0f, 1f)
        return (0.40f + 0.60f * rel).coerceIn(0.40f, 1f)
    }

    private fun pointerUp(id: Int) {
        when (val t = pointerTargets.remove(id)) {
            is Stick -> resetStick(t)
            "LT" -> lt = 0f
            "RT" -> rt = 0f
        }
        dpadPos.remove(id)
    }

    private val dpadPos = HashMap<Int, Pair<Float, Float>>()

    private fun rebuildButtons() {
        var b = 0
        for ((id, t) in pointerTargets) {
            when (t) {
                is Button -> b = b or (1 shl t.bit)
                "DPAD" -> {
                    val (x, y) = dpadPos[id] ?: continue
                    val dx = x - dpad.cx
                    val dy = y - dpad.cy
                    val dead = dpad.r * 0.25f
                    // Eight directions: diagonals press two arrows.
                    if (dy < -dead && abs(dx) < abs(dy) * 2.4f) b = b or (1 shl Button.UP.bit)
                    if (dy > dead && abs(dx) < abs(dy) * 2.4f) b = b or (1 shl Button.DOWN.bit)
                    if (dx < -dead && abs(dy) < abs(dx) * 2.4f) b = b or (1 shl Button.LEFT.bit)
                    if (dx > dead && abs(dy) < abs(dx) * 2.4f) b = b or (1 shl Button.RIGHT.bit)
                }
            }
        }
        buttons = b
    }

    private fun publish() {
        listener?.onPadState(buttons, leftStick.x, leftStick.y, rightStick.x, rightStick.y, lt, rt)
    }

    private fun pulse() {
        if (!customHapticsEnabled) return
        val v = vibrator ?: return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            v.vibrate(VibrationEffect.createPredefined(VibrationEffect.EFFECT_TICK))
        } else {
            v.vibrate(VibrationEffect.createOneShot(15, 80))
        }
    }

    /** Releases everything, e.g. when a physical controller takes over. */
    fun releaseAll() {
        pointerTargets.clear()
        dpadPos.clear()
        resetStick(leftStick)
        resetStick(rightStick)
        lt = 0f
        rt = 0f
        buttons = 0
        publish()
        invalidate()
    }
}
