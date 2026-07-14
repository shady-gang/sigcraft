#ifndef FRUSTUM_H
#define FRUSTUM_H

struct Plane {
    vec3 normal;
    float distance;
};

struct Frustum {
    Plane planes[6];
};

#endif
