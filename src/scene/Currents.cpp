#include "Currents.h"

#include <osg/Depth>
#include <osgDB/ReadFile>
#include <osg/Notify>
#include <osg/StateSet>

Currents::Currents(
    const std::string& plyFile,
    const osg::Vec3& translation
)
{
    osg::ref_ptr<osg::Node> currentsNode =
        osgDB::readNodeFile(plyFile);

    if (!currentsNode)
    {
        osg::notify(osg::FATAL)
            << "Currents: failed to load " << plyFile << std::endl;
        return;
    }

    _transform = new osg::MatrixTransform;
    _transform->setMatrix(osg::Matrix::translate(translation));
    _transform->addChild(currentsNode.get());

    osg::StateSet* ss = _transform->getOrCreateStateSet();

    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

    ss->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

    addChild(_transform.get());

    setDataVariance(osg::Object::STATIC);
}
