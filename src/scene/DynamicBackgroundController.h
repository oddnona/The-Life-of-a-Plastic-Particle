#pragma once
#include <osg/Camera>
#include <osg/Math>

class DynamicBackgroundController : public osg::NodeCallback
{
public:
    DynamicBackgroundController(float waterZ,
                                const osg::Vec4& skyColor,
                                const osg::Vec4& underwaterColor,
                                float transitionHeight = 200.0f)
        : _waterZ(waterZ),
          _skyColor(skyColor),
          _underwaterColor(underwaterColor),
          _transitionHeight(transitionHeight)
    {}

    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
    {
        osg::Camera* cam = dynamic_cast<osg::Camera*>(node);
        if (!cam)
        {
            traverse(node, &*nv);
            return;
        }

        osg::Vec3d eye, center, up;
        cam->getViewMatrixAsLookAt(eye, center, up);

        float cameraZ = eye.z();

        float t = (cameraZ - _waterZ) / _transitionHeight;
        t = osg::clampBetween(t, 0.0f, 1.0f);

        float smoothT = t * t * (3.0f - 2.0f * t);

        osg::Vec4 clearColor =
            _underwaterColor * (1.0f - smoothT) +
            _skyColor * smoothT;

        cam->setClearColor(clearColor);

        traverse(node, &*nv);
    }

private:
    float _waterZ;
    osg::Vec4 _skyColor;
    osg::Vec4 _underwaterColor;
    float _transitionHeight;
};
