#include "SurfacePlane.h"
#include <osg/BlendFunc>
#include <osg/Geometry>

SurfacePlane::SurfacePlane(float size, const osg::Vec3& center)
    : _size(size), _center(center)
{
}

void SurfacePlane::setExaggeration(float exaggeration)
{
    _exaggeration = exaggeration;
}

osg::ref_ptr<osg::Geode> SurfacePlane::createGeometry()
{
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();

    float s = _size * 0.5f;

    osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array();
    verts->push_back(osg::Vec3(-s, -s, 0.0f));
    verts->push_back(osg::Vec3( s, -s, 0.0f));
    verts->push_back(osg::Vec3( s,  s, 0.0f));
    verts->push_back(osg::Vec3(-s,  s, 0.0f));
    geom->setVertexArray(verts.get());

    osg::ref_ptr<osg::DrawElementsUInt> indices =
        new osg::DrawElementsUInt(GL_QUADS);
    indices->push_back(0);
    indices->push_back(1);
    indices->push_back(2);
    indices->push_back(3);
    geom->addPrimitiveSet(indices.get());

    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    //colors->push_back(osg::Vec4(0.4f, 0.7f, 1.0f, 0.9f)); // nice blue
     // to match the globe texture
    //colors->push_back(osg::Vec4(1.0f/255.0f, 28.0f/255.0f, 74.0f/255.0f, 0.9f));
    colors->push_back(osg::Vec4(1.0f/255.0f, 19.0f/255.0f, 51.0f/255.0f, 0.95f));
    geom->setColorArray(colors.get(), osg::Array::BIND_OVERALL);

    // You can actually skip normals if you disable lighting,
    // but keeping them is fine:
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
    geom->setNormalArray(normals.get(), osg::Array::BIND_OVERALL);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geom.get());

    osg::StateSet* ss = geode->getOrCreateStateSet();
    ss->setMode(GL_BLEND, osg::StateAttribute::ON);
    ss->setAttribute(new osg::BlendFunc());
    ss->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

    return geode;
}

osg::ref_ptr<osg::Node> SurfacePlane::createNode()
{
    osg::ref_ptr<osg::Geode> geode = createGeometry();

    osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform();
    mt->addChild(geode);

    return geode.release();
}
