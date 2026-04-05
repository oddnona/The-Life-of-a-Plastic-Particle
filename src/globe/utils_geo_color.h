#ifndef PLASTICPARTICLEVE_UTILS_GEO_COLOR_H
#define PLASTICPARTICLEVE_UTILS_GEO_COLOR_H


#pragma once
#include <osg/Vec3>
#include <osg/Vec4>
#include <optional>
#include <vector>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace pcve::util {

inline osg::Vec4 hsvToRgba(float h, float s, float v, float a=1.0f)
{
    h = h - std::floor(h); // wrap 0..1
    float r=0,g=0,b=0;

    float i = std::floor(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f*s);
    float t = v * (1.0f - (1.0f - f)*s);

    switch ((int)i % 6) {
        case 0: r=v; g=t; b=p; break;
        case 1: r=q; g=v; b=p; break;
        case 2: r=p; g=v; b=t; break;
        case 3: r=p; g=q; b=v; break;
        case 4: r=t; g=p; b=v; break;
        case 5: r=v; g=p; b=q; break;
    }
    return osg::Vec4(r,g,b,a);
}

// lon/lat -> XYZ on unit sphere
inline osg::Vec3 lonLat_toXYZ(float lon_deg, float lat_deg, float radius = 1.0f)
{
    if (lon_deg > 180.0f) lon_deg -= 360.0f;
    const float d2r = (float)(M_PI / 180.0);
    float lon = lon_deg * d2r;
    float lat = lat_deg * d2r;
    float clat = std::cos(lat);
    float x = radius * clat * std::cos(lon);
    float y = radius * clat * std::sin(lon);
    float z = radius * std::sin(lat);
    return osg::Vec3(x, y, z);
}

inline std::vector<osg::Vec3> buildSphereSamplesIntro(const std::vector<float>& lon,
                                                      const std::vector<float>& lat,
                                                      std::optional<float> fillLon,
                                                      std::optional<float> fillLat,
                                                      float radius = 1.002f)
{
    std::vector<osg::Vec3> out;
    out.reserve(std::min(lon.size(), lat.size()));

    for (size_t i = 0; i < lon.size() && i < lat.size(); ++i) {
        float lo = lon[i];
        float la = lat[i];
        if (!std::isfinite(lo) || !std::isfinite(la)) continue;
        if (fillLon && lo == *fillLon) continue;
        if (fillLat && la == *fillLat) continue;
        if (lo < -20.0f && lo > -40.0f && la > 0.0f && la < 20.0f) std::swap(lo, la);
        out.push_back(lonLat_toXYZ(lo, la, radius));
    }
    return out;
}

} // namespace pcve::util
#endif //PLASTICPARTICLEVE_UTILS_GEO_COLOR_H