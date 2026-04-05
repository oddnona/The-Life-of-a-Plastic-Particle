
#ifndef PLASTICPARTICLEVE_UTILS_H
#define PLASTICPARTICLEVE_UTILS_H

#pragma once
#include <netcdf>
#include <string>
#include <initializer_list>
#include <optional>
#include <vector>
#include <cctype>
#include <algorithm>

namespace pcve::nc {

inline std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
inline bool ieq(const std::string& a, const std::string& b) {
    return lower(a) == lower(b);
}

inline netCDF::NcVar getVarAny(netCDF::NcFile& f, std::initializer_list<const char*> names)
{
    // direct-name match, case-insensitive
    for (auto& kv : f.getVars()) {
        for (auto n : names) {
            if (ieq(kv.first, n)) return kv.second;
        }
    }
    // standard_name match
    for (auto& kv : f.getVars()) {
        auto att = kv.second.getAtt("standard_name");
        if (!att.isNull()) {
            std::string val;
            att.getValues(val);
            for (auto n : names) {
                if (ieq(val, n)) return kv.second;
            }
        }
    }
    return netCDF::NcVar();
}

inline std::optional<float> getFillValue(const netCDF::NcVar& v)
{
    if (v.isNull()) return std::nullopt;
    for (const char* k : {"_FillValue", "missing_value"}) {
        auto a = v.getAtt(k);
        if (!a.isNull()) {
            float f;
            a.getValues(&f);
            return f;
        }
    }
    return std::nullopt;
}

// reads one trajectory row v(traj, obs) into dst
inline bool readTrajSeries2D(const netCDF::NcVar& v, int trajIndex, std::vector<float>& dst)
{
    if (v.isNull()) return false;
    if (v.getDimCount() != 2) return false;
    size_t nTraj = v.getDim(0).getSize();
    size_t nObs = v.getDim(1).getSize();
    if (trajIndex < 0 || (size_t)trajIndex >= nTraj) return false;
    dst.resize(nObs);
    std::vector<size_t> start = { (size_t)trajIndex, 0 };
    std::vector<size_t> count = { 1, nObs };
    v.getVar(start, count, dst.data());
    return true;
}

inline bool readTrajBlock2D(const netCDF::NcVar& v,
                            size_t startTraj,
                            size_t countTraj,
                            std::vector<float>& dst)
{
    if (v.isNull()) return false;
    if (v.getDimCount() != 2) return false;
    size_t nTraj = v.getDim(0).getSize();
    size_t nObs = v.getDim(1).getSize();
    if (startTraj >= nTraj) return false;
    countTraj = std::min(countTraj, nTraj - startTraj);
    if (countTraj == 0) return false;

    dst.resize(countTraj * nObs);

    std::vector<size_t> start = { startTraj, 0 };
    std::vector<size_t> count = { countTraj, nObs };

    v.getVar(start, count, dst.data());
    return true;
}

} // namespace pcve::nc


#endif //PLASTICPARTICLEVE_UTILS_H