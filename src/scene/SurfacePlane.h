#pragma once
#include <osg/Node>
#include <osg/Geode>
#include <osg/MatrixTransform>
#include <osg/Vec3>

class SurfacePlane
{
public:
    SurfacePlane(float size, const osg::Vec3& center);

    void setExaggeration(float exaggeration);

    osg::ref_ptr<osg::Node> createNode();

private:
    osg::ref_ptr<osg::Geode> createGeometry();

    float _size;
    osg::Vec3 _center;
    float _exaggeration = 1.0f;
};
