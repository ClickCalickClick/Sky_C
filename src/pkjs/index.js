var SunCalc = require("suncalc");
var Clay = require("@rebble/clay");
var clayConfig = require("./config");

var CHICAGO = { lat: 41.8781, lon: -87.6298 };
var SETTINGS_KEY = "solar-gradient-settings-v1";
var SOLAR_CACHE_KEY = "solar-gradient-solar-cache-v1";
var WEATHER_CACHE_KEY = "solar-gradient-weather-cache-v1";
var AUTO_UPDATE_MS = 10 * 60 * 1000;
var WEATHER_UPDATE_MS = 45 * 60 * 1000;
var RETRY_MS = 5 * 60 * 1000;
var GEOCODE_TIMEOUT_MS = 4000;
var STATUS_MESSAGES_ENABLED = true;
var RELOAD_TOKEN_KEY = "solar-gradient-reload-token-v1";
var OUTBOX_RETRY_DELAY_MS = 180;
var OUTBOX_MAX_RETRIES = 4;
var DEV_SWEEP_ALT_MIN = -18;
var DEV_SWEEP_ALT_MAX = 55;
var DEV_SWEEP_DEFAULT_SECONDS = 180;

var STATUS = {
    starting: 0,
    grabbingLocation: 1,
    calculatingSun: 2,
    resolvingCity: 3,
    sendingPayload: 4,
    complete: 6
};

var STATUS_LABEL = {
    0: "Starting",
    1: "Grabbing location",
    2: "Calculating sun position",
    3: "Resolving city",
    4: "Sending solar payload",
    6: "Ready"
};

var SOURCE = {
    phone: 0,
    manual: 1,
    chicago: 2,
    cached: 3
};

var KNOWN_CITIES = [
    { name: "Davenport", lat: 41.5236, lon: -90.5776 },
    { name: "Chicago", lat: 41.8781, lon: -87.6298 },
    { name: "Des Moines", lat: 41.5868, lon: -93.6250 },
    { name: "Cedar Rapids", lat: 41.9779, lon: -91.6656 },
    { name: "Madison", lat: 43.0731, lon: -89.4012 },
    { name: "Minneapolis", lat: 44.9778, lon: -93.2650 },
    { name: "St. Louis", lat: 38.6270, lon: -90.1994 },
    { name: "Kansas City", lat: 39.0997, lon: -94.5786 },
    { name: "Omaha", lat: 41.2565, lon: -95.9345 },
    { name: "Milwaukee", lat: 43.0389, lon: -87.9065 }
];

var clay = new Clay(clayConfig, null, { autoHandleEvents: false });
var retryTimer = null;
var autoUpdateTimer = null;
var settings = loadSettings();
var reloadFaceToken = loadReloadToken();
var weatherCache = loadWeatherCache();
var outboxQueue = [];
var outboxBusy = false;
var outboxRetryTimer = null;

function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
}

function toRadians(degrees) {
    return degrees * Math.PI / 180;
}

function distanceKm(latA, lonA, latB, lonB) {
    var earthRadiusKm = 6371;
    var dLat = toRadians(latB - latA);
    var dLon = toRadians(lonB - lonA);
    var a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
        Math.cos(toRadians(latA)) * Math.cos(toRadians(latB)) *
        Math.sin(dLon / 2) * Math.sin(dLon / 2);
    var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
    return earthRadiusKm * c;
}

function nearestKnownCityName(lat, lon) {
    var bestName = "Chicago";
    var bestDistance = Infinity;
    var i;

    for (i = 0; i < KNOWN_CITIES.length; i += 1) {
        var city = KNOWN_CITIES[i];
        var km = distanceKm(lat, lon, city.lat, city.lon);
        if (km < bestDistance) {
            bestDistance = km;
            bestName = city.name;
        }
    }

    return bestName;
}

function mergeObjects(base, patch) {
    var out = {};
    var key;
    for (key in base) {
        if (Object.prototype.hasOwnProperty.call(base, key)) {
            out[key] = base[key];
        }
    }
    for (key in patch) {
        if (Object.prototype.hasOwnProperty.call(patch, key)) {
            out[key] = patch[key];
        }
    }
    return out;
}

function toBoolInt(value) {
    if (value === true || value === 1) {
        return 1;
    }

    if (value === false || value === 0 || value === null || value === undefined) {
        return 0;
    }

    if (typeof value === "string") {
        var normalized = value.trim().toLowerCase();
        if (normalized === "true" || normalized === "1" || normalized === "on" || normalized === "yes") {
            return 1;
        }
    }

    return 0;
}

function parseNumber(value, fallback) {
    var parsed = Number(value);
    if (isFinite(parsed)) {
        return parsed;
    }
    return fallback;
}

function parseDateTime(value) {
    if (value === undefined || value === null) {
        return null;
    }

    var normalized = String(value).trim();
    if (!normalized.length) {
        return null;
    }

    var parsed = new Date(normalized);
    if (!isFinite(parsed.getTime())) {
        return null;
    }

    return parsed;
}

function loadReloadToken() {
    var raw = localStorage.getItem(RELOAD_TOKEN_KEY);
    if (!raw) {
        return 0;
    }

    var parsed = Number(raw);
    if (isFinite(parsed)) {
        return parsed | 0;
    }
    return 0;
}

function nextReloadToken() {
    reloadFaceToken = (reloadFaceToken + 1) & 0x7fffffff;
    localStorage.setItem(RELOAD_TOKEN_KEY, String(reloadFaceToken));
    return reloadFaceToken;
}

function loadSettings() {
    var defaults = {
        TextOverrideMode: "0",
        MotionMode: "0",
        GradientSpread: "0",
        BatterySaveMode: false,
        TimeSizeBasalt: "1",
        TimeSizeChalk: "1",
        TimeSizeEmery: "1",
        TimeSizeGabbro: "1",
        ShowLocation: true,
        ShowAltitude: true,
        WeatherEnabled: true,
        WeatherUnitFahrenheit: true,
        WeatherDetailLevel: "1",
        ForceChicagoForTesting: false,
        CustomLocationEnabled: false,
        CustomLatitude: String(CHICAGO.lat),
        CustomLongitude: String(CHICAGO.lon),
        DevModeEnabled: false,
        DevLatitude: String(CHICAGO.lat),
        DevLongitude: String(CHICAGO.lon),
        DevDateTime: "",
        DevSweepEnabled: false,
        DevSweepCycleSeconds: String(DEV_SWEEP_DEFAULT_SECONDS),
        DevShowDebugOverlay: true,
        DebugBenchmark: false
    };

    var raw = localStorage.getItem(SETTINGS_KEY);
    if (!raw) {
        return defaults;
    }

    try {
        return mergeObjects(defaults, JSON.parse(raw));
    } catch (error) {
        console.log("[solar] failed to parse saved settings");
        return defaults;
    }
}

function saveSettings(nextSettings) {
    settings = mergeObjects(settings, nextSettings);
    localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings));
}

function loadWeatherCache() {
    var raw = localStorage.getItem(WEATHER_CACHE_KEY);
    if (!raw) {
        return null;
    }

    try {
        var parsed = JSON.parse(raw);
        if (!parsed || !parsed.payload) {
            return null;
        }
        return parsed;
    } catch (error) {
        return null;
    }
}

function saveWeatherCache(payload, lat, lon, style) {
    weatherCache = {
        fetchedAtMs: Date.now(),
        lat: lat,
        lon: lon,
        weatherUnitFahrenheit: style.WeatherUnitFahrenheit,
        payload: payload
    };
    localStorage.setItem(WEATHER_CACHE_KEY, JSON.stringify(weatherCache));
}

function clonePayload(payload) {
    return mergeObjects({}, payload || {});
}

function isAppMessageBusyError(error) {
    if (!error) {
        return false;
    }

    var msg = "";
    if (error.message !== undefined && error.message !== null) {
        msg = String(error.message).toLowerCase();
    }
    if (msg.indexOf("busy") !== -1) {
        return true;
    }

    var numericCode = Number(error.error);
    if (!isFinite(numericCode)) {
        numericCode = Number(error.code);
    }
    return numericCode === 8;
}

function scheduleOutboxFlush(delayMs) {
    if (outboxRetryTimer) {
        return;
    }

    outboxRetryTimer = setTimeout(function() {
        outboxRetryTimer = null;
        flushOutboxQueue();
    }, delayMs);
}

function flushOutboxQueue() {
    if (outboxBusy || outboxQueue.length === 0) {
        return;
    }

    var item = outboxQueue[0];
    outboxBusy = true;

    Pebble.sendAppMessage(
        item.payload,
        function() {
            outboxBusy = false;
            outboxQueue.shift();
            if (item.label) {
                console.log("[solar] sent " + item.label);
            }
            flushOutboxQueue();
        },
        function(error) {
            outboxBusy = false;

            if (isAppMessageBusyError(error) && item.retries < OUTBOX_MAX_RETRIES) {
                item.retries += 1;
                scheduleOutboxFlush(OUTBOX_RETRY_DELAY_MS * item.retries);
                return;
            }

            outboxQueue.shift();
            console.log("[solar] failed " + (item.label || "message") + ": " +
                (error && error.message ? error.message : error));
            flushOutboxQueue();
        }
    );
}

function sendAppMessage(payload, label) {
    outboxQueue.push({
        payload: clonePayload(payload),
        label: label,
        retries: 0
    });
    flushOutboxQueue();
}

function sendStatus(statusCode, progressPercent) {
    if (!STATUS_MESSAGES_ENABLED) {
        return;
    }
    console.log("[solar] stage " + statusCode + ":" + (STATUS_LABEL[statusCode] || "Unknown") + " " + (progressPercent | 0) + "%");
    sendAppMessage(
        {
            StatusCode: statusCode,
            ProgressPercent: clamp(progressPercent | 0, 0, 100)
        },
        "status"
    );
}

function normalizeStyleSettings() {
    var devDate = parseDateTime(settings.DevDateTime);
    var devLatitude = parseNumber(settings.DevLatitude, CHICAGO.lat);
    var devLongitude = parseNumber(settings.DevLongitude, CHICAGO.lon);

    return {
        TextOverrideMode: clamp(parseNumber(settings.TextOverrideMode, 0) | 0, 0, 3),
        MotionMode: clamp(parseNumber(settings.MotionMode, 0) | 0, 0, 2),
        GradientSpread: clamp(parseNumber(settings.GradientSpread, 0) | 0, 0, 3),
        BatterySaveMode: toBoolInt(settings.BatterySaveMode),
        TimeSizeBasalt: clamp(parseNumber(settings.TimeSizeBasalt, 1) | 0, 0, 2),
        TimeSizeChalk: clamp(parseNumber(settings.TimeSizeChalk, 1) | 0, 0, 2),
        TimeSizeEmery: clamp(parseNumber(settings.TimeSizeEmery, 1) | 0, 0, 2),
        TimeSizeGabbro: clamp(parseNumber(settings.TimeSizeGabbro, 1) | 0, 0, 2),
        ShowLocation: toBoolInt(settings.ShowLocation),
        ShowAltitude: toBoolInt(settings.ShowAltitude),
        WeatherEnabled: toBoolInt(settings.WeatherEnabled),
        WeatherUnitFahrenheit: toBoolInt(settings.WeatherUnitFahrenheit),
        WeatherDetailLevel: clamp(parseNumber(settings.WeatherDetailLevel, 1) | 0, 0, 2),
        CustomLocationEnabled: toBoolInt(settings.CustomLocationEnabled),
        CustomLatitudeE6: Math.round(parseNumber(settings.CustomLatitude, CHICAGO.lat) * 1000000),
        CustomLongitudeE6: Math.round(parseNumber(settings.CustomLongitude, CHICAGO.lon) * 1000000),
        DevModeEnabled: toBoolInt(settings.DevModeEnabled),
        DevLatitudeE6: Math.round(devLatitude * 1000000),
        DevLongitudeE6: Math.round(devLongitude * 1000000),
        DevReferenceEpoch: devDate ? Math.floor(devDate.getTime() / 1000) : 0,
        DevSweepEnabled: toBoolInt(settings.DevSweepEnabled),
        DevSweepCycleSeconds: clamp(parseNumber(settings.DevSweepCycleSeconds, DEV_SWEEP_DEFAULT_SECONDS) | 0, 30, 900),
        DevShowDebugOverlay: toBoolInt(settings.DevShowDebugOverlay),
        DebugBenchmark: toBoolInt(settings.DebugBenchmark)
    };
}

function sendSettingsToWatch() {
    sendAppMessage(normalizeStyleSettings(), "settings");
}

function sendReloadFaceToken(reason) {
    var token = nextReloadToken();
    sendAppMessage({ ReloadFaceToken: token }, "reload-face");
    console.log("[solar] reload token=" + token + " (" + reason + ")");
}

function cacheSolarPayload(payload) {
    localStorage.setItem(SOLAR_CACHE_KEY, JSON.stringify(payload));
}

function getCachedSolarPayload() {
    var raw = localStorage.getItem(SOLAR_CACHE_KEY);
    if (!raw) {
        return null;
    }

    try {
        return JSON.parse(raw);
    } catch (error) {
        return null;
    }
}

function sendCachedSolarIfAvailable() {
    if (!STATUS_MESSAGES_ENABLED) {
        return;
    }

    var cached = getCachedSolarPayload();
    if (!cached) {
        return;
    }

    cached.SourceCode = SOURCE.cached;
    sendStatus(STATUS.sendingPayload, 86);
    sendAppMessage(cached, "cached-solar");
}

function scheduleRetry() {
    if (retryTimer) {
        return;
    }

    retryTimer = setTimeout(function() {
        retryTimer = null;
        requestAndSendSolar("retry");
    }, RETRY_MS);
}

function resolveLocation(done) {
    var style = normalizeStyleSettings();
    if (toBoolInt(settings.ForceChicagoForTesting) === 1) {
        console.log("[solar] forcing Chicago location for testing");
        done({ lat: CHICAGO.lat, lon: CHICAGO.lon, source: SOURCE.chicago });
        return;
    }

    if (style.CustomLocationEnabled === 1) {
        console.log("[solar] using manual override location");
        done({
            lat: style.CustomLatitudeE6 / 1000000,
            lon: style.CustomLongitudeE6 / 1000000,
            source: SOURCE.manual
        });
        return;
    }

    if (!navigator.geolocation) {
        console.log("[solar] geolocation unavailable, using Chicago fallback");
        done({ lat: CHICAGO.lat, lon: CHICAGO.lon, source: SOURCE.chicago });
        return;
    }

    navigator.geolocation.getCurrentPosition(
        function(position) {
            console.log("[solar] using phone location");
            done({
                lat: position.coords.latitude,
                lon: position.coords.longitude,
                source: SOURCE.phone
            });
        },
        function(error) {
            console.log("[solar] location failed, using Chicago fallback: " + (error && error.message ? error.message : error));
            scheduleRetry();
            done({ lat: CHICAGO.lat, lon: CHICAGO.lon, source: SOURCE.chicago });
        },
        {
            enableHighAccuracy: false,
            timeout: 12000,
            maximumAge: 60 * 60 * 1000
        }
    );
}

function computeSolarPayload(lat, lon, source, when) {
    var now = when || new Date();
    var position = SunCalc.getPosition(now, lat, lon);
    var rawAzimuthDeg = (position.azimuth * 180) / Math.PI;
    var azimuthDeg = ((rawAzimuthDeg + 180) % 360 + 360) % 360;
    var gradientAngleDeg = (azimuthDeg + 180) % 360;
    var altitudeDeg = (position.altitude * 180) / Math.PI;

    return {
        SourceCode: source,
        LatitudeE6: Math.round(lat * 1000000),
        LongitudeE6: Math.round(lon * 1000000),
        AzimuthDegX100: Math.round(azimuthDeg * 100),
        AltitudeDegX100: Math.round(altitudeDeg * 100),
        GradientAngleDegX100: Math.round(gradientAngleDeg * 100),
        ComputedAtEpoch: Math.floor(now.getTime() / 1000)
    };
}

function computeSweepPayload(lat, lon, source, cycleSeconds) {
    var nowMs = Date.now();
    var clampedCycle = clamp(cycleSeconds | 0, 30, 900);
    var cycleMs = clampedCycle * 1000;
    var phase = (nowMs % cycleMs) / cycleMs;
    var altitudeDeg = DEV_SWEEP_ALT_MIN + (phase * (DEV_SWEEP_ALT_MAX - DEV_SWEEP_ALT_MIN));
    var azimuthDeg = (phase * 360 + 360) % 360;
    var gradientAngleDeg = (azimuthDeg + 180) % 360;

    return {
        SourceCode: source,
        LatitudeE6: Math.round(lat * 1000000),
        LongitudeE6: Math.round(lon * 1000000),
        AzimuthDegX100: Math.round(azimuthDeg * 100),
        AltitudeDegX100: Math.round(altitudeDeg * 100),
        GradientAngleDegX100: Math.round(gradientAngleDeg * 100),
        ComputedAtEpoch: Math.floor(nowMs / 1000)
    };
}

function normalizeCityCandidate(city) {
    if (city === undefined || city === null) {
        return "";
    }

    var normalized = String(city).trim();
    if (!normalized.length) {
        return "";
    }

    if (normalized === "Current" || normalized === "Manual" || normalized === "Cached" || normalized === "Backup") {
        return "";
    }

    return normalized;
}

function requestJson(url, done) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", url, true);
    xhr.timeout = GEOCODE_TIMEOUT_MS;
    xhr.setRequestHeader("Accept", "application/json");

    xhr.onreadystatechange = function() {
        if (xhr.readyState !== 4) {
            return;
        }

        if (xhr.status >= 200 && xhr.status < 300) {
            try {
                done(JSON.parse(xhr.responseText), "");
            } catch (error) {
                done(null, "parse");
            }
            return;
        }

        done(null, "status=" + xhr.status);
    };

    xhr.ontimeout = function() {
        done(null, "timeout");
    };

    xhr.onerror = function() {
        done(null, "network");
    };

    try {
        xhr.send();
    } catch (error) {
        done(null, "send");
    }
}

function parseWeatherEpochSeconds(timeString) {
    if (!timeString) {
        return Math.floor(Date.now() / 1000);
    }

    var parsed = new Date(String(timeString));
    if (!isFinite(parsed.getTime())) {
        return Math.floor(Date.now() / 1000);
    }
    return Math.floor(parsed.getTime() / 1000);
}

function buildWeatherPayload(current, style) {
    var temp = parseNumber(current.temperature_2m, 0);
    var cloud = clamp(Math.round(parseNumber(current.cloud_cover, 0)), 0, 100);
    var weatherCode = clamp(Math.round(parseNumber(current.weather_code, 0)), 0, 255);
    var wind = Math.max(0, parseNumber(current.wind_speed_10m, 0));
    var precip = Math.max(0, parseNumber(current.precipitation, 0));

    return {
        WeatherStatus: 0,
        WeatherTempX10: Math.round(temp * 10),
        WeatherCloudCover: cloud,
        WeatherCode: weatherCode,
        WeatherWindX10: Math.round(wind * 10),
        WeatherPrecipX100: Math.round(precip * 100),
        WeatherUpdatedEpoch: parseWeatherEpochSeconds(current.time),
        WeatherUnitFahrenheit: style.WeatherUnitFahrenheit
    };
}

function buildOpenMeteoUrl(lat, lon, style) {
    var temperatureUnit = style.WeatherUnitFahrenheit === 1 ? "fahrenheit" : "celsius";
    var windUnit = style.WeatherUnitFahrenheit === 1 ? "mph" : "kmh";
    var precipitationUnit = style.WeatherUnitFahrenheit === 1 ? "inch" : "mm";

    return "https://api.open-meteo.com/v1/forecast" +
        "?latitude=" + encodeURIComponent(String(lat)) +
        "&longitude=" + encodeURIComponent(String(lon)) +
        "&current=temperature_2m,weather_code,cloud_cover,precipitation,wind_speed_10m" +
        "&temperature_unit=" + temperatureUnit +
        "&wind_speed_unit=" + windUnit +
        "&precipitation_unit=" + precipitationUnit +
        "&timezone=auto";
}

function shouldFetchWeather(style, reason, nowMs) {
    if (style.WeatherEnabled !== 1 || style.WeatherDetailLevel === 0) {
        return false;
    }

    if (reason === "startup" || reason === "watch-request" || reason === "settings-updated") {
        return true;
    }

    if (style.BatterySaveMode === 1 && reason === "auto-refresh") {
        return false;
    }

    if (!weatherCache || !weatherCache.fetchedAtMs) {
        return true;
    }

    return (nowMs - weatherCache.fetchedAtMs) >= WEATHER_UPDATE_MS;
}

function requestAndSendWeather(location, reason, style) {
    if (style.WeatherEnabled !== 1 || style.WeatherDetailLevel === 0) {
        return;
    }

    var nowMs = Date.now();
    if (weatherCache && weatherCache.weatherUnitFahrenheit !== undefined && weatherCache.weatherUnitFahrenheit !== style.WeatherUnitFahrenheit) {
        weatherCache = null;
    }

    if (weatherCache && isFinite(weatherCache.lat) && isFinite(weatherCache.lon)) {
        var movedKm = distanceKm(location.lat, location.lon, weatherCache.lat, weatherCache.lon);
        if (movedKm >= 20) {
            weatherCache = null;
        }
    }

    if (!shouldFetchWeather(style, reason, nowMs)) {
        if (weatherCache && weatherCache.payload) {
            sendAppMessage(weatherCache.payload, "weather-cached");
        }
        return;
    }

    var url = buildOpenMeteoUrl(location.lat, location.lon, style);
    requestJson(url, function(json, errorTag) {
        if (json && json.current) {
            var payload = buildWeatherPayload(json.current, style);
            saveWeatherCache(payload, location.lat, location.lon, style);
            sendAppMessage(payload, "weather-current");
            return;
        }

        console.log("[solar] weather fetch failed" + (errorTag ? " (" + errorTag + ")" : ""));
        if (weatherCache && weatherCache.payload) {
            var stalePayload = mergeObjects(weatherCache.payload, { WeatherStatus: 1 });
            sendAppMessage(stalePayload, "weather-stale");
            return;
        }
        sendAppMessage({ WeatherStatus: 2 }, "weather-failed");
    });
}

function cityFromAddress(address) {
    if (!address) {
        return "";
    }

    return normalizeCityCandidate(
        address.city ||
        address.town ||
        address.village ||
        address.municipality ||
        address.locality ||
        address.suburb ||
        address.county ||
        address.state
    );
}

function reverseGeocodeCity(lat, lon, done, onAttempt) {
    var providers = [
        {
            label: "nominatim",
            url: "https://nominatim.openstreetmap.org/reverse?format=jsonv2&zoom=10&addressdetails=1&accept-language=en&lat=" + encodeURIComponent(String(lat)) + "&lon=" + encodeURIComponent(String(lon)),
            parse: function(json) {
                if (!json) {
                    return "";
                }
                return normalizeCityCandidate(json.name) || cityFromAddress(json.address);
            }
        },
        {
            label: "mapsco",
            url: "https://geocode.maps.co/reverse?lat=" + encodeURIComponent(String(lat)) + "&lon=" + encodeURIComponent(String(lon)),
            parse: function(json) {
                if (!json) {
                    return "";
                }
                return normalizeCityCandidate(json.display_name) || cityFromAddress(json.address);
            }
        },
        {
            label: "bigdatacloud",
            url: "https://api.bigdatacloud.net/data/reverse-geocode-client?latitude=" + encodeURIComponent(String(lat)) + "&longitude=" + encodeURIComponent(String(lon)) + "&localityLanguage=en",
            parse: function(json) {
                if (!json) {
                    return "";
                }
                return normalizeCityCandidate(json.city || json.locality || json.principalSubdivision || json.localityInfo && json.localityInfo.administrative && json.localityInfo.administrative[0] && json.localityInfo.administrative[0].name);
            }
        }
    ];

    var index = 0;

    function next() {
        if (index >= providers.length) {
            done("");
            return;
        }

        var provider = providers[index];
        if (onAttempt) {
            onAttempt(provider, index, providers.length);
        }
        index += 1;

        requestJson(provider.url, function(json, errorTag) {
            var city = provider.parse(json);
            if (city && city.length) {
                console.log("[solar] city resolved via " + provider.label + ": " + city);
                done(city);
                return;
            }

            console.log("[solar] geocode miss via " + provider.label + (errorTag ? " (" + errorTag + ")" : ""));
            next();
        });
    }

    next();
}

function resolveCityName(location, done) {
    sendStatus(STATUS.resolvingCity, 58);

    if (location.source === SOURCE.chicago) {
        sendStatus(STATUS.resolvingCity, 74);
        done("Chicago");
        return;
    }

    reverseGeocodeCity(location.lat, location.lon, function(city) {
        if (city && city.length) {
            console.log("[solar] city resolved: " + city);
            sendStatus(STATUS.resolvingCity, 76);
            done(city);
            return;
        }

        var fallbackCity = nearestKnownCityName(location.lat, location.lon);
        console.log("[solar] city fallback nearest=" + fallbackCity + " source=" + location.source);
        sendStatus(STATUS.resolvingCity, 76);
        done(fallbackCity);
    }, function(_provider, attemptIndex, attemptCount) {
        var geocodeProgress = 60 + Math.round(((attemptIndex + 1) / Math.max(attemptCount, 1)) * 14);
        sendStatus(STATUS.resolvingCity, geocodeProgress);
    });
}

function requestAndSendSolar(reason) {
    console.log("[solar] update started (" + reason + ")");
    if (reason === "startup" || reason === "watch-request" || reason === "settings-updated") {
        sendReloadFaceToken("refresh-" + reason);
    }
    sendStatus(STATUS.starting, 5);
    sendStatus(STATUS.grabbingLocation, 14);

    var style = normalizeStyleSettings();
    if (style.DevModeEnabled === 1) {
        var devLat = style.DevLatitudeE6 / 1000000;
        var devLon = style.DevLongitudeE6 / 1000000;
        var payload;

        sendStatus(STATUS.grabbingLocation, 34);
        sendStatus(STATUS.calculatingSun, 48);

        if (style.DevSweepEnabled === 1) {
            payload = computeSweepPayload(devLat, devLon, SOURCE.manual, style.DevSweepCycleSeconds);
            payload.CityName = "Dev Sweep";
        } else {
            var devWhen = style.DevReferenceEpoch > 0 ? new Date(style.DevReferenceEpoch * 1000) : new Date();
            payload = computeSolarPayload(devLat, devLon, SOURCE.manual, devWhen);
            payload.CityName = "Dev Preview";
        }

        console.log("[solar] dev payload city=" + payload.CityName + " lat=" + devLat.toFixed(4) + " lon=" + devLon.toFixed(4));
        cacheSolarPayload(payload);
        sendStatus(STATUS.sendingPayload, 88);
        sendAppMessage(payload, "solar-payload");
        sendSettingsToWatch();
        requestAndSendWeather({ lat: devLat, lon: devLon, source: SOURCE.manual }, reason, style);
        sendStatus(STATUS.complete, 100);
        return;
    }

    resolveLocation(function(location) {
        sendStatus(STATUS.grabbingLocation, 34);
        sendStatus(STATUS.calculatingSun, 48);
        var payload = computeSolarPayload(location.lat, location.lon, location.source, null);

        // Ship gradients immediately after location resolve; city label can refine a moment later.
        payload.CityName = location.source === SOURCE.chicago ? "Chicago" : nearestKnownCityName(location.lat, location.lon);
        console.log("[solar] initial payload source=" + location.source + " city=" + payload.CityName + " lat=" + location.lat.toFixed(4) + " lon=" + location.lon.toFixed(4));
        cacheSolarPayload(payload);
        sendStatus(STATUS.sendingPayload, 82);
        sendAppMessage(payload, "solar-payload-initial");
        sendSettingsToWatch();
        requestAndSendWeather(location, reason, style);
        sendStatus(STATUS.complete, 100);

        resolveCityName(location, function(cityName) {
            if (!cityName || cityName === payload.CityName) {
                return;
            }
            payload.CityName = cityName;
            console.log("[solar] refined city source=" + location.source + " city=" + cityName + " lat=" + location.lat.toFixed(4) + " lon=" + location.lon.toFixed(4));
            cacheSolarPayload(payload);
            sendAppMessage(payload, "solar-city-update");
        });
    });
}

function startSchedulers() {
    if (!autoUpdateTimer) {
        autoUpdateTimer = setInterval(function() {
            requestAndSendSolar("auto-refresh");
        }, AUTO_UPDATE_MS);
    }
}

function applyClaySettingsFromResponse(response) {
    try {
        // This internally parses the URL component and stores the flattened
        // configuration directly into localStorage 'clay-settings'.
        clay.getSettings(response);
    } catch (error) {
        console.log("[solar] failed to parse Clay response: " + (error && error.message ? error.message : error));
        return;
    }

    var parsed;
    try {
        var rawSettings = localStorage.getItem('clay-settings');
        parsed = rawSettings ? JSON.parse(rawSettings) : {};
    } catch (error) {
        parsed = {};
    }

    saveSettings(parsed);
    requestAndSendSolar("settings-updated");
}

function syncSettingsFromClayStorage() {
    var parsed;

    try {
        var rawSettings = localStorage.getItem('clay-settings');
        parsed = rawSettings ? JSON.parse(rawSettings) : {};
    } catch (error) {
        console.log("[solar] failed to read Clay storage: " + (error && error.message ? error.message : error));
        return;
    }

    saveSettings(parsed);
}

function readRefreshRequest(payload) {
    if (!payload) {
        return 0;
    }
    if (payload.RefreshRequest !== undefined) {
        return Number(payload.RefreshRequest) || 0;
    }
    if (payload.ReloadFaceToken !== undefined) {
        return 1;
    }
    return 0;
}

Pebble.addEventListener("ready", function() {
    console.log("[solar] pkjs ready");
    syncSettingsFromClayStorage();
    sendSettingsToWatch();
    // Prefer fresh phone location on launch; fallback to Chicago happens in resolveLocation.
    requestAndSendSolar("startup");
    startSchedulers();
});

Pebble.addEventListener("showConfiguration", function() {
    Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener("appmessage", function(event) {
    if (readRefreshRequest(event.payload) === 1) {
        console.log("[solar] refresh requested by watch");
        requestAndSendSolar("watch-request");
    }
});

Pebble.addEventListener("webviewclosed", function(event) {
    if (!event || !event.response || event.response === "CANCELLED") {
        return;
    }
    applyClaySettingsFromResponse(event.response);
});
