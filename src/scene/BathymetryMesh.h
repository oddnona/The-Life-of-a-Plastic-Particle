#pragma once
#include <string>
#include <osg/Node>
#include <osg/ref_ptr>

class BathymetryMesh
{
public:
    static osg::ref_ptr<osg::Node> load(const std::string& path,
                                        float scale = 1.0f,
                                        float tx = 0.0f,
                                        float ty = 0.0f,
                                        float tz = 0.0f);
};
