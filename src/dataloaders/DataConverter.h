#ifndef DATA_CONVERTER_H
#define DATA_CONVERTER_H

#include <string>

class DataConverter
{
public:

    DataConverter(double lat0Deg,
                  double lon0Deg,
                  double earthRadius);

    void project(double lonDeg,
                 double latDeg,
                 double depth,
                 double& x,
                 double& y,
                 double& z) const;

    bool convertASCToXYZ(const std::string& ascPath,
                         const std::string& outXYZ,
                         size_t step = 1);

    bool convertNCToXYZ(const std::string& ncPath,
                        const std::string& outXYZ);

private:
    double _lat0Deg;
    double _lon0Deg;
    double _R;
};

#endif // DATA_CONVERTER_H
