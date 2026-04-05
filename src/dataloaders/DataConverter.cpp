#include "DataConverter.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <iomanip>

#include "LoadNC.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

DataConverter::DataConverter(double lat0Deg,
                             double lon0Deg,
                             double earthRadius)
    : _lat0Deg(lat0Deg),
      _lon0Deg(lon0Deg),
      _R(earthRadius)
{
    std::cout << "[DataConverter] Shared origin: LAT0=" << _lat0Deg
              << "  LON0=" << _lon0Deg << std::endl;
}

inline void DataConverter::project(double lonDeg, double latDeg, double depth,
                                   double& x, double& y, double& z) const
{
    constexpr double DEG2RAD = M_PI / 180.0;

    double lat0 = _lat0Deg * DEG2RAD;
    double lon0 = _lon0Deg * DEG2RAD;
    double lat  = latDeg * DEG2RAD;
    double lon  = lonDeg * DEG2RAD;

    double dLon = lon - lon0;
    double dLat = lat - lat0;

    x = _R * dLon * cos(lat0);
    y = _R * dLat;
    z = depth;
}

bool DataConverter::convertASCToXYZ(const std::string& ascPath,
                                    const std::string& outXYZ,
                                    size_t step)
{
    std::ifstream asc(ascPath);
    if (!asc.is_open()) {
        std::cerr << "[DataConverter] Could not open ASC file\n";
        return false;
    }

    std::string tmp;
    int ncols, nrows;
    double xllcorner, yllcorner, cellsize, nodata;

    asc >> tmp >> ncols;
    asc >> tmp >> nrows;
    asc >> tmp >> xllcorner;
    asc >> tmp >> yllcorner;
    asc >> tmp >> cellsize;
    asc >> tmp >> nodata;

    std::vector<float> depth(ncols * nrows);
    for (int i = 0; i < nrows; ++i)
        for (int j = 0; j < ncols; ++j)
            asc >> depth[i * ncols + j];
    asc.close();

    std::ofstream out(outXYZ);
    if (!out.is_open()) return false;


    const double LAT_MIN = -60.0;
    const double LAT_MAX =  15.0;
    const double LON_MIN = -60.0;
    const double LON_MAX =  30.0;

    for (int i = 0; i < nrows; i += static_cast<int>(step)) {

        int row = i;
        double latDeg = yllcorner + row * cellsize;

        if (latDeg < LAT_MIN || latDeg > LAT_MAX)
            continue;

        for (int j = 0; j < ncols; j += static_cast<int>(step)) {

            double lonDeg = xllcorner + j * cellsize;

            if (lonDeg < LON_MIN || lonDeg > LON_MAX)
                continue;

            float d = depth[row * ncols + j];
            if (d == nodata)
                continue;

            double x, y, z;
            project(lonDeg, latDeg, d, x, y, z);

            out << std::setprecision(8) << std::fixed
                << x << " " << y << " " << z << "\n";
        }
    }

    out.close();
    std::cout << "[DataConverter] ASC to XYZ (meters) written: " << outXYZ << std::endl;
    return true;
}


bool DataConverter::convertNCToXYZ(const std::string& ncPath,
                                   const std::string& outXYZ)
{
    LoadNC loader;
    TrajectoryData raw = loader.loadTrajectoryRaw(ncPath, 300);

    if (raw.lon.empty()) {
        std::cerr << "[DataConverter] Failed to load NC file\n";
        return false;
    }

    std::ofstream out(outXYZ);
    if (!out.is_open()) return false;

    for (size_t i = 0; i < raw.lon.size(); ++i)
    {
        double x, y, z;
        double lonDeg = raw.lon[i];
        double latDeg = raw.lat[i];
        double depth  = -raw.depth[i];

        project(lonDeg, latDeg, depth, x, y, z);

        out << std::scientific << std::setprecision(6)
            << x << " " << y << " " << z << "\n";
    }

    out.close();
    std::cout << "[DataConverter] NC trajectory XYZ meters written: " << outXYZ << std::endl;

    return true;
}
