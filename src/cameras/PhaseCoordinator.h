#ifndef PLASTICPARTICLEVE_PHASECOORDINATOR_H
#define PLASTICPARTICLEVE_PHASECOORDINATOR_H

class PhaseCoordinator
{
public:
    enum class AppPhase
    {
        Intro,
        GlobeAnimations,
        MainScene
    };

    struct Config
    {
        double bathyCutawayTriggerT = 19.5;
        double bathyCutawayDurationT = 60.0;
        float cutawayParticleStartPercent = 0.10f;
    };

    PhaseCoordinator();
    explicit PhaseCoordinator(const Config& config);

    AppPhase phase() const;
    void setPhase(AppPhase phase);

    bool switchedToGlobe() const;
    void markSwitchedToGlobe();

    bool bathyCutawayDone() const;
    bool bathyCutawayActive() const;
    bool followCamInstalled() const;
    bool cutawayUsingHUD() const;

    bool zoomOutActive() const;
    bool zoomOutPhaseBStarted() const;
    bool zoomOutPhaseAStarted() const;

    double bathyCutawayStartTime() const;

    double bathyCutawayTriggerTime() const;
    double bathyCutawayDuration() const;
    float cutawayParticleStartPercent() const;

    bool shouldTriggerCutaway(double nowT, double travelStartT) const;
    void beginCutaway(double nowT);

    bool shouldEndCutaway(double nowT) const;
    void endCutaway();

    bool shouldInstallFollowCam(bool phaseBDone) const;
    void markFollowCamInstalled();

    void beginZoomOut();
    bool shouldStartZoomOutPhaseB() const;
    void markZoomOutPhaseBStarted();

    bool shouldStartZoomOutPhaseA(bool phaseBOutDone) const;
    void markZoomOutPhaseAStarted();

    bool shouldFinishZoomOut(bool phaseAOutDone) const;
    void finishZoomOut();

    void setCutawayUsingHUD(bool value);
    bool shouldSwitchToGlobePhase(bool introFinished) const;
    bool shouldUpdateGlobeAnimations() const;
    bool shouldProcessTravelCutaway(bool travelStarted) const;
    bool shouldFinalizePostDensityMainSceneHide(bool globeIntroFinished,
                                                bool globeDensityFinished,
                                                bool mainSceneAlreadyInitialized) const;

private:
    Config config_;

    AppPhase phase_ = AppPhase::Intro;
    bool switchedToGlobe_ = false;

    bool bathyCutawayDone_ = false;
    bool bathyCutawayActive_ = false;
    bool followCamInstalled_ = false;
    bool cutawayUsingHUD_ = false;

    double bathyCutawayStartT_ = -1.0;

    bool zoomOutActive_ = false;
    bool zoomOutPhaseBStarted_ = false;
    bool zoomOutPhaseAStarted_ = false;
};

#endif // PLASTICPARTICLEVE_PHASECOORDINATOR_H