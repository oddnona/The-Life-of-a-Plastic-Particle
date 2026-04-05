#pragma once

#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <string>

class Currents : public osg::Group
{
public:
    Currents(
        const std::string& plyFile,
        const osg::Vec3& translation
    );

private:
    osg::ref_ptr<osg::MatrixTransform> _transform;
};
