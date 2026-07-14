#ifndef FRUSTUM_H
#define FRUSTUM_H

struct Plane {
    vec3 normal;
    float distance;
};

struct Frustum {
    Plane planes[6];
};

float distance_to_point(Plane plane, vec3 point) {
    return dot(point, plane.normal) + plane.distance;
}

bool isAABBInsideFrustum(Frustum frustum, vec3 min, vec3 max) {
    for (int i = 0; i < 6; ++i) {
        // Calculate the positive vertex of the AABB along the plane's normal (Projection)
        vec3 farthestPoint = vec3(
                mix(max.x, min.x, frustum.planes[i].normal.x < 0),
                mix(max.y, min.y, frustum.planes[i].normal.y < 0),
                mix(max.z, min.z, frustum.planes[i].normal.z < 0)
        );

        // If the farthest point is strictly behind the plane, it's outside the frustum
        if (distance_to_point(frustum.planes[i], farthestPoint) < 0) {
            return false;
        }
    }
    return true;
}

#endif
