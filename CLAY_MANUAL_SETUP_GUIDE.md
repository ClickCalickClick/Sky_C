# Pebble Clay Manual Setup Guide (`autoHandleEvents: false`)

This document outlines the correct way to handle Pebble Clay settings when `autoHandleEvents: false` is required. 

## Why use manual event handling?
If your watchface needs to send complex payloads, sequentially queue multiple messages (e.g., Solar data then Weather data), or implement retry logic for `APP_MSG_BUSY` errors, you cannot let Clay automatically send the AppMessage dictionary. You must manually process the webview response and construct your own payload.

## The Pitfalls of `@rebble/clay`
When manually managing Clay, two major bugs often occur due to how the modern `@rebble/clay` library functions internally:

1. **Boot Crashes:** Calling `clay.getSettings()` with no arguments (to load cached settings on boot) throws a `TypeError` in recent versions because it attempts to run `.match()` on an `undefined` response.
2. **Storage Corruption:** By default, `clay.getSettings(response)` converts human-readable keys (e.g., `"TimeSizeBasalt"`) into their integer AppMessage equivalents (e.g., `10012`). If you save this object back into your local JS state, your string keys are lost. If you write it back to Clay using `clay.setSettings()`, the webview will fail to recognize the integer keys and completely reset the UI.

## The Correct Implementation

### 1. Initialization
Pass `autoHandleEvents: false` when creating the Clay instance so you can intercept the `webviewclosed` event.

```javascript
var Clay = require("@rebble/clay");
var clayConfig = require("./config");
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });
```

### 2. Loading Settings on Boot
Instead of relying on `clay.getSettings()`, manually parse `'clay-settings'` from localStorage. Clay keeps this updated internally.

```javascript
function syncSettingsFromClayStorage() {
    var parsed;
    try {
        // DO NOT use clay.getSettings() here, it causes a TypeError crash.
        var rawSettings = localStorage.getItem('clay-settings');
        parsed = rawSettings ? JSON.parse(rawSettings) : {};
    } catch (error) {
        console.log("Failed to read Clay storage: " + error);
        return;
    }

    // Merge into your global JS settings object
    saveSettings(parsed);
}

Pebble.addEventListener("ready", function() {
    syncSettingsFromClayStorage();
});
```

### 3. Handling the Webview Response
When the user clicks "Save", grab the response. Allow `clay.getSettings(response)` to do its internal parsing, discarding its return value, and then manually pull the string-keyed JSON from `localStorage` just like on boot.

```javascript
function applyClaySettingsFromResponse(response) {
    try {
        // Calling this triggers Clay's internal URL decoding.
        // It saves flattened, string-keyed properties directly into localStorage 'clay-settings'.
        // We do NOT use the return value because it is formatted for AppMessage (integer keys).
        clay.getSettings(response);
    } catch (error) {
        console.log("Failed to parse Clay response: " + error);
        return;
    }

    // Now safely retrieve the string-keyed properties
    var parsed;
    try {
        var rawSettings = localStorage.getItem('clay-settings');
        parsed = rawSettings ? JSON.parse(rawSettings) : {};
    } catch (error) {
        parsed = {};
    }

    // 1. Update your global JS state
    saveSettings(parsed);
    
    // 2. Dispatch your custom payloads and notify the watch C-code to refresh the UI
    requestAndSendSolar("settings-updated"); 
}

Pebble.addEventListener("webviewclosed", function(event) {
    if (!event || !event.response || event.response === "CANCELLED") {
        return;
    }
    applyClaySettingsFromResponse(event.response);
});
```

### 4. Forcing a Watch UI Redraw
When settings change—especially layout changes like font sizes—the C canvas needs to know it should discard its cached layout and redraw from scratch.

Ensure your Javascript sends down a specific key (like a `ReloadFaceToken`) to the C `InboxReceived` handler cleanly triggering a layer flush.

```javascript
// JS Side
if (reason === "settings-updated") {
    sendAppMessage({ "ReloadFaceToken": generateNewToken() });
}
```

```c
// C Side
Tuple *reload_token = dict_find(iter, MESSAGE_KEY_ReloadFaceToken);
if (reload_token) {
    if (token != s_state.last_reload_face_token) {
        s_state.last_reload_face_token = token;
        // Wipes text layers, resets sizing modes, triggers load animation
        prv_begin_loading(); 
    }
}
```
