#include "HandOffs.h"
#include <iostream>
#include <cmath>
#include "RenderCompositeSystem.h"
#include "MainScene.h"
#include "PhaseCoordinator.h"
#include "../globe/ZoomTransitionController.h"
#include "../scene/FollowCameraManipulator.h"
#include "GlobeSequence.h"
#include "../scene/Particle.h"

HandOffs::HandOffs(PhaseCoordinator& phaseCoordinator,
                   RenderCompositeSystem& renderComposite,
                   MainScene& mainScene,
                   ZoomTransitionController* zoomCtrl)
    : phaseCoordinator_(phaseCoordinator),
      renderComposite_(renderComposite),
      mainScene_(mainScene),
      zoomCtrl_(zoomCtrl)
{
}

void HandOffs::setFollowCamera(FollowCameraManipulator* followCam)
{
    followCam_ = followCam;
}

void HandOffs::setGlobeSequence(GlobeSequence* globeSequence)
{
    globeSequence_ = globeSequence;
}

void HandOffs::setParticle(Particle* particle)
{
    particle_ = particle;
}

void HandOffs::setPhaseBAnchor(const osg::Vec3d& anchor)
{
    savedPhaseBAnchor_ = anchor;
    savedPhaseBAnchorValid_ = true;
}

void HandOffs::clearPhaseBAnchor()
{
    savedPhaseBAnchor_ = osg::Vec3d(0.0, 0.0, 0.0);
    savedPhaseBAnchorValid_ = false;
}

void HandOffs::updateFollowCamInstall()
{
    if (!zoomCtrl_) return;
    if (!followCam_) return;

    if (!phaseCoordinator_.shouldInstallFollowCam(zoomCtrl_->isPhaseBDone()))
        return;

    phaseCoordinator_.markFollowCamInstalled();
    renderComposite_.setMasterCullSceneAndOverlay();
    mainScene_.prepareForFollowCam();
}

void HandOffs::updateCutaway(osgViewer::Viewer& viewer,
                             double nowT,
                             float waterZ,
                             const osg::Vec4& skyColor,
                             const osg::Vec4& underwaterColor,
                             float transitionHeight)
{
    if (!zoomCtrl_) return;
    if (!globeSequence_) return;
    if (!particle_) return;

    const bool travelStarted = globeSequence_->travelStarted();
    const double travelStartT = globeSequence_->travelStartTime();

    if (travelStarted && !loggedTravelStart_)
    {
        std::cerr << "[HANDOFFS_DIAG] travelStarted first true"
                  << " nowT=" << nowT
                  << " travelStartT=" << travelStartT
                  << "\n";
        loggedTravelStart_ = true;
    }

    if (!phaseCoordinator_.shouldProcessTravelCutaway(travelStarted))
        return;

    const double triggerT = travelStartT + 20.0;

    if (!loggedCutawayWindow_ && nowT >= triggerT - 0.25)
    {
        std::cerr << "[HANDOFFS_DIAG] near trigger window"
                  << " nowT=" << nowT
                  << " travelStartT=" << travelStartT
                  << " triggerT=" << triggerT
                  << " delta=" << (nowT - triggerT)
                  << "\n";
        loggedCutawayWindow_ = true;
    }

    // trigger cutaway
    if (!loggedCutawayTrigger_ &&
        phaseCoordinator_.shouldTriggerCutaway(nowT, globeSequence_->travelStartTime()))
    {
        std::cerr << "[HANDOFFS_DIAG] trigger condition became true"
                  << " nowT=" << nowT
                  << " travelStartT=" << globeSequence_->travelStartTime()
                  << " delta=" << (nowT - globeSequence_->travelStartTime())
                  << "\n";
        loggedCutawayTrigger_ = true;
    }

    if (phaseCoordinator_.shouldTriggerCutaway(nowT, globeSequence_->travelStartTime()))
    {
        std::cerr << "[HANDOFFS] CUTAWAY_TRIGGER_ENTER nowT=" << nowT << "\n";

        phaseCoordinator_.beginCutaway(nowT);
        std::cerr << "[HANDOFFS] after beginCutaway\n";

        globeSequence_->setStarfieldVisible(false);
        std::cerr << "[HANDOFFS] after setStarfieldVisible(false)\n";

        renderComposite_.setMasterCullGlobeOnly();
        std::cerr << "[HANDOFFS] after setMasterCullGlobeOnly\n";

        const float startPercent = phaseCoordinator_.cutawayParticleStartPercent();
        std::cerr << "[HANDOFFS] cutawayParticleStartPercent=" << startPercent << "\n";

        osg::Vec3d anchor = particle_->getCurrentPosition();
        std::cerr << "[HANDOFFS] after getCurrentPosition anchor=("
                  << anchor.x() << "," << anchor.y() << "," << anchor.z() << ")\n";
        if (!std::isfinite(anchor.x()) ||
            !std::isfinite(anchor.y()) ||
            !std::isfinite(anchor.z()))
        {
            std::cerr << "[HANDOFFS] invalid anchor, aborting cutaway start\n";
            return;
        }
        setPhaseBAnchor(anchor);
        std::cerr << "[HANDOFFS] after setPhaseBAnchor\n";

        mainScene_.ensureCutawayEffects(
            viewer,
            waterZ,
            skyColor,
            underwaterColor,
            transitionHeight
        );
        std::cerr << "[HANDOFFS] after ensureCutawayEffects\n";

        mainScene_.showParticleForCutaway(startPercent);
        std::cerr << "[HANDOFFS] after showParticleForCutaway\n";

        mainScene_.startParticleBubbleCutawayClock();
        std::cerr << "[HANDOFFS] after startParticleBubbleCutawayClock\n";

        zoomCtrl_->startPhaseB(nowT, anchor);
        std::cerr << "[HANDOFFS] after startPhaseB\n";

        return;
    }
    // end cutaway
    else if (phaseCoordinator_.shouldEndCutaway(nowT))
    {
        std::cerr << "[HANDOFFS] CUTAWAY_END_ENTER nowT=" << nowT << "\n";

        phaseCoordinator_.endCutaway();
        std::cerr << "[HANDOFFS] after endCutaway\n";

        globeSequence_->setStarfieldVisible(true);
        std::cerr << "[HANDOFFS] after setStarfieldVisible(true)\n";

        viewer.setCameraManipulator(nullptr);
        std::cerr << "[HANDOFFS] after setCameraManipulator(nullptr)\n";

        phaseCoordinator_.beginZoomOut();
        std::cerr << "[HANDOFFS] after beginZoomOut\n";

        renderComposite_.setMasterCullGlobeOnly();
        std::cerr << "[HANDOFFS] after setMasterCullGlobeOnly\n";

        mainScene_.setVisible(true);
        std::cerr << "[HANDOFFS] after mainScene.setVisible(true)\n";

        mainScene_.hideParticleAndPause();
        std::cerr << "[HANDOFFS] after hideParticleAndPause\n";

        renderComposite_.showBothCompositeQuads();
        std::cerr << "[HANDOFFS] after showBothCompositeQuads\n";

        renderComposite_.setGlobeAlpha(0.0f);
        std::cerr << "[HANDOFFS] after setGlobeAlpha(0.0f)\n";

        renderComposite_.setBathyAlpha(1.0f);
        std::cerr << "[HANDOFFS] after setBathyAlpha(1.0f)\n";

        mainScene_.removeCutawayEffects(viewer);
        std::cerr << "[HANDOFFS] after removeCutawayEffects\n";

        renderComposite_.setMasterBlackClear();
        std::cerr << "[HANDOFFS] after setMasterBlackClear\n";

        return;
    }
}

void HandOffs::updateZoomOut(double nowT)
{
    if (!zoomCtrl_) return;
    if (!phaseCoordinator_.zoomOutActive()) return;

    // start PhaseB OUT exactly once
    if (phaseCoordinator_.shouldStartZoomOutPhaseB())
    {
        osg::Vec3d anchor =
            savedPhaseBAnchorValid_
                ? savedPhaseBAnchor_
                : osg::Vec3d(0.0, 0.0, 0.0);

        zoomCtrl_->startPhaseBOut(nowT, anchor);
        phaseCoordinator_.markZoomOutPhaseBStarted();
    }

    // when PhaseB OUT finishes, start PhaseA OUT (globe zoom-out) exactly once
    if (phaseCoordinator_.shouldStartZoomOutPhaseA(zoomCtrl_->isPhaseBOutDone()))
    {
        zoomCtrl_->beginPhaseAOut(nowT);
        phaseCoordinator_.markZoomOutPhaseAStarted();
    }

    // when PhaseA OUT finishes, return to normal globe mode
    if (phaseCoordinator_.shouldFinishZoomOut(zoomCtrl_->isPhaseAOutDone()))
    {
        phaseCoordinator_.finishZoomOut();
        renderComposite_.showGlobeCompositeOnly();
        mainScene_.setVisible(false);
        renderComposite_.setMasterCullGlobeOnly();
        renderComposite_.setMasterBlackClear();
    }
}