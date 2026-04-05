#ifndef PLASTICPARTICLEVE_HANDOFFS_H
#define PLASTICPARTICLEVE_HANDOFFS_H

#include <osg/Vec3d>
#include <osgViewer/Viewer>

class RenderCompositeSystem;
class MainScene;
class PhaseCoordinator;
class ZoomTransitionController;
class FollowCameraManipulator;
class GlobeSequence;
class Particle;

class HandOffs
{
public:
    HandOffs(PhaseCoordinator& phaseCoordinator,
             RenderCompositeSystem& renderComposite,
             MainScene& mainScene,
             ZoomTransitionController* zoomCtrl);

    void setPhaseBAnchor(const osg::Vec3d& anchor);
    void setFollowCamera(FollowCameraManipulator* followCam);
    void setGlobeSequence(GlobeSequence* globeSequence);
    void setParticle(Particle* particle);
    void clearPhaseBAnchor();
    void updateFollowCamInstall();
    void updateCutaway(osgViewer::Viewer& viewer,
                   double nowT,
                   float waterZ,
                   const osg::Vec4& skyColor,
                   const osg::Vec4& underwaterColor,
                   float transitionHeight);
    void updateZoomOut(double nowT);

private:
    PhaseCoordinator& phaseCoordinator_;
    RenderCompositeSystem& renderComposite_;
    MainScene& mainScene_;
    ZoomTransitionController* zoomCtrl_ = nullptr;
    FollowCameraManipulator* followCam_ = nullptr;
    GlobeSequence* globeSequence_ = nullptr;
    Particle* particle_ = nullptr;
    osg::Vec3d savedPhaseBAnchor_ = osg::Vec3d(0.0, 0.0, 0.0);
    bool savedPhaseBAnchorValid_ = false;
    bool loggedTravelStart_ = false;
    bool loggedCutawayWindow_ = false;
    bool loggedCutawayTrigger_ = false;
};

#endif // PLASTICPARTICLEVE_HANDOFFS_H