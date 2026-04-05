#pragma once
#include <osg/Fog>
#include <osg/Group>
#include <osg/NodeCallback>
#include <osgViewer/Viewer>

class FogController : public osg::Referenced
{
public:
    FogController(osgViewer::Viewer& viewer,
                  osg::Group* root,
                  float fogStart,
                  float fogEnd,
                  float surfaceZ,
                  const osg::Vec4& fogColor)
        : _surfaceZ(surfaceZ)
    {
        _fog = new osg::Fog();
        _fog->setMode(osg::Fog::EXP2);
        _fog->setColor(fogColor);

        // Attach fog attribute
        root->getOrCreateStateSet()->setAttributeAndModes(
            _fog.get(), osg::StateAttribute::ON
        );

        viewer.getCamera()->addUpdateCallback(
            new FogUpdateCallback(_fog.get(), fogStart, fogEnd, _surfaceZ)
        );
    }

private:
    float _surfaceZ;
    osg::ref_ptr<osg::Fog> _fog;

    class FogUpdateCallback : public osg::NodeCallback
    {
    public:
        FogUpdateCallback(osg::Fog* fog,
                          float fogStart,
                          float fogEnd,
                          float surfaceZ)
            : _fog(fog), _start(fogStart), _end(fogEnd), _surfaceZ(surfaceZ)
        {}

        virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
        {
            osg::Camera* cam = dynamic_cast<osg::Camera*>(node);
            if (cam)
            {
                osg::Vec3d eye, center, up;
                cam->getViewMatrixAsLookAt(eye, center, up);

                float fadeHeight = 10000.0f;
                float maxDensity = 0.000005f;

                if (eye.z() < _surfaceZ)
                {
                    _fog->setDensity(maxDensity);
                }
                else if (eye.z() < _surfaceZ + fadeHeight)
                {
                    float t = (eye.z() - _surfaceZ) / fadeHeight;
                    float density = maxDensity * (1.0f - t);
                    _fog->setDensity(density);
                }
                else
                {
                    _fog->setDensity(0.0f);
                }
            }

            node->traverse(*nv);
        }


    private:
        osg::ref_ptr<osg::Fog> _fog;
        float _start;
        float _end;
        float _surfaceZ;
    };
};