#pragma once
#include <osg/NodeCallback>
#include <osg/observer_ptr>
#include <osg/Node>
#include <osg/Camera>
#include <osg/Geode>
#include <osg/Uniform>
#include <osg/Vec3d>
#include <osg/Matrixd>
#include <functional>

class ZoomTransitionController : public osg::NodeCallback
{
public:
    struct Params
    {
        // phase A
        double durationSec = 2.5;
        double startFovYDeg = 45.0;
        double endFovYDeg = 10.0;
        double phaseB_dollyDistance = 1000000.0;
        osg::Vec3d endEye = osg::Vec3d(0.0, -2.2, 0.0);
        osg::Vec3d endCenter= osg::Vec3d(0.0, 0.0, 0.0);
        osg::Vec3d endUp = osg::Vec3d(0.0, 0.0, 1.0);
        double zNear = 100.0;
        double zFar = 50000000.0;
        double easePower = 1.0;

        // phase B
        double phaseB_fadeSec = 1.0;   // crossfade duration
        double phaseB_aimSec = 1.0;   // how long to re-aim to anchor
        double phaseB_dollySec = 4.0;   // dolly-in duration after fade/aim
        double phaseB_endFovYDeg = 18.0;
    };

    ZoomTransitionController(osg::Camera* master,
                             osg::Geode* globeQuadLeft,
                             osg::Geode* globeQuadRight,
                             osg::Geode* bathyQuadLeft,
                             osg::Geode* bathyQuadRight);

    void setParams(const Params& p) { _p = p; }
    void setSceneRadius(double r) { _sceneRadius = (r > 1e-6) ? r : 1.0; }
    // phase A: track moving node
    void setPhaseATrackNode(osg::Node* n) { _phaseATrackNode = n; }
    // phase B: provide live particle current position
    void setPhaseBAnchorProvider(std::function<osg::Vec3d()> fn) { _phaseBAnchorProvider = std::move(fn); }
    // phase A
    void begin(double nowSimTime);
    bool isPhaseADone() const { return _phaseADone; }
    void clearPhaseADone() { _phaseADone = false; }
    // phase B
    void startPhaseB(double nowSimTime, const osg::Vec3d& anchorWorld);
    bool shouldFreezeGlobeRTT() const
    {
        return _state == State::PhaseB_FadeAim  ||
               _state == State::PhaseB_Dolly    ||
               _state == State::PhaseB_DollyOut;
    }

    bool shouldFreezeBathyRTT() const
    {
        return _state == State::PhaseB_FadeAimOut ||
               _state == State::PhaseA_Out;
    }
    bool isPhaseBDone() const { return _phaseBDone; }
    bool isPhaseBActive() const
    {
        return _state == State::PhaseB_FadeAim   ||
               _state == State::PhaseB_Dolly     ||
               _state == State::PhaseB_DollyOut  ||
               _state == State::PhaseB_FadeAimOut;
    }
    // zoom OUT
    void startPhaseBOut(double nowSimTime, const osg::Vec3d& anchorWorld);
    bool isPhaseBOutDone() const { return _phaseBOutDone; }
    void beginPhaseAOut(double nowSimTime);
    bool isPhaseAOutDone() const { return _phaseAOutDone; }
    const char* debugStateName() const;
    void resetToStart();
    void cancel();
    void operator()(osg::Node* node, osg::NodeVisitor* nv) override;

private:
    enum class State { Idle, PhaseA, PhaseA_Out, PhaseB_FadeAim, PhaseB_Dolly, PhaseB_DollyOut, PhaseB_FadeAimOut };
    void captureStartState();
    void applyPhaseA(double t01);
    void capturePhaseBStart();
    void applyPhaseB_FadeAim(double t01);
    void applyPhaseB_Dolly(double t01);
    void capturePhaseBOutStart();
    void computePhaseBTopDownTargets();
    void applyPhaseB_DollyOut(double t01); // close -> far
    void applyPhaseB_FadeAimOut(double t01); // bathy -> globe
    void applyPhaseAOut(double t01);
    static double clamp01(double x);
    static double smoothstep(double x);
    double ease(double x) const;
    osg::Vec3d getPhaseBAnchorNow() const;

private:
    osg::observer_ptr<osg::Camera> _master;
    osg::observer_ptr<osg::Geode> _globeQuadL;
    osg::observer_ptr<osg::Geode> _globeQuadR;
    osg::observer_ptr<osg::Geode> _bathyQuadL;
    osg::observer_ptr<osg::Geode> _bathyQuadR;

    osg::ref_ptr<osg::Uniform> _globeAlphaL;
    osg::ref_ptr<osg::Uniform> _globeAlphaR;
    osg::ref_ptr<osg::Uniform> _bathyAlphaL;
    osg::ref_ptr<osg::Uniform> _bathyAlphaR;
    osg::observer_ptr<osg::Node> _phaseATrackNode;
    std::function<osg::Vec3d()> _phaseBAnchorProvider;

    Params _p;

    State _state = State::Idle;
    bool _captured = false;
    bool _active = false;
    double _sceneRadius = 1.0; // default fallback
    double _startTime = 0.0;
    double _t = 0.0;
    // phase B readiness gates
    int _phaseBFrames = 0;
    bool _bathyReady = false;
    int _globeReadyFrames = 0;
    bool _globeReady = false;
    int _phaseBDollyFrames = 0;
    int _phaseBDollyWarmupFrames = 3; // how many dolly frames to crossfade out globe
    double _phaseBDollyTotalSec = 0.0;
    double _phaseBDollyRemainingSec = 0.0;
    bool _phaseADone = false;
    // zoom OUT
    bool _phaseBOutDone = false;
    bool _phaseAOutDone = false;
    osg::Vec3d _bInGlobeEye, _bInGlobeCenter, _bInGlobeUp;
    double _bInGlobeFovYDeg = 45.0;
    bool _hasBInGlobe = false;
    osg::Vec3d _aInStartEye, _aInStartCenter, _aInStartUp;
    double _aInStartFovYDeg = 45.0;
    bool _hasAInStart = false;
    // phase B OUT start
    osg::Vec3d _bOutStartEye, _bOutStartCenter, _bOutStartUp;
    double _bOutStartFovYDeg = 45.0;
    // saved baseline for reset
    osg::Matrixd _savedView;
    osg::Matrixd _savedProj;
    bool _hasSaved = false;
    // phase A
    osg::Vec3d _startEye, _startCenter, _startUp;
    double _startFovYDeg = 45.0;
    double _aspect = 1.0;
    // phase B
    osg::Vec3d _anchor;
    bool _phaseBDone = false;
    // phase B start camera
    osg::Vec3d _bStartEye, _bStartCenter, _bStartUp;
    double _bStartFovYDeg = 45.0;
    // phase B targets
    osg::Vec3d _bTargetEye, _bTargetCenter, _bTargetUp;
    double _bTargetFovYDeg = 18.0;
};