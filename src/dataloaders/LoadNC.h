#pragma once
#include <string>
#include <vector>

struct TrajectoryData
{
    std::vector<float> lon;
    std::vector<float> lat;
    std::vector<float> depth;
};

class LoadNC
{
public:
    bool load(const std::string& filename);
    std::vector<float> loadVariable(const std::string& filename, const std::string& varName);
    std::vector<float> loadMask(const std::string& filename, const std::string& varName);

    TrajectoryData loadTrajectoryRaw(const std::string& filename, int particleIndex);


    const std::vector<float>& getLatitude() const { return _latitude; }
    const std::vector<float>& getLongitude() const { return _longitude; }
    const std::vector<float>& getDepth() const { return _depth; }

private:
    std::vector<float> _latitude;
    std::vector<float> _longitude;
    std::vector<float> _depth;
};
