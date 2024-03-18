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
import android.os.Build.VERSION
import android.os.Build.VERSION_CODES
import android.os.Bundle
import android.view.KeyEvent
import android.view.View
import android.view.WindowManager.LayoutParams
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.google.androidgamesdk.GameActivity
import androidx.core.view.DisplayCutoutCompat
import android.util.Log
import android.view.*
import androidx.core.view.*


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
            //window.attributes.layoutInDisplayCutoutMode = LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
            //window.attributes.layoutInDisplayCutoutMode = LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER
            window.attributes.layoutInDisplayCutoutMode = LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        }
        val decorView: View = window.decorView
        val controller = WindowInsetsControllerCompat(
            window,
            decorView
        )
        controller.hide(WindowInsetsCompat.Type.systemBars()) // top notification bar && bottom navigation bar
        //controller.hide(WindowInsetsCompat.Type.displayCutout()) // no effect
        controller.systemBarsBehavior =
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE

        //debugWindowInsets()

        //debugDisplayCutout()
    }

    override fun onAttachedToWindow() {
        debugWindowInsets()
        //val cutout = window.decorView.rootWindowInsets.displayCutout

        debugDisplayCutout()
    }

    private fun debugWindowInsets() {
        val windowInsets : WindowInsets = this.window.decorView.rootWindowInsets
        val displayCutout : DisplayCutout? = windowInsets.displayCutout
        if ( displayCutout == null ) {
            Log.d("INSET", "DisplayCutout is NULL")
        }
        //Log.d("INSET", "displayCutout LEFT: " + displayCutout!!.getSafeInsetLeft())

        val myCutout: DisplayCutout? = this.getWindowManager().getDefaultDisplay().getCutout()
        if ( myCutout != null ) {
            Log.d("INSET", "myCutout LEFT: " + myCutout!!.getSafeInsetLeft())
        }

        //val foundationInsets: WindowInsets = WindowInsets.Companion.displayCutout // Unresolved reference: Companion
        //if ( foundationInsets != null ) {
        //    Log.d("INSET", "foundationInsets LEFT: " + foundationInsets!!.getSafeInsetLeft())
        //}

//        val rootView = this.window.decorView.rootView
//        ViewCompat.setOnApplyWindowInsetsListener(rootView) { _, windowInsets ->
//            // Use the window insets
//            val displayCutoutCompat : DisplayCutoutCompat? = windowInsets.displayCutout
//            if ( displayCutoutCompat == null ) {
//                Log.d("INSET", "DisplayCutout is NULL")
//            }
//            Log.d("INSET", "setOnApplyWindowInsetsListener: displayCutout LEFT: " + displayCutoutCompat!!.getSafeInsetLeft().toString())
//        }

    }

    private fun debugDisplayCutout() {
//        val rootView = this.window.decorView.rootView
//        val windowInsets = ViewCompat.onApplyWindowInsets(rootView, null)
//        val displayCutout = windowInsets.displayCutout
//        if ( displayCutout == null ) {
//            Log.d("INSET", "DisplayCutout is NULL")
//        }
        //Log.d("INSET", "displayCutout LEFT: " + displayCutout!!.getSafeInsetLeft())

        // val toughLuck = WindowInsetsCompat.getDisplayCutout()
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