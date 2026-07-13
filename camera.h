#pragma once

#include <stddef.h>
#include <math.h>

#include "nasl/nasl.h"
#include "nasl/nasl_mat.h"

using namespace nasl;

typedef struct {
    vec3 position;
    struct {
        float yaw, pitch;
    } rotation;
    float fov;
} Camera;

vec3 camera_get_forward_vec(const Camera* cam, vec3 forward = vec3(0, 0, -1));
vec3 camera_get_right_vec(const Camera*);
mat4 camera_get_view_mat4(const Camera*, size_t, size_t);

typedef struct {
    float fly_speed, mouse_sensitivity;
    double last_mouse_x, last_mouse_y;
    unsigned mouse_was_held: 1;
} CameraFreelookState;

typedef struct {
    bool mouse_held;
    bool should_capture;
    double mouse_x, mouse_y;
    struct {
        bool forward, back, left, right;
    } keys;
} CameraInput;

bool camera_move_freelook(Camera*, CameraInput*, CameraFreelookState*, float);

inline vec2 camera_scale_from_hfov(float fov, float aspect) {
    float sw = tanf(fov * 0.5f);
    float sh = sw / aspect;
    return vec2(sw, sh);
}

typedef struct Plane {
    vec3 normal;
    float distance;

    float distance_to_point(vec3 point) {
        return dot(point, normal) + distance;
    }
} Plane;

struct Frustum {
    Plane planes[6];

    enum { LEFT, RIGHT, BOTTOM, TOP, NEAR_PLANE, FAR_PLANE };

    void extractFromMatrix(const mat4& viewProj) {
        // Left Plane
        planes[LEFT].normal = vec3(viewProj.rows[0][3] + viewProj.rows[0][0], viewProj.rows[1][3] + viewProj.rows[1][0], viewProj.rows[2][3] + viewProj.rows[2][0]);
        planes[LEFT].distance = viewProj.rows[3][3] + viewProj.rows[3][0];

        // Right Plane
        planes[RIGHT].normal = vec3(viewProj.rows[0][3] - viewProj.rows[0][0], viewProj.rows[1][3] - viewProj.rows[1][0], viewProj.rows[2][3] - viewProj.rows[2][0]);
        planes[RIGHT].distance = viewProj.rows[3][3] - viewProj.rows[3][0];

        // Bottom Plane
        planes[BOTTOM].normal = vec3(viewProj.rows[0][3] + viewProj.rows[0][1], viewProj.rows[1][3] + viewProj.rows[1][1], viewProj.rows[2][3] + viewProj.rows[2][1]);
        planes[BOTTOM].distance = viewProj.rows[3][3] + viewProj.rows[3][1];

        // Top Plane
        planes[TOP].normal = vec3(viewProj.rows[0][3] - viewProj.rows[0][1], viewProj.rows[1][3] - viewProj.rows[1][1], viewProj.rows[2][3] - viewProj.rows[2][1]);
        planes[TOP].distance = viewProj.rows[3][3] - viewProj.rows[3][1];

        // Near Plane
        planes[NEAR_PLANE].normal = vec3(viewProj.rows[0][3] + viewProj.rows[0][2], viewProj.rows[1][3] + viewProj.rows[1][2], viewProj.rows[2][3] + viewProj.rows[2][2]);
        planes[NEAR_PLANE].distance = viewProj.rows[3][3] + viewProj.rows[3][2];

        // Far Plane
        planes[FAR_PLANE].normal = vec3(viewProj.rows[0][3] - viewProj.rows[0][2], viewProj.rows[1][3] - viewProj.rows[1][2], viewProj.rows[2][3] - viewProj.rows[2][2]);
        planes[FAR_PLANE].distance = viewProj.rows[3][3] - viewProj.rows[3][2];

        // Normalize all planes
        for (auto& plane : planes) {
            float len = length(plane.normal);
            plane.normal = plane.normal / len;
            plane.distance /= len;
        }
    }

    bool isAABBInsideFrustum(vec3 min, vec3 max) {
        for (int i = 0; i < 6; ++i) {
            // Calculate the positive vertex of the AABB along the plane's normal (Projection)
            vec3 farthestPoint = {
                planes[i].normal.x < 0 ? min.x : max.x,
                planes[i].normal.y < 0 ? min.y : max.y,
                planes[i].normal.z < 0 ? min.z : max.z
            };

            // If the farthest point is strictly behind the plane, it's outside the frustum
            if (planes[i].distance_to_point(farthestPoint) < 0) {
                return false;
            }
        }
        return true;
    }
};
