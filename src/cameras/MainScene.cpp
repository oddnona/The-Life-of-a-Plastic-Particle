#include "MainScene.h"

#include <osg/ComputeBoundsVisitor>
#include <osgDB/ReadFile>
#include <iostream>

#include "../scene/BathymetryMesh.h"
#include "../scene/SurfacePlane.h"

namespace
{
    static osg::BoundingBox getBounds(osg::Node* n)
    {
        osg::ComputeBoundsVisitor cbv;
        n->accept(cbv);
        return cbv.getBoundingBox();
    }
}

bool MainScene::initialize(osgViewer::Viewer& viewer,
                           unsigned int sceneMask,
                           float cameraDistance,
                           ZoomTransitionController* zoomCtrl)
{
    sceneMask_ = sceneMask;
    worldScene_ = new osg::Group();
    worldScene_->setNodeMask(sceneMask_);
    bathyMesh_ = BathymetryMesh::load("../../OceanGrid/src/coordinates/meshply.ply", 1.0f);
    if (!bathyMesh_)
        return false;

    worldScene_->addChild(bathyMesh_.get());
    osg::ComputeBoundsVisitor cbvBathy;
    bathyMesh_->accept(cbvBathy);
    bathyBB_ = cbvBathy.getBoundingBox();
    bathyBounds_ = osg::BoundingSphere(bathyBB_.center(), bathyBB_.radius());

    if (zoomCtrl)
        zoomCtrl->setSceneRadius(bathyBB_.radius());

    std::cout << "[Zoom] ComputeBoundsVisitor BB radius = " << bathyBB_.radius() << "\n";
    osg::ComputeBoundsVisitor cbvRadius;
    bathyMesh_->accept(cbvRadius);
    osg::BoundingBox bathyBBForRadius = cbvRadius.getBoundingBox();

    double bathyRadius = bathyBBForRadius.radius();
    if (zoomCtrl)
        zoomCtrl->setSceneRadius(bathyRadius);

    std::cout << "[Zoom] ComputeBoundsVisitor BB radius = " << bathyRadius << "\n";
    viewer.getCamera()->setComputeNearFarMode(
        osg::Camera::COMPUTE_NEAR_FAR_USING_BOUNDING_VOLUMES
    );

    bathyBB_ = getBounds(bathyMesh_.get());
    osg::ref_ptr<osg::Node> uvwRaw = osgDB::readNodeFile("../../OceanGrid/src/coordinates/tubes_7.ply");
    if (uvwRaw.valid())
    {
        osg::BoundingBox uvwBB = getBounds(uvwRaw.get());
        osg::Vec3 bathyCenter = bathyBB_.center();
        osg::Vec3 uvwCenter = uvwBB.center();
        osg::Vec3 centerOffset = bathyCenter - uvwCenter;

        osg::Vec3 currentsOffset(
            3.12e6f,
            -2.19e6f,
            33720.6f
        );

        osg::Vec3 uvwToBathyOffset = centerOffset + currentsOffset;
        (void)uvwToBathyOffset;
    }

    osg::Vec3 center = bathyMesh_->getBound().center();
    SurfacePlane surface(10000000.0f, center);
    surface.setExaggeration(100.0f);

    surfaceNode_ = surface.createNode();
    worldScene_->addChild(surfaceNode_.get());

    std::string particleFile = "../../OceanGrid/src/trajectories/trajectory_origin.xyz";
    particle_ = new Particle(particleFile, 1);

    if (zoomCtrl)
    {
        zoomCtrl->setPhaseBAnchorProvider([p = particle_.get()]() -> osg::Vec3d {
            osg::Vec3 v = p ? p->getCurrentPosition() : osg::Vec3(0,0,0);
            return osg::Vec3d(v.x(), v.y(), v.z());
        });
    }

    worldScene_->addChild(particle_.get());
    particle_->setNodeMask(0x0);
    particle_->setPaused(true);
    followCam_ = new FollowCameraManipulator(particle_.get(), cameraDistance, bathyBounds_);
    particle_->setFollowCam(followCam_.get());

    return true;
}

void MainScene::setVisible(bool visible)
{
    if (!worldScene_) return;
    worldScene_->setNodeMask(visible ? sceneMask_ : 0x0);
}

void MainScene::showParticleForCutaway(float normalizedT)
{
    if (!particle_) return;
    particle_->resetBubbleGrowth();
    particle_->seekNormalized(normalizedT);
    particle_->setNodeMask(0xffffffff);
    particle_->setPaused(false);
}

void MainScene::startParticleBubbleCutawayClock()
{
    if (!particle_) return;

    particle_->startBubbleCutawayClock();
    particle_->updateBubbleNow();
}

void MainScene::hideParticleAndPause()
{
    if (!particle_) return;

    particle_->setPaused(true);
    particle_->setNodeMask(0x0);
}

void MainScene::prepareForFollowCam()
{
    if (followCam_)
        followCam_->resetDistanceRamp();

    if (surfaceNode_)
        surfaceNode_->setNodeMask(0xffffffff);
}

void MainScene::ensureCutawayEffects(osgViewer::Viewer& viewer,
                                     float waterZ,
                                     const osg::Vec4& skyColor,
                                     const osg::Vec4& underwaterColor,
                                     float transitionHeight)
{
    if (!cutawayEffectsInitialized_)
    {
        bgController_ = new DynamicBackgroundController(
            waterZ,
            skyColor,
            underwaterColor,
            transitionHeight
        );
        viewer.getCamera()->addUpdateCallback(bgController_.get());

        fog_ = new FogController(
            viewer,
            worldScene_.get(),
            1.0f,
            500.0f,
            waterZ,
            underwaterColor
        );

        cutawayEffectsInitialized_ = true;
        return;
    }

    if (bgController_)
        viewer.getCamera()->addUpdateCallback(bgController_.get());
}

void MainScene::removeCutawayEffects(osgViewer::Viewer& viewer)
{
    if (bgController_)
    {
        viewer.getCamera()->removeUpdateCallback(bgController_.get());
        bgController_ = nullptr;
    }

    fog_ = nullptr;
    cutawayEffectsInitialized_ = false;
}