#ifndef HEATMAP_H
#define HEATMAP_H

float heat_temps[5] = {
1, 2, 4, 8, 16
};

vec3 heat_colors[6] = {
vec3(0.0, 0.0, 1.0),
vec3(0.0, 1.0, 1.0),
vec3(1.0, 1.0, 0.0),
vec3(1.0, 0.75, 0.0),
vec3(1.0, 0.5, 0.0),
vec3(1.0, 0.0, 0.0)
};

vec3 heatmap(float lod) {
    float prev_temp = 0;
    for (int i = 0; i < 5; i++) {
        float range = heat_temps[i] - prev_temp;
        if (lod < heat_temps[i]) {
            float factor = (lod - prev_temp) / range;
            return mix(heat_colors[i], heat_colors[i + 1], factor);
        }
        prev_temp = heat_temps[i];
    }
    return heat_colors[5];
}

#endif
