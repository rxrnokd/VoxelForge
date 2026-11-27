#pragma once

#include "core/Math.hpp"
#include "physics/AABB.hpp"

namespace voxelforge {

// Movement intent for one physics step, produced from keyboard state.
struct MoveInput {
    float forward = 0.0f; // -1..1 (W/S)
    float right = 0.0f;   // -1..1 (D/A)
    bool jump = false;    // held: jump / swim up / fly up
    bool descend = false; // held: fly down
    bool sprint = false;
};

// First-person player: a 0.6 x 1.8 box with gravity, jumping, swimming and an optional
// fly mode. Pure simulation code; rendering and input live elsewhere.
class Player {
public:
    static constexpr float kWidth = 0.6f;
    static constexpr float kHeight = 1.8f;
    static constexpr float kEyeHeight = 1.62f;
    static constexpr float kGravity = 28.0f;
    static constexpr float kTerminalVelocity = 60.0f;
    static constexpr float kJumpVelocity = 8.4f; // ~1.25 block jump
    static constexpr float kWalkSpeed = 4.3f;
    static constexpr float kSprintSpeed = 5.6f;
    static constexpr float kFlySpeed = 10.0f;
    static constexpr float kFlySprintSpeed = 22.0f;
    static constexpr float kSwimSpeed = 3.0f;
    static constexpr float kMaxPitch = 89.0f * kPi / 180.0f;

    void setPosition(const Vec3& feet);
    void setLook(float yawRadians, float pitchRadians);
    void addLook(float yawDelta, float pitchDelta);

    // Advances the simulation by `dt` seconds (called at a fixed rate).
    void step(const VoxelQuery& world, const MoveInput& input, float dt);

    // Feet position interpolated between the last two physics steps.
    Vec3 interpolatedPosition(float alpha) const { return lerp(m_previous, m_position, alpha); }

    const Vec3& position() const { return m_position; }
    const Vec3& velocity() const { return m_velocity; }
    Vec3 eyePosition() const { return m_position + Vec3{0.0f, kEyeHeight, 0.0f}; }
    AABB bounds() const { return AABB::fromFeet(m_position, kWidth, kHeight); }
    Vec3 forward() const { return directionFromYawPitch(m_yaw, m_pitch); }
    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }
    bool onGround() const { return m_onGround; }
    bool inWater() const { return m_inWater; }
    bool flying() const { return m_flying; }
    void setFlying(bool flying);

private:
    Vec3 m_position;
    Vec3 m_previous;
    Vec3 m_velocity;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    bool m_onGround = false;
    bool m_inWater = false;
    bool m_flying = false;
};

} // namespace voxelforge
