#include "ZoomTransitionController.h"
#include <osg/NodeVisitor>
#include <osg/FrameStamp>
#include <cmath>
#include <iostream>
#include <osg/Math>
#include <algorithm>
#include <iomanip>
#include <osg/ComputeBoundsVisitor>
#include <osg/Matrix>

ZoomTransitionController::ZoomTransitionController(osg::Camera* master,
osg::Geode* globeQuadLeft,
osg::Geode* globeQuadRight,
osg::Geode* bathyQuadLeft,
osg::Geode* bathyQuadRight)
: _master(master)
, _globeQuadL(globeQuadLeft)
, _globeQuadR(globeQuadRight)
, _bathyQuadL(bathyQuadLeft)
, _bathyQuadR(bathyQuadRight)
{
    if (_globeQuadL.valid())
        _globeAlphaL = _globeQuadL->getOrCreateStateSet()->getUniform("u_alpha");
    if (_globeQuadR.valid())
        _globeAlphaR = _globeQuadR->getOrCreateStateSet()->getUniform("u_alpha");

    if (_bathyQuadL.valid())
        _bathyAlphaL = _bathyQuadL->getOrCreateStateSet()->getUniform("u_alpha");
    if (_bathyQuadR.valid())
        _bathyAlphaR = _bathyQuadR->getOrCreateStateSet()->getUniform("u_alpha");
}

void ZoomTransitionController::begin(double nowSimTime)
{
    _active = true;
    _captured = false;
    _startTime = nowSimTime;
    _t = 0.0;
    _state = State::PhaseA;
    _phaseBDone = false;
    _phaseBOutDone = false;
    _phaseAOutDone = false;
    _phaseBFrames = 0;
    _bathyReady = false;

    if (_master.valid())
    {
        _savedView = _master->getViewMatrix();
        _savedProj = _master->getProjectionMatrix();
        _hasSaved = true;
    }

    // phase A visuals: globe visible, bathy hidden
    if (_globeAlphaL.valid()) _globeAlphaL->set(1.0f);
    if (_globeAlphaR.valid()) _globeAlphaR->set(1.0f);
    if (_bathyAlphaL.valid()) _bathyAlphaL->set(0.0f);
    if (_bathyAlphaR.valid()) _bathyAlphaR->set(0.0f);
}

void ZoomTransitionController::beginPhaseAOut(double nowSimTime)
{
    if (!_master.valid()) return;
    if (!_hasAInStart) return;
    _active = true;
    _captured = false;
    _startTime = nowSimTime;
    _t = 0.0;
    _state = State::PhaseA_Out;
    _phaseAOutDone = false;

    // during globe zoom-out
    if (_globeAlphaL.valid()) _globeAlphaL->set(1.0f);
    if (_globeAlphaR.valid()) _globeAlphaR->set(1.0f);
    if (_bathyAlphaL.valid()) _bathyAlphaL->set(0.0f);
    if (_bathyAlphaL.valid()) _bathyAlphaL->set(0.0f);

    // capture the CURRENT globe camera as start of zoom-out
    osg::Vec3d eye, center, up;
    _master->getViewMatrixAsLookAt(eye, center, up);
    _startEye = eye;
    _startCenter = center;
    _startUp = up;
    double fovy = 45.0, aspect = 1.0, zNear = 0.01, zFar = 1000.0;
    if (_master->getProjectionMatrixAsPerspective(fovy, aspect, zNear, zFar))
    {
        _startFovYDeg = fovy;
        _aspect = aspect;
    }
    else
    {
        osg::Viewport* vp = _master->getViewport();
        if (vp && vp->height() > 0.0)
            _aspect = vp->width() / vp->height();
        _startFovYDeg = _p.endFovYDeg;
    }
}

void ZoomTransitionController::startPhaseB(double nowSimTime, const osg::Vec3d& anchorWorld)
{
    if (!_master.valid()) return;
    _anchor = anchorWorld;
    _active = true;
    _captured = false;
    _startTime = nowSimTime;
    _t = 0.0;
    _state = State::PhaseB_FadeAim;
    _phaseBDone = false;
    _phaseBFrames = 0;
    _bathyReady = false;
    _phaseBDollyFrames = 0;
    capturePhaseBStart();
    // save the globe view at the start of Phase B IN, so Phase B OUT can aim back to it
    _bInGlobeEye = _bStartEye;
    _bInGlobeCenter = _bStartCenter;
    _bInGlobeUp = _bStartUp;
    _bInGlobeFovYDeg = _bStartFovYDeg;
    _hasBInGlobe = true;
    // start from globe-only
    if (_globeAlphaL.valid()) _globeAlphaL->set(1.0f);
    if (_globeAlphaR.valid()) _globeAlphaR->set(1.0f);
    if (_bathyAlphaL.valid()) _bathyAlphaL->set(0.0f);
    if (_bathyAlphaR.valid()) _bathyAlphaR->set(0.0f);
}

void ZoomTransitionController::startPhaseBOut(double nowSimTime, const osg::Vec3d& anchorWorld)
{
    if (!_master.valid()) return;
    _anchor = anchorWorld;
    _active = true;
    _captured = false;
    _startTime = nowSimTime;
    _t = 0.0;
    _state = State::PhaseB_DollyOut;
    _phaseBDone = false;
    _phaseBOutDone = false;
    _phaseBFrames = 0;
    _bathyReady = false;

    // OUT: reset globe-ready gate
    _globeReadyFrames = 0;
    _globeReady = false;
    capturePhaseBOutStart();
    computePhaseBTopDownTargets();

    // start bathy-only
    if (_globeAlphaL.valid()) _globeAlphaL->set(0.0f);
    if (_globeAlphaR.valid()) _globeAlphaR->set(0.0f);
    if (_bathyAlphaL.valid()) _bathyAlphaL->set(1.0f);
    if (_bathyAlphaR.valid()) _bathyAlphaR->set(1.0f);
}

void ZoomTransitionController::resetToStart()
{
    if (!_master.valid() || !_hasSaved) return;

    _master->setViewMatrix(_savedView);
    _master->setProjectionMatrix(_savedProj);
    _active = false;
    _captured = false;
    _t = 0.0;
    _state = State::Idle;
    // reset blend state
    if (_globeAlphaL.valid()) _globeAlphaL->set(1.0f);
    if (_globeAlphaR.valid()) _globeAlphaR->set(1.0f);
    if (_bathyAlphaL.valid()) _bathyAlphaL->set(0.0f);
    if (_bathyAlphaR.valid()) _bathyAlphaR->set(0.0f);
}

void ZoomTransitionController::cancel()
{
    _active = false;
    _state = State::Idle;
}

void ZoomTransitionController::operator()(osg::Node* node, osg::NodeVisitor* nv)
{
    if (_active && _master.valid() && nv)
    {
        const osg::FrameStamp* fs = nv->getFrameStamp();
        if (fs)
        {
            double now = fs->getSimulationTime();

            if (_state == State::PhaseA)
            {
                if (!_captured)
                {
                    captureStartState();
                    _captured = true;
                }

                double dur = (_p.durationSec > 1e-6) ? _p.durationSec : 1e-6;
                _t = clamp01((now - _startTime) / dur);
                applyPhaseA(_t);

                if (_t >= 1.0)
                {
                    _active = false;
                    _state = State::Idle;
                }
            }
            else if (_state == State::PhaseB_FadeAim)
            {
                // fade and aim happen together
                double dur = (_p.phaseB_fadeSec > 1e-6) ? _p.phaseB_fadeSec : 1e-6;
                double t = clamp01((now - _startTime) / dur);
                applyPhaseB_FadeAim(t);

                if (t >= 1.0)
                {
                    // start dolly phase
                    _state = State::PhaseB_Dolly;
                    _phaseBDollyFrames = 0;
                    _startTime = now;
                }
            }
            else if (_state == State::PhaseB_Dolly)
            {

                const double oldBlendSec = 3.0;
                const double newBlendSec = 10.0;
                const double oldDur = (_p.phaseB_dollySec > 1e-6) ? _p.phaseB_dollySec : 1e-6;
                const double preDur = std::max(0.0, oldDur - oldBlendSec);
                const double totalDur = preDur + newBlendSec;
                const double elapsed = std::max(0.0, now - _startTime);
                _phaseBDollyTotalSec = totalDur;
                _phaseBDollyRemainingSec = std::max(0.0, totalDur - elapsed);

                double t01 = 0.0;
                if (elapsed <= preDur || preDur <= 1e-9)
                {
                    t01 = clamp01(elapsed / oldDur);
                }
                else
                {
                    const double tStart = clamp01(preDur / oldDur);
                    const double tailElapsed = elapsed - preDur;
                    const double tail01 = clamp01(tailElapsed / newBlendSec);
                    t01 = tStart + (1.0 - tStart) * tail01;
                    t01 = clamp01(t01);
                }

                applyPhaseB_Dolly(t01);
                if (elapsed >= totalDur)
                {
                    _active = false;
                    _state = State::Idle;
                    _phaseBDone = true;
                }
            }
            else if (_state == State::PhaseB_DollyOut)
            {
                double dur = (_p.phaseB_dollySec > 1e-6) ? _p.phaseB_dollySec : 1e-6;
                double t = clamp01((now - _startTime) / dur);
                applyPhaseB_DollyOut(t);

                if (t >= 1.0)
                {
                    // after dolly-out, do Fade/Aim OUT (bathy->globe)
                    _state = State::PhaseB_FadeAimOut;
                    _phaseBFrames = 0;
                    _bathyReady = false;
                    _globeReadyFrames = 0;
                    _globeReady = false;
                    _startTime = now;
                }
            }
            else if (_state == State::PhaseB_FadeAimOut)
            {
                double dur = (_p.phaseB_fadeSec > 1e-6) ? _p.phaseB_fadeSec : 1e-6;
                double t = clamp01((now - _startTime) / dur);
                applyPhaseB_FadeAimOut(t);

                if (t >= 1.0)
                {
                    _active = false;
                    _state = State::Idle;
                    _phaseBOutDone = true;
                }
            }
            else if (_state == State::PhaseA_Out)
            {
                double dur = (_p.durationSec > 1e-6) ? _p.durationSec : 1e-6;
                double t = clamp01((now - _startTime) / dur);
                applyPhaseAOut(t);

                if (t >= 1.0)
                {
                    _active = false;
                    _state = State::Idle;
                    _phaseAOutDone = true;
                }
            }
        }
    }

    traverse(node, nv);
}

void ZoomTransitionController::captureStartState()
{
    if (!_master) return;
    osg::Vec3d eye, center, up;
    _master->getViewMatrixAsLookAt(eye, center, up);
    _startEye = eye;
    _startCenter = center;
    _startUp = up;
    double fovy = 45.0, aspect = 1.0, zNear = 0.01, zFar = 1000.0;
    if (_master->getProjectionMatrixAsPerspective(fovy, aspect, zNear, zFar))
    {
        _startFovYDeg = fovy;
        _aspect = aspect;
    }
    else
    {
        osg::Viewport* vp = _master->getViewport();
        if (vp && vp->height() > 0.0)
            _aspect = vp->width() / vp->height();
        _startFovYDeg = _p.startFovYDeg;
    }
    if (!_hasAInStart)
    {
        _aInStartEye = _startEye;
        _aInStartCenter = _startCenter;
        _aInStartUp = _startUp;
        _aInStartFovYDeg = _startFovYDeg;
        _hasAInStart = true;
    }
}

static osg::Vec3d getWorldPosition(osg::Node* n)
{
    if (!n) return osg::Vec3d(0.0, 0.0, 0.0);
    osg::NodePathList paths = n->getParentalNodePaths();
    if (paths.empty()) return osg::Vec3d(0.0, 0.0, 0.0);
    osg::Matrixd world = osg::computeLocalToWorld(paths.front());
    return world.getTrans();
}

osg::Vec3d ZoomTransitionController::getPhaseBAnchorNow() const
{
    if (_phaseBAnchorProvider)
        return _phaseBAnchorProvider();

    return _anchor;
}

void ZoomTransitionController::applyPhaseA(double t01)
{
    if (!_master) return;
    const double te = ease(t01);

    // default Phase A fixed target
    osg::Vec3d endCenter = _p.endCenter;
    osg::Vec3d endEye = _p.endEye;

    // keep the same camera offset, but centered on the moving target
    if (_phaseATrackNode.valid())
    {
        osg::Vec3d anchor = getWorldPosition(_phaseATrackNode.get());
        osg::Vec3d offset = _p.endEye - _p.endCenter;
        endCenter = anchor;
        endEye = anchor + offset;
    }

    osg::Vec3d eye = _startEye * (1.0 - te) + endEye * te;
    osg::Vec3d center = _startCenter * (1.0 - te) + endCenter * te;
    osg::Vec3d up = _startUp * (1.0 - te) + _p.endUp * te;
    up.normalize();
    _master->setViewMatrixAsLookAt(eye, center, up);
    double fovy = _startFovYDeg * (1.0 - te) + _p.endFovYDeg * te;
    double aspect = _aspect;
    osg::Viewport* vp = _master->getViewport();
    if (vp && vp->height() > 0.0)
        aspect = vp->width() / vp->height();
    _master->setProjectionMatrixAsPerspective(fovy, aspect, _p.zNear, _p.zFar);
}

void ZoomTransitionController::capturePhaseBStart()
{
    osg::Vec3d eye, center, up;
    _master->getViewMatrixAsLookAt(eye, center, up);
    _bStartEye = eye;
    _bStartCenter = center;
    _bStartUp = up;

    double fovy = 45.0, aspect = 1.0, zNear = 0.01, zFar = 1000.0;
    if (_master->getProjectionMatrixAsPerspective(fovy, aspect, zNear, zFar))
        _bStartFovYDeg = fovy;
    else
        _bStartFovYDeg = _p.phaseB_endFovYDeg;
    // aim straight down onto bathymetry (Z-up world)
    _bTargetCenter = _anchor;
    _bTargetUp = osg::Vec3d(0.0, 1.0, 0.0);
    // straight down toward bathy (negative Z)
    osg::Vec3d forward = osg::Vec3d(0.0, 0.0, -1.0);
    osg::Viewport* vp = _master->getViewport();
    double aspectt = 1.0;
    if (vp && vp->height() > 0.0) aspectt = vp->width() / vp->height();
    double vfovRad = osg::DegreesToRadians(_p.phaseB_endFovYDeg);
    double vHalf = 0.5 * vfovRad;
    double hHalf = std::atan(std::tan(vHalf) * aspectt);
    double halfFov = std::min(vHalf, hHalf);
    double fitDist = (_sceneRadius / std::tan(halfFov)) * 0.1;
    _bTargetEye = _anchor - forward * fitDist;
    _bTargetFovYDeg = _p.phaseB_endFovYDeg;
}

void ZoomTransitionController::capturePhaseBOutStart()
{
    osg::Vec3d eye, center, up;
    _master->getViewMatrixAsLookAt(eye, center, up);
    _bOutStartEye = eye;
    _bOutStartCenter = center;
    _bOutStartUp = up;
    double fovy = 45.0, aspect = 1.0, zNear = 0.01, zFar = 1000.0;
    if (_master->getProjectionMatrixAsPerspective(fovy, aspect, zNear, zFar))
        _bOutStartFovYDeg = fovy;
    else
        _bOutStartFovYDeg = _p.phaseB_endFovYDeg;
}

void ZoomTransitionController::computePhaseBTopDownTargets()
{
    // same logic as capturePhaseBStart()
    _bTargetCenter = _anchor;
    _bTargetUp = osg::Vec3d(0.0, 1.0, 0.0);
    osg::Vec3d forward = osg::Vec3d(0.0, 0.0, -1.0);
    osg::Viewport* vp = _master->getViewport();
    double aspectt = 1.0;
    if (vp && vp->height() > 0.0) aspectt = vp->width() / vp->height();

    double vfovRad = osg::DegreesToRadians(_p.phaseB_endFovYDeg);
    double vHalf = 0.5 * vfovRad;
    double hHalf = std::atan(std::tan(vHalf) * aspectt);
    double halfFov = std::min(vHalf, hHalf);
    double fitDist = (_sceneRadius / std::tan(halfFov)) * 0.1;
    _bTargetEye = _anchor - forward * fitDist;
    _bTargetFovYDeg = _p.phaseB_endFovYDeg;
}

void ZoomTransitionController::applyPhaseB_FadeAim(double t01)
{
    // AIM completes in the first 25% of this phase
    const double AIM_PORTION = 0.25;
    double aim01 = clamp01(t01 / AIM_PORTION);
    double teAim = ease(aim01);

    // FADE starts after 60% of this phase
    const double FADE_DELAY = 0.60;
    double fade01 = clamp01((t01 - FADE_DELAY) / (1.0 - FADE_DELAY));
    double teFade = ease(fade01);
    if (!_bathyReady)
    {
        ++_phaseBFrames;
        if (_phaseBFrames >= 3) _bathyReady = true; // wait 3 frames
    }

    if (!_bathyReady)
    {
        // until bathy RTT has had time to render
        if (_globeAlphaL.valid()) _globeAlphaL->set(1.0f);
        if (_globeAlphaR.valid()) _globeAlphaR->set(1.0f);
        if (_bathyAlphaL.valid()) _bathyAlphaL->set(0.0f);
        if (_bathyAlphaR.valid()) _bathyAlphaR->set(0.0f);
    }
    else
    {
        // crossfade, delayed
        if (_globeAlphaL.valid()) _globeAlphaL->set((float)(1.0 - teFade));
        if (_globeAlphaR.valid()) _globeAlphaR->set((float)(1.0 - teFade));
        if (_bathyAlphaL.valid()) _bathyAlphaL->set((float)(teFade));
        if (_bathyAlphaR.valid()) _bathyAlphaR->set((float)(teFade));
    }

    // Phase B anchor: recompute top-down targets every frame
    const osg::Vec3d anchorNow = getPhaseBAnchorNow();
    const osg::Vec3d forward(0.0, 0.0, -1.0);  // look down
    const osg::Vec3d upTopDown(0.0, 1.0, 0.0);
    osg::Viewport* vpTD = _master->getViewport();
    double aspectTD = 1.0;
    if (vpTD && vpTD->height() > 0.0) aspectTD = vpTD->width() / vpTD->height();
    const double vfovRad = osg::DegreesToRadians(_p.phaseB_endFovYDeg);
    const double vHalf = 0.5 * vfovRad;
    const double hHalf = std::atan(std::tan(vHalf) * aspectTD);
    const double halfFov = std::min(vHalf, hHalf);
    const double fitDist = (_sceneRadius / std::tan(halfFov)) * 0.1;
    _bTargetCenter = anchorNow;
    _bTargetUp = upTopDown;
    _bTargetEye = anchorNow - forward * fitDist;
    _bTargetFovYDeg = _p.phaseB_endFovYDeg;
    // aim camera toward anchor
    osg::Vec3d eye    = _bStartEye    * (1.0 - teAim) + _bTargetEye    * teAim;
    osg::Vec3d center = _bStartCenter * (1.0 - teAim) + _bTargetCenter * teAim;
    osg::Vec3d up     = _bStartUp     * (1.0 - teAim) + _bTargetUp     * teAim;
    up.normalize();

    _master->setViewMatrixAsLookAt(eye, center, up);
    double fovy = _bStartFovYDeg * (1.0 - teAim) + _bTargetFovYDeg * teAim;
    osg::Viewport* vp = _master->getViewport();
    double aspect = 1.0;
    if (vp && vp->height() > 0.0) aspect = vp->width() / vp->height();
    double dist = (center - eye).length();
    double zn = std::max(1.0, _p.zNear);
    double zf = std::max(_p.zFar, dist * 2.0);
    _master->setProjectionMatrixAsPerspective(fovy, aspect, zn, zf);
}

void ZoomTransitionController::applyPhaseB_Dolly(double t01)
{
    const double te = ease(t01);
    if (_globeAlphaL.valid()) _globeAlphaL->set(0.0f);
    if (_globeAlphaR.valid()) _globeAlphaR->set(0.0f);
    if (_bathyAlphaL.valid()) _bathyAlphaL->set(1.0f);
    if (_bathyAlphaR.valid()) _bathyAlphaR->set(1.0f);
    // update center every frame.
    const osg::Vec3d anchorNow = getPhaseBAnchorNow();
    const osg::Vec3d forwardTD(0.0, 0.0, -1.0);
    const osg::Vec3d upTD(0.0, 1.0, 0.0);
    const osg::Vec3d cen = anchorNow;
    osg::Viewport* vpTD = _master->getViewport();
    double aspectTD = 1.0;
    if (vpTD && vpTD->height() > 0.0) aspectTD = vpTD->width() / vpTD->height();
    const double vfovRad = osg::DegreesToRadians(_p.phaseB_endFovYDeg);
    const double vHalf = 0.5 * vfovRad;
    const double hHalf = std::atan(std::tan(vHalf) * aspectTD);
    const double halfFov = std::min(vHalf, hHalf);
    const double distOut = std::max(1.0, (_sceneRadius / std::tan(halfFov)) * 0.1);
    const double distIn = std::max(1.0, _p.phaseB_dollyDistance);
    // dolly distance interpolation
    const double dist = distOut * (1.0 - te) + distIn * te;
    const osg::Vec3d eyeTopDown = cen - forwardTD * dist;
    const double fovyTopDown = _p.phaseB_endFovYDeg * (1.0 - te) + (_p.phaseB_endFovYDeg * 0.85) * te;
    // replicates FollowCameraManipulator::computeFollowMatrix() logic
    double zoomFactor = 1.0;
    const double z = cen.z();
    if (z < -5000.0)
    {
        const double depthBelow = std::min(100.0, -(z + 50.0));
        zoomFactor = 1.0 - (depthBelow / 100.0);
    }

    const double actualDistance = _p.phaseB_dollyDistance * (0.1 + 0.9 * zoomFactor);
    const osg::Vec3d eyeFollow = cen + osg::Vec3d(0.0, -actualDistance * 0.7, 0.0);
    const osg::Vec3d upFollow(0.0, 0.0, 1.0);
    // blend window: last 10 seconds of PhaseB dolly
    const double remainingSec = _phaseBDollyRemainingSec;
    const double blendWindowSec = 10.0;
    double w = 0.0;
    if (remainingSec <= blendWindowSec)
    {
        // w: 0 at start of window -> 1 at end of dolly
        w = clamp01((blendWindowSec - remainingSec) / blendWindowSec);
        w = ease(w);
    }

    const osg::Vec3d eye = eyeTopDown * (1.0 - w) + eyeFollow * w;
    osg::Vec3d up = upTD * (1.0 - w) + upFollow * w;
    up.normalize();
    _master->setViewMatrixAsLookAt(eye, cen, up);
    osg::Viewport* vp = _master->getViewport();
    double aspect = 1.0;
    if (vp && vp->height() > 0.0) aspect = vp->width() / vp->height();
    double zn = 100000.0;
    double zf = 100000000.0;
    _master->setProjectionMatrixAsPerspective(fovyTopDown, aspect, zn, zf);
}

void ZoomTransitionController::applyPhaseB_DollyOut(double t01)
{
    const double te = ease(t01);
    if (_globeAlphaL.valid()) _globeAlphaL->set(0.0f);
    if (_globeAlphaR.valid()) _globeAlphaR->set(0.0f);
    if (_bathyAlphaL.valid()) _bathyAlphaL->set(1.0f);
    if (_bathyAlphaR.valid()) _bathyAlphaR->set(1.0f);
    osg::Vec3d cen = _bTargetCenter;
    osg::Vec3d up = _bTargetUp;
    osg::Vec3d eyeFar = _bTargetEye;
    osg::Vec3d eyeClose = _bOutStartEye; // close eye at cutaway end
    osg::Vec3d forwardFar = (cen - eyeFar);
    double distFar = forwardFar.length();
    if (distFar < 1e-6) distFar = 1.0;
    forwardFar.normalize();
    osg::Vec3d forwardClose = (cen - eyeClose);
    double distClose = forwardClose.length();
    if (distClose < 1e-6) distClose = std::max(1.0, _p.phaseB_dollyDistance);

    // same distance interpolation logic, but with reversed endpoints, close -> far
    double dist = distClose * (1.0 - te) + distFar * te;
    osg::Vec3d eye = cen - forwardFar * dist;
    _master->setViewMatrixAsLookAt(eye, cen, up);
    double fovy = (_bTargetFovYDeg * 0.85) * (1.0 - te) + _bTargetFovYDeg * te;

    osg::Viewport* vp = _master->getViewport();
    double aspect = 1.0;
    if (vp && vp->height() > 0.0) aspect = vp->width() / vp->height();
    double zn = 100000.0;
    double zf = 100000000.0;
    _master->setProjectionMatrixAsPerspective(fovy, aspect, zn, zf);
}

void ZoomTransitionController::applyPhaseB_FadeAimOut(double t01)
{
    const double AIM_PORTION = 0.25;
    double aim01 = clamp01(t01 / AIM_PORTION);
    double teAim = ease(aim01);
    const double FADE_DELAY = 0.60;
    double fade01 = clamp01((t01 - FADE_DELAY) / (1.0 - FADE_DELAY));
    double teFade = ease(fade01);
    if (!_globeReady)
    {
        ++_globeReadyFrames;
        if (_globeReadyFrames >= 3) _globeReady = true;
    }

    if (!_globeReady)
    {
        if (_globeAlphaL.valid()) _globeAlphaL->set(0.0f);
        if (_globeAlphaR.valid()) _globeAlphaR->set(0.0f);
        if (_bathyAlphaL.valid()) _bathyAlphaL->set(1.0f);
        if (_bathyAlphaR.valid()) _bathyAlphaR->set(1.0f);
    }
    else
    {
        // crossfade OUT, delayed
        if (_globeAlphaL.valid()) _globeAlphaL->set((float)(teFade));
        if (_globeAlphaR.valid()) _globeAlphaR->set((float)(teFade));
        if (_bathyAlphaL.valid()) _bathyAlphaL->set((float)(1.0 - teFade));
        if (_bathyAlphaR.valid()) _bathyAlphaR->set((float)(1.0 - teFade));
    }

    osg::Vec3d startEye = _bTargetEye;     // far top-down bathy
    osg::Vec3d startCenter = _bTargetCenter;
    osg::Vec3d startUp = _bTargetUp;
    double startFov = _bTargetFovYDeg;

    osg::Vec3d endEye = _hasBInGlobe ? _bInGlobeEye : _bOutStartEye;
    osg::Vec3d endCenter = _hasBInGlobe ? _bInGlobeCenter : _bOutStartCenter;
    osg::Vec3d endUp = _hasBInGlobe ? _bInGlobeUp : _bOutStartUp;
    double endFov = _hasBInGlobe ? _bInGlobeFovYDeg : _bOutStartFovYDeg;

    osg::Vec3d eye = startEye * (1.0 - teAim) + endEye * teAim;
    osg::Vec3d center = startCenter * (1.0 - teAim) + endCenter * teAim;
    osg::Vec3d up = startUp * (1.0 - teAim) + endUp * teAim;
    up.normalize();
    _master->setViewMatrixAsLookAt(eye, center, up);
    double fovy = startFov * (1.0 - teAim) + endFov * teAim;
    osg::Viewport* vp = _master->getViewport();
    double aspect = 1.0;
    if (vp && vp->height() > 0.0) aspect = vp->width() / vp->height();
    double dist = (center - eye).length();
    double zn = std::max(1.0, _p.zNear);
    double zf = std::max(_p.zFar, dist * 2.0);
    _master->setProjectionMatrixAsPerspective(fovy, aspect, zn, zf);
}

void ZoomTransitionController::applyPhaseAOut(double t01)
{
    if (!_master) return;
    const double te = ease(t01);
    // interpolate from current back to PhaseA IN start
    osg::Vec3d endEye = _aInStartEye;
    osg::Vec3d endCenter = _aInStartCenter;
    osg::Vec3d endUp = _aInStartUp;
    double endFov = _aInStartFovYDeg;
    osg::Vec3d eye = _startEye * (1.0 - te) + endEye * te;
    osg::Vec3d center = _startCenter * (1.0 - te) + endCenter * te;
    osg::Vec3d up = _startUp * (1.0 - te) + endUp * te;
    up.normalize();

    _master->setViewMatrixAsLookAt(eye, center, up);
    double fovy = _startFovYDeg * (1.0 - te) + endFov * te;
    osg::Viewport* vp = _master->getViewport();
    double aspect = _aspect;
    if (vp && vp->height() > 0.0)
        aspect = vp->width() / vp->height();
    _master->setProjectionMatrixAsPerspective(fovy, aspect, _p.zNear, _p.zFar);
}

double ZoomTransitionController::clamp01(double x)
{
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

double ZoomTransitionController::smoothstep(double x)
{
    x = clamp01(x);
    return x * x * (3.0 - 2.0 * x);
}

double ZoomTransitionController::ease(double x) const
{
    double s = smoothstep(x);
    if (_p.easePower <= 1.0) return s;
    return std::pow(s, _p.easePower);
}

const char* ZoomTransitionController::debugStateName() const
{
    switch (_state)
    {
        case State::Idle: return "Idle";
        case State::PhaseA: return "PhaseA";
        case State::PhaseA_Out: return "PhaseA_Out";
        case State::PhaseB_FadeAim: return "PhaseB_FadeAim";
        case State::PhaseB_Dolly: return "PhaseB_Dolly";
        case State::PhaseB_DollyOut: return "PhaseB_DollyOut";
        case State::PhaseB_FadeAimOut: return "PhaseB_FadeAimOut";
        default: return "Unknown";
    }
}