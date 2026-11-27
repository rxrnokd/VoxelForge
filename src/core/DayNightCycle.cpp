#include "core/DayNightCycle.hpp"

#include <cmath>

namespace voxelforge {
namespace {

const Vec3 kDayZenith{0.30f, 0.53f, 0.93f};
const Vec3 kDayHorizon{0.68f, 0.82f, 0.97f};
const Vec3 kNightZenith{0.008f, 0.012f, 0.04f};
const Vec3 kNightHorizon{0.035f, 0.05f, 0.10f};
const Vec3 kSunsetHorizon{0.98f, 0.52f, 0.26f};
const Vec3 kSunsetZenith{0.28f, 0.28f, 0.52f};

const Vec3 kSunColor{1.0f, 0.95f, 0.84f};
const Vec3 kSunsetSunColor{1.0f, 0.62f, 0.35f};
const Vec3 kMoonColor{0.55f, 0.62f, 0.85f};
const Vec3 kDayAmbient{0.56f, 0.60f, 0.68f};
const Vec3 kNightAmbient{0.10f, 0.11f, 0.19f};

float wrap01(float t) {
    t = std::fmod(t, 1.0f);
    return t < 0.0f ? t + 1.0f : t;
}

} // namespace

DayNightCycle::DayNightCycle(float dayLengthSeconds, float timeOfDay)
    : m_dayLength(dayLengthSeconds > 1.0f ? dayLengthSeconds : 1.0f), m_time(wrap01(timeOfDay)) {}

void DayNightCycle::advance(float seconds) { m_time = wrap01(m_time + seconds / m_dayLength); }

void DayNightCycle::setTimeOfDay(float t) { m_time = wrap01(t); }

void DayNightCycle::setDayLength(float seconds) { m_dayLength = seconds > 1.0f ? seconds : 1.0f; }

int DayNightCycle::hours() const {
    const float h = std::fmod(m_time * 24.0f + 6.0f, 24.0f);
    return static_cast<int>(h);
}

int DayNightCycle::minutes() const {
    const float h = std::fmod(m_time * 24.0f + 6.0f, 24.0f);
    return static_cast<int>((h - std::floor(h)) * 60.0f);
}

SkyState DayNightCycle::computeSky(float timeOfDay) {
    const float angle = wrap01(timeOfDay) * 2.0f * kPi;
    SkyState s;
    // The sun rises in the east (+X), culminates overhead, sets in the west; a slight
    // tilt towards +Z keeps north/south faces from being lit identically.
    s.sunDirection = normalize(Vec3{std::cos(angle), std::sin(angle), 0.28f});
    s.sunElevation = s.sunDirection.y;

    const float elevation = s.sunElevation;
    s.daylight = smoothstep(-0.14f, 0.22f, elevation);
    // Strongest when the sun is near the horizon, fading out above and below.
    const float twilight = clampf(1.0f - std::fabs(elevation) / 0.32f, 0.0f, 1.0f);
    s.starVisibility = 1.0f - smoothstep(-0.28f, 0.02f, elevation);

    s.zenithColor = lerp(lerp(kNightZenith, kDayZenith, s.daylight), kSunsetZenith, twilight * 0.35f);
    s.horizonColor = lerp(lerp(kNightHorizon, kDayHorizon, s.daylight), kSunsetHorizon, twilight * 0.75f);
    s.fogColor = s.horizonColor; // distant terrain dissolves exactly into the horizon

    if (elevation > 0.0f) {
        const float strength = smoothstep(0.0f, 0.18f, elevation);
        s.lightDirection = s.sunDirection;
        s.directLight = lerp(kSunColor, kSunsetSunColor, twilight) * (0.62f * strength);
    } else {
        const float strength = smoothstep(0.0f, 0.2f, -elevation);
        s.lightDirection = -s.sunDirection;
        s.directLight = kMoonColor * (0.22f * strength);
    }
    s.ambientLight = lerp(kNightAmbient, kDayAmbient, s.daylight) + kSunsetHorizon * (twilight * s.daylight * 0.08f);
    return s;
}

} // namespace voxelforge
