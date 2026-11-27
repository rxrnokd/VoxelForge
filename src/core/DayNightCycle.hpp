#pragma once

#include "core/Math.hpp"

namespace voxelforge {

// Everything that depends on the time of day: sun position, sky colours and lighting.
struct SkyState {
    Vec3 sunDirection;    // unit vector towards the sun (the moon is opposite)
    Vec3 lightDirection;  // towards the dominant light: sun by day, moon by night
    Vec3 zenithColor;
    Vec3 horizonColor;
    Vec3 fogColor;
    Vec3 directLight;     // colour * intensity of the sun/moon light
    Vec3 ambientLight;    // colour * intensity of the sky light
    float sunElevation;   // sine of the sun's altitude, -1..1
    float daylight;       // 0 at night, 1 in full day
    float starVisibility; // 0..1
};

// Time of day in [0, 1): 0 = sunrise (06:00), 0.25 = noon, 0.5 = sunset, 0.75 = midnight.
class DayNightCycle {
public:
    explicit DayNightCycle(float dayLengthSeconds = 600.0f, float timeOfDay = 0.1f);

    void advance(float seconds);
    void setTimeOfDay(float t);
    void setDayLength(float seconds);

    float timeOfDay() const { return m_time; }
    float dayLength() const { return m_dayLength; }
    // Wall clock representation (hours 0..23, minutes 0..59).
    int hours() const;
    int minutes() const;

    SkyState sky() const { return computeSky(m_time); }
    static SkyState computeSky(float timeOfDay);

private:
    float m_dayLength;
    float m_time;
};

} // namespace voxelforge
