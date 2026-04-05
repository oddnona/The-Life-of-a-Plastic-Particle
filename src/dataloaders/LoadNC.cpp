#include "LoadNC.h"
#include <netcdf>
#include <iostream>
#include <optional>
#include <cmath>

using namespace netCDF;
using namespace netCDF::exceptions;

bool LoadNC::load(const std::string& filename)
{
    try
    {


        NcFile file(filename, NcFile::read);

        NcVar latVar = file.getVar("latitude");
        NcVar lonVar = file.getVar("longitude");
        NcVar depthVar = file.getVar("depth");

        size_t nLat = latVar.getDim(0).getSize();
        size_t nLon = lonVar.getDim(0).getSize();
        size_t nDepth = depthVar.getDim(0).getSize();

        _latitude.resize(nLat);
        _longitude.resize(nLon);
        _depth.resize(nDepth);

        latVar.getVar(_latitude.data());
        lonVar.getVar(_longitude.data());
        depthVar.getVar(_depth.data());

        std::cout << "Loaded coordinate file: " << filename << std::endl;
        std::cout << "Lat: " << nLat << " Lon: " << nLon << " Depth: " << nDepth << std::endl;

        return true;
    }
    catch (NcException& e)
    {
        std::cerr << "NetCDF Error: " << e.what() << std::endl;
        return false;
    }
}

std::vector<float> LoadNC::loadVariable(const std::string& filename, const std::string& varName)
{
    std::vector<float> data;
    try
    {
        netCDF::NcFile file(filename, netCDF::NcFile::read);
        netCDF::NcVar var = file.getVar(varName);


        size_t totalSize = 1;
        for (auto& dim : var.getDims())
            totalSize *= dim.getSize();

        data.resize(totalSize);
        var.getVar(data.data());
    }
    catch (netCDF::exceptions::NcException& e)
    {
        std::cerr << "NetCDF Error while reading " << varName << ": " << e.what() << std::endl;
    }

    return data;
}

std::vector<float> LoadNC::loadMask(const std::string& filename, const std::string& varName)
{
    std::vector<float> mask;

    try
    {
        NcFile file(filename, NcFile::read);
        NcVar maskVar = file.getVar(varName);

        size_t totalSize = 1;
        for (auto& dim : maskVar.getDims())
            totalSize *= dim.getSize();

        mask.resize(totalSize);
        maskVar.getVar(mask.data());

        std::cout << "Loaded mask variable '" << varName << "' (" << totalSize << " values)\n";
    }
    catch (NcException& e)
    {
        std::cerr << "NetCDF mask read error: " << e.what() << std::endl;
    }

    return mask;
}

TrajectoryData LoadNC::loadTrajectoryRaw(const std::string& filename, int particleIndex)
{
    TrajectoryData data;

    try
    {
        netCDF::NcFile file(filename, netCDF::NcFile::read);

        auto getVar = [&](std::initializer_list<const char*> names) -> netCDF::NcVar {
            for (auto n : names)
            {
                auto var = file.getVar(n);
                if (!var.isNull()) return var;
            }
            return netCDF::NcVar();
        };

        netCDF::NcVar lonVar = getVar({"lon", "longitude", "x"});
        netCDF::NcVar latVar = getVar({"lat", "latitude", "y"});
        netCDF::NcVar depVar = getVar({"depth", "z", "elevation"});


        size_t nTraj = lonVar.getDim(0).getSize();
        size_t nObs  = lonVar.getDim(1).getSize();
        if ((size_t)particleIndex >= nTraj)
        {
            std::cerr << "[LoadNC] Particle index out of range (0-" << nTraj - 1 << ")\n";
            return data;
        }

        data.lon.resize(nObs);
        data.lat.resize(nObs);
        data.depth.resize(nObs, 0.0f);

        std::vector<size_t> start = {static_cast<size_t>(particleIndex), 0};
        std::vector<size_t> count = {1, nObs};

        lonVar.getVar(start, count, data.lon.data());
        latVar.getVar(start, count, data.lat.data());
        if (!depVar.isNull()) depVar.getVar(start, count, data.depth.data());

        // Compute bounds
        double minLon = data.lon[0], maxLon = data.lon[0];
        double minLat = data.lat[0], maxLat = data.lat[0];
        double minDep = data.depth[0], maxDep = data.depth[0];

        for (size_t i = 0; i < nObs; i++)
        {
            minLon = std::min(minLon, (double)data.lon[i]);
            maxLon = std::max(maxLon, (double)data.lon[i]);

            minLat = std::min(minLat, (double)data.lat[i]);
            maxLat = std::max(maxLat, (double)data.lat[i]);

            minDep = std::min(minDep, (double)data.depth[i]);
            maxDep = std::max(maxDep, (double)data.depth[i]);
        }

        std::cout << "[LoadNC] Loaded raw trajectory (" << nObs
                  << " samples) for particle " << particleIndex << ".\n\n";
    }
    catch (netCDF::exceptions::NcException& e)
    {
        std::cerr << "[LoadNC] NetCDF error: " << e.what() << std::endl;
    }

    return data;
}
