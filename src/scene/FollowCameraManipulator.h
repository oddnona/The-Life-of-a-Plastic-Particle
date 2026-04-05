#pragma once

#include <osgGA/CameraManipulator>
#include <osg/Matrix>
#include <osg/Vec3>
#include <osg/BoundingSphere>
#include <algorithm>
#include <osg/Timer>
#include "Particle.h"

class FollowCameraManipulator : public osgGA::CameraManipulator
{
public:
    FollowCameraManipulator(
        Particle* particle,
        float followDistance,
        const osg::BoundingSphere& bathyBounds
    )
        : _particle(particle)
        , _baseDistance(followDistance)
        , _bathyBounds(bathyBounds)
    {}

    void setByMatrix(const osg::Matrixd&) override {}
    void setByInverseMatrix(const osg::Matrixd&) override {}

    osg::Matrixd getMatrix() const override
    {
        return osg::Matrixd::inverse(getInverseMatrix());
    }

    osg::Matrixd getInverseMatrix() const override
    {
        if (!_particle)
            return osg::Matrixd::identity();

        if (_particle->isPaused())
            return computeFollowMatrix();


        return computeFollowMatrix();
    }

    void resetStartLatch()
    {
        _latched = false;
    }
    float getDistanceMultiplier() const
    {
        return _distanceMultiplier;
    }

    void resetDistanceRamp()
    {
        _lastTime = -1.0;
        _elapsedSinceStep = 0.0;
        _distanceMultiplier = 0.55f;
        _lastZoomStepTime = -1.0;
    }

    float getZoomStepPulse() const
    {
        const double now = osg::Timer::instance()->time_s();
        if (_lastZoomStepTime < 0.0) return 0.0f;

        const double age = now - _lastZoomStepTime;
        if (age < 0.0 || age >= _zoomPulseDuration) return 0.0f;
        return 1.0f - static_cast<float>(age / _zoomPulseDuration);
    }

private:

    osg::Matrixd computePausedMatrix() const
    {
        const osg::Vec3 center = _bathyBounds.center();
        const float radius = _bathyBounds.radius();
        osg::Vec3 up(0, 1, 0);
        float height = radius * _pausedHeightFactor;
        float yOffset = -(radius * 0.9);
        osg::Vec3 eye(
            center.x(),
            center.y() + yOffset,
            center.z() + height
        );
        return osg::Matrixd::lookAt(eye, center, up);
    }

    osg::Matrixd computeFollowMatrix() const
    {
        osg::Vec3 p = computeTargetPosition();

        float z = p.z();
        float zoomFactor = 1.0f;

        if (z < -5000.0f)
        {
            float depthBelow = std::min(100.0f, -(z + 50.0f));
            zoomFactor = 1.0f - (depthBelow / 100.0f);
        }
        double now = osg::Timer::instance()->time_s();

        if (_lastTime < 0.0)
        {
            _lastTime = now;
        }

        double delta = now - _lastTime;
        _lastTime = now;
        _elapsedSinceStep += delta;
        if (_elapsedSinceStep >= _stepInterval)
        {
            _elapsedSinceStep = 0.0;
            _distanceMultiplier += _distanceStep;

            const float maxMultiplier = 1.30f;
            if (_distanceMultiplier > maxMultiplier)
                _distanceMultiplier = maxMultiplier;
            _lastZoomStepTime = now;
        }

        float actualDistance =
            (_baseDistance * (0.1f + 0.9f * zoomFactor))
            * _distanceMultiplier;

        const float height = actualDistance * 0.10f; // tilt amount
        osg::Vec3 eye =
            p + osg::Vec3(0.0f, -actualDistance * 0.7f, height);

        osg::Vec3 center = p;
        osg::Vec3 up(0, 0, 1);
        return osg::Matrixd::lookAt(eye, center, up);
    }

    osg::Vec3 computeTargetPosition() const
    {
        if (!_latched)
        {
            // latch to current position so handoff from PhaseB is seamless
            _latchedPosition = _particle->getCurrentPosition();
            _latched = true;
            return _latchedPosition;
        }

        return _particle->getCurrentPosition();
    }

private:
    Particle* _particle = nullptr;
    float _baseDistance = 1.0f;
    osg::BoundingSphere _bathyBounds;
    float _pausedHeightFactor = 1.0f;
    mutable bool _latched = false;
    mutable osg::Vec3 _latchedPosition;
    mutable double _lastTime = -1.0;
    mutable double _elapsedSinceStep = 0.0;
    mutable float _distanceMultiplier = 0.55f;
    float _distanceStep = 0.15f;
    double _stepInterval = 10.0;
    // zoom-step pulse
    mutable double _lastZoomStepTime = -1.0;
    float _zoomPulseDuration = 0.35f;
};