#ifndef PLASTICPARTICLEVE_RENDERCOMPOSITESYSTEM_H
#define PLASTICPARTICLEVE_RENDERCOMPOSITESYSTEM_H

#include <osg/ref_ptr>
#include <osg/observer_ptr>
#include <osg/Group>
#include <osg/Camera>
#include <osg/Geode>
#include <osg/Texture2D>
#include <osgViewer/Viewer>

#include "../globe/ZoomTransitionController.h"

class RenderCompositeSystem
{
public:
    struct Masks
    {
        unsigned int overlay = 0;
        unsigned int scene = 0;
        unsigned int globe = 0;
        unsigned int masterCullGlobe = 0;
        unsigned int masterCullScene = 0;
    };

    bool initialize(osgViewer::Viewer& viewer, osg::Group* root, const Masks& masks);
    void attachScenes(osg::Node* globeScene, osg::Node* worldScene);

    osg::Camera* hudCameraLeft() const { return hudLeft_.get(); }
    osg::Camera* hudCameraRight() const { return hudRight_.get(); }
    osg::Camera* globeRTTCameraLeft() const { return rttGlobeCamL_.get(); }
    osg::Camera* globeRTTCameraRight() const { return rttGlobeCamR_.get(); }
    osg::Camera* bathyRTTCameraLeft() const { return rttBathyCamL_.get(); }
    osg::Camera* bathyRTTCameraRight() const { return rttBathyCamR_.get(); }

    osg::Geode* globeQuadLeft() const { return globeQuadL_.get(); }
    osg::Geode* globeQuadRight() const { return globeQuadR_.get(); }
    osg::Geode* bathyQuadLeft() const { return bathyQuadL_.get(); }
    osg::Geode* bathyQuadRight() const { return bathyQuadR_.get(); }

    osg::Texture2D* globeTextureLeft() const { return texGlobeL_.get(); }
    osg::Texture2D* globeTextureRight() const { return texGlobeR_.get(); }
    osg::Texture2D* bathyTextureLeft() const { return texBathyL_.get(); }
    osg::Texture2D* bathyTextureRight() const { return texBathyR_.get(); }

    ZoomTransitionController* zoomController() const { return zoomCtrl_.get(); }

    int width() const { return width_; }
    int height() const { return height_; }

    bool globeTextureReady() const { return texGlobeReady_; }
    bool bathyTextureReady() const { return texBathyReady_; }

    void setGlobeAlpha(float a);
    void setBathyAlpha(float a);

    void setGlobeQuadVisible(bool visible);
    void setBathyQuadVisible(bool visible);
    void setMasterCullGlobeOnly();
    void setMasterCullSceneAndOverlay();
    void setMasterBlackClear();

    void hideCompositeQuads();
    void showGlobeCompositeOnly();
    void showBothCompositeQuads();
    void showBathyCompositeOnly();

private:
    osg::ref_ptr<osg::Texture2D> texGlobeL_;
    osg::ref_ptr<osg::Texture2D> texGlobeR_;
    osg::observer_ptr<osg::Camera> masterCamera_;
    osg::ref_ptr<osg::Texture2D> texBathyL_;
    osg::ref_ptr<osg::Texture2D> texBathyR_;

    osg::ref_ptr<osg::Camera> rttGlobeCamL_;
    osg::ref_ptr<osg::Camera> rttGlobeCamR_;
    osg::ref_ptr<osg::Camera> rttBathyCamL_;
    osg::ref_ptr<osg::Camera> rttBathyCamR_;
    osg::ref_ptr<osg::Camera> hudLeft_;
    osg::ref_ptr<osg::Camera> hudRight_;

    osg::ref_ptr<osg::Geode> globeQuadL_;
    osg::ref_ptr<osg::Geode> globeQuadR_;
    osg::ref_ptr<osg::Geode> bathyQuadL_;
    osg::ref_ptr<osg::Geode> bathyQuadR_;
    osg::ref_ptr<ZoomTransitionController> zoomCtrl_;

    Masks masks_;
    int width_ = 0;
    int height_ = 0;
    bool texGlobeReady_ = false;
    bool texBathyReady_ = false;
    bool scenesAttached_ = false;
};

#endif // PLASTICPARTICLEVE_RENDERCOMPOSITESYSTEM_H