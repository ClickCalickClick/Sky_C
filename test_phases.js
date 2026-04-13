const SunCalc = require('suncalc');

function getSolarPhases(lat, lon, now) {
    var thresholds = [-18, -6, -2, 2, 10, 20, 35];
    function getAlt(d) {
        return SunCalc.getPosition(d, lat, lon).altitude * 180 / Math.PI;
    }
    
    var currentPhaseId = null;
    var searchAltBack = getAlt(now);
    for (var m = 0; m <= 1440 * 2; m++) {
        var t = new Date(now.getTime() - m * 60000);
        var alt = getAlt(t);
        // We are going backward. A rising crossing going forward looks like:
        // earlier (alt) was < th, later (searchAltBack) is >= th.
        for (var k = 0; k < thresholds.length; k++) {
            var th = thresholds[k];
            // Rising crossing (going backward: earlier alt < th && later searchAltBack >= th)
            if (alt < th && searchAltBack >= th) {
                currentPhaseId = k;
                break;
            }
            // Setting crossing (going backward: earlier alt > th && later searchAltBack <= th)
            if (alt > th && searchAltBack <= th) {
                currentPhaseId = 7 + (thresholds.length - 1 - k);
                break;
            }
        }
        if (currentPhaseId !== null) break;
        searchAltBack = alt;
    }
    
    var nextPhaseId = null;
    var nextEpoch = null;
    var searchAltFwd = getAlt(now);
    for (var m = 1; m <= 1440 * 2; m++) {
        var t = new Date(now.getTime() + m * 60000);
        var alt = getAlt(t);
        
        for (var k = 0; k < thresholds.length; k++) {
            var th = thresholds[k];
            // Rising crossing
            if (searchAltFwd < th && alt >= th) {
                nextPhaseId = k;
                nextEpoch = Math.floor(t.getTime() / 1000);
                break;
            }
            // Setting crossing
            if (searchAltFwd > th && alt <= th) {
                nextPhaseId = 7 + (thresholds.length - 1 - k);
                nextEpoch = Math.floor(t.getTime() / 1000);
                break;
            }
        }
        if (nextPhaseId !== null) break;
        searchAltFwd = alt;
    }
    
    return {
        CurrentSolarPhaseId: currentPhaseId !== null ? currentPhaseId : 0,
        NextSolarPhaseId: nextPhaseId !== null ? nextPhaseId : 0,
        NextSolarPhaseEpoch: nextEpoch !== null ? nextEpoch : 0
    };
}
console.log("Summer:", getSolarPhases(41.8781, -87.6298, new Date("2026-06-21T12:00:00Z")));
console.log("Winter noon:", getSolarPhases(41.8781, -87.6298, new Date("2026-12-21T18:00:00Z"))); // solar noon roughly
console.log("Winter evening:", getSolarPhases(41.8781, -87.6298, new Date("2026-12-21T21:00:00Z"))); // sunset
