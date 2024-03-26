/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.hellovk

import android.annotation.SuppressLint
import android.graphics.Rect
import android.os.Build.VERSION
import android.os.Build.VERSION_CODES
import android.os.Bundle
import android.util.Log
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager.LayoutParams
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.google.androidgamesdk.GameActivity


class VulkanActivity : GameActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        hideSystemUI()
    }

    private fun hideSystemUI() {
        // This will put the game behind any cutouts and waterfalls on devices which have
        // them, so the corresponding insets will be non-zero.

        // We cannot guarantee that AndroidManifest won't be tweaked
        // and we don't want to crash if that happens so we suppress warning.
        @SuppressLint("ObsoleteSdkInt")
        if (VERSION.SDK_INT >= VERSION_CODES.P) {
            window.attributes.layoutInDisplayCutoutMode =
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
        }
        val decorView: View = window.decorView
        val controller = WindowInsetsControllerCompat(
            window,
            decorView
        )
        controller.hide(WindowInsetsCompat.Type.systemBars())
        controller.hide(WindowInsetsCompat.Type.displayCutout())
        controller.systemBarsBehavior =
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }

    override fun dispatchTouchEvent(ev: MotionEvent): Boolean {
        super.dispatchTouchEvent(ev)
        if (ev.action == MotionEvent.ACTION_DOWN) {
            samplePrintoutDisplayCutout(ev)
        }
        return true
    }

    private fun samplePrintoutDisplayCutout(ev: MotionEvent) {
        val windowInsets = this.window.decorView.rootWindowInsets
        if ( windowInsets != null ) {
            if ( windowInsets.displayCutout != null ) {
                val topRect = windowInsets.displayCutout!!.boundingRectTop
                val leftRect = windowInsets.displayCutout!!.boundingRectLeft
                val rightRect = windowInsets.displayCutout!!.boundingRectRight
                val bottomRect = windowInsets.displayCutout!!.boundingRectBottom
                Log.d("MainActivity", "displayCutout TOP: " + topRect.toString())
                Log.d("MainActivity", "displayCutout TOP LEFT: " + topRect.left + " TOP: " + topRect.top + " RIGHT: " + topRect.right + " BOTTOM: " + topRect.bottom)
                Log.d("MainActivity", "displayCutout LEFT: " + leftRect.toString())
                Log.d("MainActivity", "displayCutout RIGHT: " + rightRect.toString())
                Log.d("MainActivity", "displayCutout BOTTOM: " + bottomRect.toString())
            } else {
                Log.d("MainActivity", "displayCutout is null")
            }
        } else {
            Log.d("MainActivity", "windowInsets is null")
        }
    }

    // Filter out back button press, and handle it here after native
    // side done its processing. Application can also make a reverse JNI
    // call to onBackPressed()/finish() at the end of the KEYCODE_BACK
    // processing.
    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        var processed = super.onKeyDown(keyCode, event);
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            onBackPressed()
            processed = true
        }
        return processed
    }

    // TODO: Migrate to androidx.activity.OnBackPressedCallback.
    // onBackPressed is deprecated.
    override fun onBackPressed() {
        System.gc()
        System.exit(0)
    }

    companion object {
        init {
            System.loadLibrary("hellovkjni")
        }
    }
}