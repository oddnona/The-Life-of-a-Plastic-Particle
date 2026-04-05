#include <osgViewer/Viewer>
#include <osgViewer/GraphicsWindow>
#include <osg/LightModel>
#include <osg/ComputeBoundsVisitor>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/Camera>
#include <memory>
#include <netcdf>
#include <osg/NodeVisitor>
#include "dataloaders/DataConverter.h"
#include <osg/Texture2D>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/FrameBufferObject>
#include <osg/Program>
#include <osg/Shader>
#include <osg/Uniform>
#include <cstring>
#include <osg/StateSet>
#include <iomanip>
#include <random>
#include <algorithm>
#include <cmath>
#include <osg/Depth>
#include <osg/Point>
// Globe modules
#include "globe/globe.h"
#include "globe/density.h"
#include "globe/many.h"
#include "globe/trajectories.h"
#include "globe/travel.h"
#include "globe/timeCounter.h"
#include "globe/spin.h"
#include <osgGA/CameraManipulator>
#include "globe/ZoomTransitionController.h"
// existing scene
#include <osg/BlendFunc>
#include <osg/Depth>
#include "scene/BathymetryMesh.h"
#include "scene/DynamicBackgroundController.h"
#include "scene/FogController.h"
#include "scene/SurfacePlane.h"
#include "scene/FollowCameraManipulator.h"
#include "scene/Particle.h"
#include "scene/Currents.h"
#include "scene/TogglePlaybackHandler.h"
#include "cameras/RenderCompositeSystem.h"
#include "cameras/GlobeSequence.h"
#include "cameras/MainScene.h"
#include "cameras/PhaseCoordinator.h"
#include "cameras/HandOffs.h"
#include "intro/Intro.h"
#include "intro/AdvanceIntroHandler.h"
#include <fstream>


struct DisableClearsExceptMaster : public osg::NodeVisitor
{
    osg::observer_ptr<osg::Camera> master;
    DisableClearsExceptMaster(osg::Camera* m)
        : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN), master(m) {}

    void apply(osg::Camera& cam) override
    {
        if (&cam != master.get())
        {
            if (cam.getRenderTargetImplementation() != osg::Camera::FRAME_BUFFER_OBJECT)
            {
                cam.setClearMask(0);
            }
        }
        traverse(cam);
    }
};
struct GlobeClearGuard : public osg::Camera::DrawCallback
{
    osg::observer_ptr<osg::Camera> master;
    PhaseCoordinator::AppPhase* phase = nullptr;
    bool* cutawayActive = nullptr;

    GlobeClearGuard(osg::Camera* cam,
                PhaseCoordinator::AppPhase* p,
                bool* a)
        : master(cam), phase(p), cutawayActive(a) {}

    void operator()(osg::RenderInfo&) const override
    {
        if (!master) return;

        // black clear during globe phase
        if (phase && cutawayActive &&
            *phase == PhaseCoordinator::AppPhase::GlobeAnimations &&
            !(*cutawayActive))
        {
            master->setClearColor(osg::Vec4(0.f, 0.f, 0.f, 1.f));
            master->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
    }
};

static const char* phaseName(PhaseCoordinator::AppPhase p)
{
    switch (p)
    {
        case PhaseCoordinator::AppPhase::Intro: return "Intro";
        case PhaseCoordinator::AppPhase::GlobeAnimations: return "GlobeAnimations";
        default: return "Unknown";
    }
}

static void logCameraState(const char* tag, osgViewer::Viewer& viewer)
{
    osg::Camera* c = viewer.getCamera();
    if (!c)
    {
        std::cerr << "[CAMERA] " << tag << " master=null\n";
        return;
    }

    std::cerr
        << "[CAMERA] " << tag
        << " master=" << c
        << " clearMask=" << c->getClearMask()
        << " clearColor=("
        << c->getClearColor().r() << ","
        << c->getClearColor().g() << ","
        << c->getClearColor().b() << ","
        << c->getClearColor().a() << ")"
        << " nodeMask=" << c->getNodeMask()
        << " renderOrder=" << c->getRenderOrder()
        << " renderTargetImpl=" << c->getRenderTargetImplementation()
        << " numSlaves=" << viewer.getNumSlaves()
        << "\n";

    for (unsigned i = 0; i < viewer.getNumSlaves(); ++i)
    {
        osg::Camera* sc = viewer.getSlave(i)._camera.get();
        std::cerr
            << "  [SLAVE " << i << "] ptr=" << sc;
        if (sc)
        {
            std::cerr
                << " clearMask=" << sc->getClearMask()
                << " nodeMask=" << sc->getNodeMask()
                << " renderOrder=" << sc->getRenderOrder()
                << " renderTargetImpl=" << sc->getRenderTargetImplementation();
        }
        std::cerr << "\n";
    }
}

static void logNodeState(const char* tag, osg::Node* n)
{
    if (!n)
    {
        std::cerr << "[NODE] " << tag << " null\n";
        return;
    }

    std::cerr
        << "[NODE] " << tag
        << " ptr=" << n
        << " nodeMask=" << n->getNodeMask()
        << " parents=" << n->getNumParents()
        << "\n";
}

static void logFrameState(
    const char* tag,
    osgViewer::Viewer& viewer,
    PhaseCoordinator& phaseCoordinator,
    osg::Node* root,
    osg::Node* mainScene,
    osg::Node* globeScene,
    osg::Node* particleNode)
{
    const osg::FrameStamp* fs = viewer.getFrameStamp();

    std::cerr
        << "[FRAME] " << tag
        << " frame=" << (fs ? fs->getFrameNumber() : 0)
        << " simTime=" << std::fixed << std::setprecision(6)
        << (fs ? fs->getSimulationTime() : 0.0)
        << " phase=" << phaseName(phaseCoordinator.phase())
        << " bathyCutawayActive=" << phaseCoordinator.bathyCutawayActive()
        << " zoomOutActive=" << phaseCoordinator.zoomOutActive()
        << "\n";

    logNodeState("root", root);
    logNodeState("mainScene", mainScene);
    logNodeState("globeScene", globeScene);
    logNodeState("particle", particleNode);
    logCameraState(tag, viewer);
}

int main() {
    osg::DisplaySettings* ds = osg::DisplaySettings::instance();
    ds->setStereo(false);
    //ds->setStereoMode(osg::DisplaySettings::QUAD_BUFFER);
    ds->setEyeSeparation(0.01f);
    ds->setScreenDistance(0.5f);
    osgViewer::Viewer viewer;
    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
    traits->x = 0;
    traits->y = 0;
    traits->width = 1920;
    traits->height = 1080;
    traits->windowDecoration = true;
    traits->doubleBuffer = true;
    traits->quadBufferStereo = false; //TURN TRUE FOR STEREO

    osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits.get());
    if (!gc) {
        std::cerr << "Failed to create stereo GraphicsContext\n";
        return 1;
    }

    viewer.setThreadingModel(osgViewer::Viewer::SingleThreaded);
    osg::Camera* cam = viewer.getCamera();
    cam->setGraphicsContext(gc.get());
    cam->setViewport(new osg::Viewport(0, 0, traits->width, traits->height));
    const unsigned int MASK_OVERLAY = 0x1; // HUD + RTT cameras (camera nodes + HUD plates)
    const unsigned int MASK_SCENE = 0x2; // main scene (worldScene) for bathy RTT + real render
    const unsigned int MASK_GLOBE = 0x4; // globe scene ONLY (for globe RTT)
    const unsigned int MASTER_CULL_GLOBE = MASK_OVERLAY | MASK_GLOBE; // draw only HUD/quads
    const unsigned int MASTER_CULL_SCENE = MASK_OVERLAY | MASK_SCENE; // draw HUD + real scene
    osg::ref_ptr<osg::Group> root = new osg::Group;
    RenderCompositeSystem renderComposite;
    osg::ref_ptr<TimeCounter> timeCounter;
    osg::ref_ptr<ZoomTransitionController> zoomCtrl;
    GlobeSequence globeSequence;
    PhaseCoordinator phaseCoordinator;
    osg::ref_ptr<osg::Group> mainScene = new osg::Group;
    MainScene mainSceneSystem;
    HandOffs* handOffs = nullptr;
    root->addChild(mainScene.get());
    //viewer.setSceneData(root.get());
    osg::ref_ptr<osg::Group> compositeRoot = new osg::Group;
    viewer.setSceneData(compositeRoot.get());
    compositeRoot->addChild(root.get());
    viewer.realize();
    float cameraDistance = 1000000.0f; // matchES followCam distance
    cam->setClearColor(osg::Vec4(0,0,0,1));
    cam->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!gc) {
        std::cerr << "Master camera has NO GraphicsContext\n";
    } else {
        const auto* traits = gc->getTraits();
        std::cerr << "GC realized=" << gc->isRealized()
                  << " size=" << traits->width << "x" << traits->height
                  << " doubleBuffer=" << traits->doubleBuffer << "\n";

        RenderCompositeSystem::Masks compositeMasks;
        compositeMasks.overlay = MASK_OVERLAY;
        compositeMasks.scene = MASK_SCENE;
        compositeMasks.globe = MASK_GLOBE;
        compositeMasks.masterCullGlobe = MASTER_CULL_GLOBE;
        compositeMasks.masterCullScene = MASTER_CULL_SCENE;

        if (!renderComposite.initialize(viewer, root.get(), compositeMasks))
        {
            return 1;
        }
        timeCounter = new TimeCounter();
        renderComposite.hudCameraLeft()->addChild(timeCounter->node());
        renderComposite.hudCameraRight()->addChild(timeCounter->node());
        timeCounter->setVisible(false);

        zoomCtrl = renderComposite.zoomController();
        ZoomTransitionController::Params zp;
        zp.durationSec = 20;
        zp.endFovYDeg = 5.0;
        zp.endEye = osg::Vec3d(0.0, -2.2, 0.0);
        zp.endCenter = osg::Vec3d(0.0, 0.0, 0.0);
        zp.endUp = osg::Vec3d(0.0, 0.0, 1.0);
        zp.zNear = 100.0;
        zp.zFar = 1000000000.0;
        zp.phaseB_dollyDistance = cameraDistance * 1.0;
        zoomCtrl->setParams(zp);
        root->addUpdateCallback(zoomCtrl.get());
        renderComposite.setMasterCullGlobeOnly();
    }

    if (!mainSceneSystem.initialize(viewer, MASK_SCENE, cameraDistance, zoomCtrl.get()))
    {
        return 1;
    }

    mainScene->addChild(mainSceneSystem.scene());
    osg::ref_ptr<Particle> particle = mainSceneSystem.particle();
    osg::ref_ptr<FollowCameraManipulator> followCam = mainSceneSystem.followCam();
    HandOffs handOffsSystem(
    phaseCoordinator,
    renderComposite,
    mainSceneSystem,
    zoomCtrl.get()
);

    handOffsSystem.setFollowCamera(followCam.get());
    handOffsSystem.setGlobeSequence(&globeSequence);
    handOffsSystem.setParticle(particle.get());
    handOffs = &handOffsSystem;
    viewer.getCamera()->setClearColor(osg::Vec4(0.05f, 0.05f, 0.08f, 1.0f));
    // intro
    osg::ref_ptr<Intro> intro = new Intro(&viewer, mainScene.get());
    root->addChild(intro.get());
    intro->start();
    osg::ref_ptr<AdvanceIntroHandler> advanceIntro =
    new AdvanceIntroHandler(intro.get());
    viewer.addEventHandler(advanceIntro.get());
    viewer.addEventHandler(new TogglePlaybackHandler(particle.get()));
    float waterZ = 0.0f;
    osg::Vec4 skyColor(0.5f, 0.7f, 1.0f, 1.0f);
    osg::Vec4 underwaterColor(0.0f, 0.28f, 0.45f, 1.0f);
    float transitionHeight = 200000.0f;
    bool mainSceneInitialized = false;
    bool prevCutawayActive = false;
    bool prevPhaseBActive = false;
    bool prevPhaseBDone = false;
    bool followCamWasInstalled = false;
    int debugFramesAfter = 0;
    mainSceneSystem.setVisible(false);
    // globe layer (separate scene)
    globeSequence.initialize(MASK_GLOBE);
    globeSequence.setDependencies(zoomCtrl.get(), timeCounter.get());
    // RTT cameras render offscreen
    renderComposite.attachScenes(globeSequence.scene(), mainSceneSystem.scene());
    PhaseCoordinator::AppPhase currentPhaseForGuard = phaseCoordinator.phase();
    bool currentCutawayActiveForGuard = phaseCoordinator.bathyCutawayActive();

    osg::ref_ptr<GlobeClearGuard> globeClearGuard =
        new GlobeClearGuard(viewer.getCamera(),
                            &currentPhaseForGuard,
                            &currentCutawayActiveForGuard);

    viewer.getCamera()->setPreDrawCallback(globeClearGuard.get());
    logFrameState(
    "AFTER_GUARD_INSTALL",
    viewer,
    phaseCoordinator,
    root.get(),
    mainSceneSystem.scene(),
    globeSequence.scene(),
    particle.get());

    const double targetFPS = 60.0;
    const double frameTime = 1.0 / targetFPS;
    while (!viewer.done())
    {
        double startTime = osg::Timer::instance()->time_s();
        if (debugFramesAfter > 0)
        {
            std::ofstream dbg("C:\\globe_switch_debug.txt", std::ios::app);

            osg::Uniform* globeAlphaL = nullptr;
            osg::Uniform* bathyAlphaL = nullptr;

            if (renderComposite.globeQuadLeft())
                globeAlphaL = renderComposite.globeQuadLeft()->getOrCreateStateSet()->getUniform("u_alpha");

            if (renderComposite.bathyQuadLeft())
                bathyAlphaL = renderComposite.bathyQuadLeft()->getOrCreateStateSet()->getUniform("u_alpha");

            float globeAlphaValue = -999.0f;
            float bathyAlphaValue = -999.0f;

            if (globeAlphaL) globeAlphaL->get(globeAlphaValue);
            if (bathyAlphaL) bathyAlphaL->get(bathyAlphaValue);

            dbg
                << "[FIRST_FRAMES_AFTER_GLOBE_SWITCH]"
                << " frame=" << (viewer.getFrameStamp() ? viewer.getFrameStamp()->getFrameNumber() : 0)
                << " simTime=" << (viewer.getFrameStamp() ? viewer.getFrameStamp()->getSimulationTime() : 0.0)
                << " phase=" << phaseName(phaseCoordinator.phase())
                << " masterCull=" << viewer.getCamera()->getCullMask()
                << " globeQuadMaskL=" << (renderComposite.globeQuadLeft() ? renderComposite.globeQuadLeft()->getNodeMask() : 0)
                << " bathyQuadMaskL=" << (renderComposite.bathyQuadLeft() ? renderComposite.bathyQuadLeft()->getNodeMask() : 0)
                << " globeAlphaL=" << globeAlphaValue
                << " bathyAlphaL=" << bathyAlphaValue
                << " globeReady=" << renderComposite.globeTextureReady()
                << " bathyReady=" << renderComposite.bathyTextureReady()
                << "\n";

            --debugFramesAfter;
        }
        currentPhaseForGuard = phaseCoordinator.phase();
        currentCutawayActiveForGuard = phaseCoordinator.bathyCutawayActive();
        // HUD selection logic
        if (!phaseCoordinator.zoomOutActive())
        {
            renderComposite.hideCompositeQuads();
        }

        if (phaseCoordinator.phase() == PhaseCoordinator::AppPhase::GlobeAnimations)
        {
            // during Phase B (fade/aim/dolly), show BOTH quads for crossfade
            if (zoomCtrl.valid() && zoomCtrl->isPhaseBActive())
            {
                renderComposite.showBothCompositeQuads();
            }
            else if (phaseCoordinator.bathyCutawayActive() && phaseCoordinator.cutawayUsingHUD())
            {
                renderComposite.showBothCompositeQuads();
            }
            else
            {
                renderComposite.showGlobeCompositeOnly();
            }
        }

        if (phaseCoordinator.shouldSwitchToGlobePhase(
            intro && intro->isFinished()))
        {
            phaseCoordinator.markSwitchedToGlobe();
            auto* c = viewer.getCamera();
            c->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            c->setClearColor(osg::Vec4(1.0f, 0.0f, 1.0f, 1.0f));
            // c->setPreDrawCallback(nullptr);
            c->setPostDrawCallback(nullptr);
            c->setFinalDrawCallback(nullptr);
            c->setPreDrawCallback(globeClearGuard.get());
            std::cerr << "Intro finished. Num slaves: " << viewer.getNumSlaves() << "\n";
            // remove any slave cameras left by Intro/HUD/RTT
            for (int i = viewer.getNumSlaves() - 1; i >= 0; --i)
            {
                viewer.removeSlave(i);
            }
            std::cerr << "After removeSlave. Num slaves: " << viewer.getNumSlaves() << "\n";

            // remove intro node
            root->removeChild(intro.get());
            // remove handler that still points at intro
            viewer.removeEventHandler(advanceIntro.get());
            advanceIntro = nullptr;

            DisableClearsExceptMaster killer(viewer.getCamera());
            root->accept(killer);
            intro = nullptr;
            phaseCoordinator.setPhase(PhaseCoordinator::AppPhase::GlobeAnimations);

            renderComposite.setMasterCullGlobeOnly();
            renderComposite.setMasterBlackClear();
            renderComposite.setGlobeAlpha(1.0f);
            renderComposite.setBathyAlpha(0.0f);
            renderComposite.showGlobeCompositeOnly();

            mainSceneSystem.setVisible(true);
            globeSequence.setSceneVisible(true);

            double nowT = viewer.getFrameStamp()->getSimulationTime();
            globeSequence.beginAfterIntro(viewer, nowT);
            std::ofstream("C:\\globe_switch_triggered.txt") << "triggered\n";

            debugFramesAfter = 10;
            logFrameState(
            "AFTER_beginAfterIntro",
            viewer,
            phaseCoordinator,
            root.get(),
            mainSceneSystem.scene(),
            globeSequence.scene(),
            particle.get());
        }
        // mid-travel MAIN SCENE Cutaway
        if (handOffs)
        {
            bool cutawayBefore = phaseCoordinator.bathyCutawayActive();
            double nowT = viewer.getFrameStamp()->getSimulationTime();

            logFrameState(
                "BEFORE_updateCutaway",
                viewer,
                phaseCoordinator,
                root.get(),
                mainSceneSystem.scene(),
                globeSequence.scene(),
                particle.get());

            handOffs->updateCutaway(
                viewer,
                nowT,
                waterZ,
                skyColor,
                underwaterColor,
                transitionHeight
            );

            bool cutawayAfter = phaseCoordinator.bathyCutawayActive();

            logFrameState(
                "AFTER_updateCutaway",
                viewer,
                phaseCoordinator,
                root.get(),
                mainSceneSystem.scene(),
                globeSequence.scene(),
                particle.get());

            if (!cutawayBefore && cutawayAfter)
            {
                std::cerr << "[EVENT] CUTAWAY_STARTED\n";
                logFrameState(
                    "CUTAWAY_STARTED",
                    viewer,
                    phaseCoordinator,
                    root.get(),
                    mainSceneSystem.scene(),
                    globeSequence.scene(),
                    particle.get());
            }

            if (cutawayBefore && !cutawayAfter)
            {
                std::cerr << "[EVENT] CUTAWAY_ENDED\n";
                logFrameState(
                    "CUTAWAY_ENDED",
                    viewer,
                    phaseCoordinator,
                    root.get(),
                    mainSceneSystem.scene(),
                    globeSequence.scene(),
                    particle.get());
            }
        }

        if (phaseCoordinator.shouldUpdateGlobeAnimations())
        {
            double nowT = viewer.getFrameStamp()->getSimulationTime();
            globeSequence.update(nowT);
        }

                if (handOffs)
        {
            bool phaseBActive = zoomCtrl.valid() && zoomCtrl->isPhaseBActive();
            bool phaseBDone = zoomCtrl.valid() && zoomCtrl->isPhaseBDone();

            if (phaseBActive && !prevPhaseBActive)
            {
                std::cerr << "[EVENT] PHASE_B_STARTED\n";
                logFrameState(
                    "PHASE_B_STARTED",
                    viewer,
                    phaseCoordinator,
                    root.get(),
                    mainSceneSystem.scene(),
                    globeSequence.scene(),
                    particle.get());
            }

            if (phaseBDone && !prevPhaseBDone)
            {
                std::cerr << "[EVENT] PHASE_B_DONE\n";
                logFrameState(
                    "PHASE_B_DONE",
                    viewer,
                    phaseCoordinator,
                    root.get(),
                    mainSceneSystem.scene(),
                    globeSequence.scene(),
                    particle.get());
            }

            bool shouldInstallFollowCam =
                zoomCtrl.valid() &&
                phaseCoordinator.shouldInstallFollowCam(zoomCtrl->isPhaseBDone());

            if (shouldInstallFollowCam && !followCamWasInstalled)
            {
                std::cerr << "[EVENT] INSTALLING_FOLLOW_CAM\n";
                logFrameState(
                    "BEFORE_setCameraManipulator",
                    viewer,
                    phaseCoordinator,
                    root.get(),
                    mainSceneSystem.scene(),
                    globeSequence.scene(),
                    particle.get());

                viewer.setCameraManipulator(followCam.get());
                followCamWasInstalled = true;

                logFrameState(
                    "AFTER_setCameraManipulator",
                    viewer,
                    phaseCoordinator,
                    root.get(),
                    mainSceneSystem.scene(),
                    globeSequence.scene(),
                    particle.get());
            }

            handOffs->updateFollowCamInstall();
            prevPhaseBActive = phaseBActive;
            prevPhaseBDone = phaseBDone;
        }
        if (phaseCoordinator.shouldFinalizePostDensityMainSceneHide(
            globeSequence.introFinished(),
            globeSequence.densityFinished(),
            mainSceneInitialized))
        {
            std::cerr << "[EVENT] FINALIZE_POST_DENSITY_HIDE_BEGIN\n";
            logFrameState(
                "BEFORE_finalizePostDensityHide",
                viewer,
                phaseCoordinator,
                root.get(),
                mainSceneSystem.scene(),
                globeSequence.scene(),
                particle.get());

            mainSceneInitialized = true;
            mainSceneSystem.setVisible(false);
            mainSceneSystem.hideParticleAndPause();
            globeSequence.setSceneVisible(true);
            mainSceneSystem.removeCutawayEffects(viewer);
            renderComposite.setMasterBlackClear();

            logFrameState(
                "AFTER_finalizePostDensityHide",
                viewer,
                phaseCoordinator,
                root.get(),
                mainSceneSystem.scene(),
                globeSequence.scene(),
                particle.get());

            std::cerr << "[EVENT] FINALIZE_POST_DENSITY_HIDE_END\n";
        }
        // zoom out driver
        if (handOffs)
        {
            double nowT = viewer.getFrameStamp()->getSimulationTime();
            handOffs->updateZoomOut(nowT);
        }
        prevCutawayActive = phaseCoordinator.bathyCutawayActive();
        viewer.frame();

        double endTime = osg::Timer::instance()->time_s();
        double elapsedTime = endTime - startTime;
        if (elapsedTime < frameTime) {
            double sleepTime = frameTime - elapsedTime;
            OpenThreads::Thread::microSleep((unsigned int)(sleepTime * 1e6));
        }
    }

    return 0;
}