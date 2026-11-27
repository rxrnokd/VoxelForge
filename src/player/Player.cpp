#include "player/Player.hpp"

#include "physics/Collision.hpp"

#include <cmath>

namespace voxelforge {
namespace {

// Moves `current` towards `target` by at most `maxDelta`.
float approach(float current, float target, float maxDelta) {
    const float diff = target - current;
    if (std::fabs(diff) <= maxDelta) {
        return target;
    }
    return current + (diff > 0.0f ? maxDelta : -maxDelta);
}

bool touchesWater(const VoxelQuery& world, const AABB& box) {
    const int x0 = fastFloor(box.min.x);
    const int x1 = fastFloor(box.max.x);
    const int z0 = fastFloor(box.min.z);
    const int z1 = fastFloor(box.max.z);
    // Sample the lower half of the body: swimming starts once you are waist deep.
    const int y0 = fastFloor(box.min.y + 0.1f);
    const int y1 = fastFloor(box.min.y + Player::kHeight * 0.5f);
    for (int y = y0; y <= y1; ++y)
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x)
                if (world.blockAt(x, y, z) == BlockType::Water) return true;
    return false;
}

} // namespace

void Player::setPosition(const Vec3& feet) {
    m_position = feet;
    m_previous = feet;
    m_velocity = {};
}

void Player::setLook(float yawRadians, float pitchRadians) {
    m_yaw = std::remainder(yawRadians, 2.0f * kPi);
    m_pitch = clampf(pitchRadians, -kMaxPitch, kMaxPitch);
}

void Player::addLook(float yawDelta, float pitchDelta) { setLook(m_yaw + yawDelta, m_pitch + pitchDelta); }

void Player::setFlying(bool flying) {
    m_flying = flying;
    m_velocity.y = 0.0f;
}

void Player::step(const VoxelQuery& world, const MoveInput& input, float dt) {
    m_previous = m_position;
    m_inWater = touchesWater(world, bounds());

    // Desired horizontal velocity in world space from the yaw (pitch does not matter).
    const Vec3 flatForward{std::sin(m_yaw), 0.0f, -std::cos(m_yaw)};
    const Vec3 flatRight{std::cos(m_yaw), 0.0f, std::sin(m_yaw)};
    Vec3 wish = flatForward * input.forward + flatRight * input.right;
    const float wishLength = length(wish);
    if (wishLength > 1.0f) {
        wish = wish / wishLength;
    }

    float speed = input.sprint ? kSprintSpeed : kWalkSpeed;
    if (m_flying) {
        speed = input.sprint ? kFlySprintSpeed : kFlySpeed;
    } else if (m_inWater) {
        speed = kSwimSpeed;
    }
    const float accel = m_flying ? 60.0f : (m_onGround ? 50.0f : (m_inWater ? 20.0f : 14.0f));
    m_velocity.x = approach(m_velocity.x, wish.x * speed, accel * dt);
    m_velocity.z = approach(m_velocity.z, wish.z * speed, accel * dt);

    if (m_flying) {
        const float vertical = (input.jump ? 1.0f : 0.0f) - (input.descend ? 1.0f : 0.0f);
        m_velocity.y = approach(m_velocity.y, vertical * speed * 0.8f, 60.0f * dt);
    } else if (m_inWater) {
        m_velocity.y -= kGravity * 0.25f * dt;
        if (input.jump) {
            m_velocity.y = approach(m_velocity.y, kSwimSpeed, 30.0f * dt);
        }
        m_velocity.y = std::max(m_velocity.y, -kSwimSpeed);
    } else {
        if (input.jump && m_onGround) {
            m_velocity.y = kJumpVelocity;
        }
        m_velocity.y = std::max(m_velocity.y - kGravity * dt, -kTerminalVelocity);
    }

    AABB box = bounds();
    const CollisionResult hit = moveAndCollide(world, box, m_velocity * dt);
    m_position = {(box.min.x + box.max.x) * 0.5f, box.min.y, (box.min.z + box.max.z) * 0.5f};
    if (hit.hitX) m_velocity.x = 0.0f;
    if (hit.hitZ) m_velocity.z = 0.0f;
    if (hit.hitY) m_velocity.y = 0.0f;
    m_onGround = hit.landed;
}

} // namespace voxelforge
