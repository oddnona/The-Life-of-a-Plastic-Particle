#ifndef PLASTICPARTICLEVE_MAINSCENE_H
#define PLASTICPARTICLEVE_MAINSCENE_H

#include <osg/ref_ptr>
#include <osg/Group>
#include <osg/Node>
#include <osg/BoundingSphere>
#include <osg/Vec4>
#include <osgViewer/Viewer>

#include "../globe/ZoomTransitionController.h"
#include "../scene/FollowCameraManipulator.h"
#include "../scene/Particle.h"
#include "../scene/DynamicBackgroundController.h"
#include "../scene/FogController.h"

class MainScene
{
public:
    MainScene() = default;

    bool initialize(osgViewer::Viewer& viewer,
                    unsigned int sceneMask,
                    float cameraDistance,
                    ZoomTransitionController* zoomCtrl);

    osg::Group* scene() const { return worldScene_.get(); }
    osg::Node* bathyMesh() const { return bathyMesh_.get(); }
    osg::Node* surfaceNode() const { return surfaceNode_.get(); }
    Particle* particle() const { return particle_.get(); }
    FollowCameraManipulator* followCam() const { return followCam_.get(); }

    const osg::BoundingBox& bathyBoundsBox() const { return bathyBB_; }
    const osg::BoundingSphere& bathyBoundsSphere() const { return bathyBounds_; }
    osg::Vec3 bathyCenter() const { return bathyBB_.center(); }

    void setVisible(bool visible);
    void showParticleForCutaway(float normalizedT);
    void startParticleBubbleCutawayClock();
    void hideParticleAndPause();
    void prepareForFollowCam();

    void ensureCutawayEffects(osgViewer::Viewer& viewer,
                              float waterZ,
                              const osg::Vec4& skyColor,
                              const osg::Vec4& underwaterColor,
                              float transitionHeight);

    void removeCutawayEffects(osgViewer::Viewer& viewer);

private:
    osg::ref_ptr<osg::Group> worldScene_;
    osg::ref_ptr<osg::Node> bathyMesh_;
    osg::ref_ptr<osg::Node> surfaceNode_;
    osg::ref_ptr<Particle> particle_;
    osg::ref_ptr<FollowCameraManipulator> followCam_;
    osg::ref_ptr<DynamicBackgroundController> bgController_;
    osg::ref_ptr<FogController> fog_;
    bool cutawayEffectsInitialized_ = false;
    osg::BoundingBox bathyBB_;
    osg::BoundingSphere bathyBounds_;
    unsigned int sceneMask_ = 0;
};

#endif // PLASTICPARTICLEVE_MAINSCENE_H