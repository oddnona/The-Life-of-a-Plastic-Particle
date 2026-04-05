#include "BathymetryMesh.h"
#include <osgDB/ReadFile>
#include <osg/MatrixTransform>
#include <iostream>
#include <osg/Material>
#include <osg/ComputeBoundsVisitor>

osg::ref_ptr<osg::Node> BathymetryMesh::load(const std::string& path,
                                             float scale,
                                             float tx,
                                             float ty,
                                             float tz)
{
    osg::ref_ptr<osg::Node> mesh = osgDB::readNodeFile(path);
    if (!mesh)
    {
        std::cerr << "[BathymetryMesh] Error loading: " << path << "\n";
        return nullptr;
    }

    std::cout << "[BathymetryMesh] Successfully loaded: " << path << "\n";



    return mesh.release();
}

