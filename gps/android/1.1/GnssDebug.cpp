/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "LocSvc_GnssDebugInterface"

#include <log/log.h>
#include <log_util.h>
#include "Gnss.h"
#include "GnssDebug.h"
#include "LocationUtil.h"

namespace android {
namespace hardware {
namespace gnss {
namespace V1_1 {
namespace implementation {

using ::android::hardware::hidl_vec;

#define GNSS_DEBUG_UNKNOWN_HORIZONTAL_ACCURACY_METERS (20000000)
#define GNSS_DEBUG_UNKNOWN_VERTICAL_ACCURACY_METERS   (20000)
#define GNSS_DEBUG_UNKNOWN_SPEED_ACCURACY_PER_SEC     (500)
#define GNSS_DEBUG_UNKNOWN_BEARING_ACCURACY_DEG       (180)

#define GNSS_DEBUG_UNKNOWN_UTC_TIME            (1483228800000LL) // 1/1/2017 00:00 GMT
#define GNSS_DEBUG_UNKNOWN_UTC_TIME_UNC_MIN    (999) // 999 ns
#define GNSS_DEBUG_UNKNOWN_UTC_TIME_UNC_MAX    (1.57783680E17) // 5 years in ns
#define GNSS_DEBUG_UNKNOWN_FREQ_UNC_NS_PER_SEC (2.0e5)  // ppm

GnssDebug::GnssDebug(Gnss* gnss) : mGnss(gnss)
{
}

/*
 * This methods requests position, time and satellite ephemeris debug information
 * from the HAL.
 *
 * @return void
*/
Return<void> GnssDebug::getDebugData(getDebugData_cb _hidl_cb)
{
    LOC_LOGD("%s]: ", __func__);

    DebugData data = { };

    if((nullptr == mGnss) || (nullptr == mGnss->getGnssInterface())){
        LOC_LOGE("GnssDebug - Null GNSS interface");
        _hidl_cb(data);
        return Void();
    }

    // get debug report snapshot via hal interface
    GnssDebugReport reports = { };
    mGnss->getGnssInterface()->getDebugReport(reports);

    // location block
    if (reports.mLocation.mValid) {
        data.position.valid = true;
        data.position.latitudeDegrees = reports.mLocation.mLocation.latitude;
        data.position.longitudeDegrees = reports.mLocation.mLocation.longitude;
        data.position.altitudeMeters = reports.mLocation.mLocation.altitude;

        data.position.speedMetersPerSec =
            (double)(reports.mLocation.mLocation.speed);
        data.position.bearingDegrees =
            (double)(reports.mLocation.mLocation.bearing);
        data.position.horizontalAccuracyMeters =
            (double)(reports.mLocation.mLocation.accuracy);
        data.position.verticalAccuracyMeters =
            reports.mLocation.verticalAccuracyMeters;
        data.position.speedAccuracyMetersPerSecond =
            reports.mLocation.speedAccuracyMetersPerSecond;
        data.position.bearingAccuracyDegrees =
            reports.mLocation.bearingAccuracyDegrees;

        timeval tv_now, tv_report;
        tv_report.tv_sec  = reports.mLocation.mUtcReported.tv_sec;
        tv_report.tv_usec = reports.mLocation.mUtcReported.tv_nsec / 1000ULL;
        gettimeofday(&tv_now, NULL);
        data.position.ageSeconds =
            (tv_now.tv_sec - tv_report.tv_sec) +
            (float)((tv_now.tv_usec - tv_report.tv_usec)) / 1000000;
    }
    else {
        data.position.valid = false;
    }

    if (data.position.horizontalAccuracyMeters <= 0 ||
        data.position.horizontalAccuracyMeters > GNSS_DEBUG_UNKNOWN_HORIZONTAL_ACCURACY_METERS) {
        data.position.horizontalAccuracyMeters = GNSS_DEBUG_UNKNOWN_HORIZONTAL_ACCURACY_METERS;
    }
    if (data.position.verticalAccuracyMeters <= 0 ||
        data.position.verticalAccuracyMeters > GNSS_DEBUG_UNKNOWN_VERTICAL_ACCURACY_METERS) {
        data.position.verticalAccuracyMeters = GNSS_DEBUG_UNKNOWN_VERTICAL_ACCURACY_METERS;
    }
    if (data.position.speedAccuracyMetersPerSecond <= 0 ||
        data.position.speedAccuracyMetersPerSecond > GNSS_DEBUG_UNKNOWN_SPEED_ACCURACY_PER_SEC) {
        data.position.speedAccuracyMetersPerSecond = GNSS_DEBUG_UNKNOWN_SPEED_ACCURACY_PER_SEC;
    }
    if (data.position.bearingAccuracyDegrees <= 0 ||
        data.position.bearingAccuracyDegrees > GNSS_DEBUG_UNKNOWN_BEARING_ACCURACY_DEG) {
        data.position.bearingAccuracyDegrees = GNSS_DEBUG_UNKNOWN_BEARING_ACCURACY_DEG;
    }

    // time block
    if (reports.mTime.mValid) {
        data.time.timeEstimate = reports.mTime.timeEstimate;
        data.time.timeUncertaintyNs = reports.mTime.timeUncertaintyNs;
        data.time.frequencyUncertaintyNsPerSec =
            reports.mTime.frequencyUncertaintyNsPerSec;
    }

    if (data.time.timeEstimate < GNSS_DEBUG_UNKNOWN_UTC_TIME) {
        data.time.timeEstimate = GNSS_DEBUG_UNKNOWN_UTC_TIME;
    }
    if (data.time.timeUncertaintyNs <= 0) {
        data.time.timeUncertaintyNs = (float)GNSS_DEBUG_UNKNOWN_UTC_TIME_UNC_MIN;
    } else if (data.time.timeUncertaintyNs > GNSS_DEBUG_UNKNOWN_UTC_TIME_UNC_MAX) {
        data.time.timeUncertaintyNs = (float)GNSS_DEBUG_UNKNOWN_UTC_TIME_UNC_MAX;
    }
    if (data.time.frequencyUncertaintyNsPerSec <= 0 ||
        data.time.frequencyUncertaintyNsPerSec > (float)GNSS_DEBUG_UNKNOWN_FREQ_UNC_NS_PER_SEC) {
        data.time.frequencyUncertaintyNsPerSec = (float)GNSS_DEBUG_UNKNOWN_FREQ_UNC_NS_PER_SEC;
    }

    // satellite data block
    SatelliteData s = { };
    std::vector<SatelliteData> s_array = { };

    for (uint32_t i=0; i<reports.mSatelliteInfo.size(); i++) {
        memset(&s, 0, sizeof(s));
        s.svid = reports.mSatelliteInfo[i].svid;
        convertGnssConstellationType(
            reports.mSatelliteInfo[i].constellation, s.constellation);
        convertGnssEphemerisType(
            reports.mSatelliteInfo[i].mEphemerisType, s.ephemerisType);
        convertGnssEphemerisSource(
            reports.mSatelliteInfo[i].mEphemerisSource, s.ephemerisSource);
        convertGnssEphemerisHealth(
            reports.mSatelliteInfo[i].mEphemerisHealth, s.ephemerisHealth);

        s.ephemerisAgeSeconds =
            reports.mSatelliteInfo[i].ephemerisAgeSeconds;
        s.serverPredictionIsAvailable =
            reports.mSatelliteInfo[i].serverPredictionIsAvailable;
        s.serverPredictionAgeSeconds =
            reports.mSatelliteInfo[i].serverPredictionAgeSeconds;

        s_array.push_back(s);
    }
    data.satelliteDataArray = s_array;

    // callback HIDL with collected debug data
    _hidl_cb(data);
    return Void();
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace gnss
}  // namespace hardware
}  // namespace android
