#include "PhaseCoordinator.h"

PhaseCoordinator::PhaseCoordinator()
    : PhaseCoordinator(Config{})
{
}

PhaseCoordinator::PhaseCoordinator(const Config& config)
    : config_(config)
{
}

PhaseCoordinator::AppPhase PhaseCoordinator::phase() const
{
    return phase_;
}

void PhaseCoordinator::setPhase(AppPhase phase)
{
    phase_ = phase;
}

bool PhaseCoordinator::switchedToGlobe() const
{
    return switchedToGlobe_;
}

void PhaseCoordinator::markSwitchedToGlobe()
{
    switchedToGlobe_ = true;
}

bool PhaseCoordinator::bathyCutawayDone() const
{
    return bathyCutawayDone_;
}

bool PhaseCoordinator::bathyCutawayActive() const
{
    return bathyCutawayActive_;
}

bool PhaseCoordinator::followCamInstalled() const
{
    return followCamInstalled_;
}

bool PhaseCoordinator::cutawayUsingHUD() const
{
    return cutawayUsingHUD_;
}

bool PhaseCoordinator::zoomOutActive() const
{
    return zoomOutActive_;
}

bool PhaseCoordinator::zoomOutPhaseBStarted() const
{
    return zoomOutPhaseBStarted_;
}

bool PhaseCoordinator::zoomOutPhaseAStarted() const
{
    return zoomOutPhaseAStarted_;
}

double PhaseCoordinator::bathyCutawayStartTime() const
{
    return bathyCutawayStartT_;
}

double PhaseCoordinator::bathyCutawayTriggerTime() const
{
    return config_.bathyCutawayTriggerT;
}

double PhaseCoordinator::bathyCutawayDuration() const
{
    return config_.bathyCutawayDurationT;
}

float PhaseCoordinator::cutawayParticleStartPercent() const
{
    return config_.cutawayParticleStartPercent;
}

bool PhaseCoordinator::shouldTriggerCutaway(double nowT, double travelStartT) const
{
    return !bathyCutawayActive_ &&
           !bathyCutawayDone_ &&
           travelStartT >= 0.0 &&
           (nowT - travelStartT) >= config_.bathyCutawayTriggerT;
}

void PhaseCoordinator::beginCutaway(double nowT)
{
    bathyCutawayActive_ = true;
    bathyCutawayStartT_ = nowT;
    cutawayUsingHUD_ = true;
    followCamInstalled_ = false;
}

bool PhaseCoordinator::shouldEndCutaway(double nowT) const
{
    return bathyCutawayActive_ &&
           (nowT - bathyCutawayStartT_) >= config_.bathyCutawayDurationT;
}

void PhaseCoordinator::endCutaway()
{
    bathyCutawayActive_ = false;
    bathyCutawayDone_ = true;
    followCamInstalled_ = false;
}

bool PhaseCoordinator::shouldInstallFollowCam(bool phaseBDone) const
{
    return bathyCutawayActive_ &&
           cutawayUsingHUD_ &&
           phaseBDone &&
           !followCamInstalled_;
}

void PhaseCoordinator::markFollowCamInstalled()
{
    followCamInstalled_ = true;
    cutawayUsingHUD_ = false;
}

void PhaseCoordinator::beginZoomOut()
{
    zoomOutActive_ = true;
    zoomOutPhaseBStarted_ = false;
    zoomOutPhaseAStarted_ = false;
    cutawayUsingHUD_ = true;
}

bool PhaseCoordinator::shouldStartZoomOutPhaseB() const
{
    return zoomOutActive_ && !zoomOutPhaseBStarted_;
}

void PhaseCoordinator::markZoomOutPhaseBStarted()
{
    zoomOutPhaseBStarted_ = true;
}

bool PhaseCoordinator::shouldStartZoomOutPhaseA(bool phaseBOutDone) const
{
    return zoomOutActive_ &&
           zoomOutPhaseBStarted_ &&
           phaseBOutDone &&
           !zoomOutPhaseAStarted_;
}

void PhaseCoordinator::markZoomOutPhaseAStarted()
{
    zoomOutPhaseAStarted_ = true;
}

bool PhaseCoordinator::shouldFinishZoomOut(bool phaseAOutDone) const
{
    return zoomOutActive_ &&
           zoomOutPhaseAStarted_ &&
           phaseAOutDone;
}

void PhaseCoordinator::finishZoomOut()
{
    zoomOutActive_ = false;
    zoomOutPhaseBStarted_ = false;
    zoomOutPhaseAStarted_ = false;
    cutawayUsingHUD_ = false;
}

void PhaseCoordinator::setCutawayUsingHUD(bool value)
{
    cutawayUsingHUD_ = value;
}
bool PhaseCoordinator::shouldSwitchToGlobePhase(bool introFinished) const
{
    return !switchedToGlobe_ &&
           phase_ == AppPhase::Intro &&
           introFinished;
}

bool PhaseCoordinator::shouldUpdateGlobeAnimations() const
{
    return phase_ == AppPhase::GlobeAnimations;
}

bool PhaseCoordinator::shouldProcessTravelCutaway(bool travelStarted) const
{
    return phase_ == AppPhase::GlobeAnimations &&
           travelStarted &&
           !bathyCutawayDone_;
}

bool PhaseCoordinator::shouldFinalizePostDensityMainSceneHide(bool globeIntroFinished,
                                                              bool globeDensityFinished,
                                                              bool mainSceneAlreadyInitialized) const
{
    return phase_ == AppPhase::GlobeAnimations &&
           globeIntroFinished &&
           globeDensityFinished &&
           !mainSceneAlreadyInitialized;
}