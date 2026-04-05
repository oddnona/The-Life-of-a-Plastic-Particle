#pragma once

#include <osg/Group>
#include <osg/ref_ptr>
#include <osg/Vec3>
#include <osg/Geometry>
#include <osg/Geode>
#include <osg/Array>
#include <osg/PrimitiveSet>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#include <osg/Depth>

#include <vector>
#include <string>
class FollowCameraManipulator;
class Particle : public osg::Group
{
public:
    Particle(const std::string& xyzPath, unsigned int frameStep);

    void update();

    osg::Vec3 getCurrentPosition() const;
    osg::Vec3 getNextPosition() const;
    osg::Vec3 getStartPosition() const;

    bool isAtEnd() const;
    void setPaused(bool p) { _paused = p; }
    bool isPaused() const { return _paused; }

    void seekNormalized(float t01);
    void setSceneRadius(double r) { _sceneRadius = (r > 1e-6) ? r : 1.0; }
    void setFollowCam(FollowCameraManipulator* m) { _followCam = m; }
    void resetBubbleGrowth();
    void startBubbleCutawayClock();
    void updateBubbleNow();

private:
    void init();
    // bubble
    void initBubble();
    void updateBubble();

    // tail tube
    void initTailTube();
    void updateTailTubeWindow();
    void writeTailRing(std::size_t ringIndex);
    void appendTailSegment(std::size_t ringIndex);

    // data
    std::vector<osg::Vec3> _points;

    unsigned int _frameStep = 1;
    unsigned int _frameCounter = 0;
    std::size_t _index = 0;
    bool _valid = false;
    bool _paused = false;
    double _sceneRadius = 1.0;
    // particle drawable
    osg::ref_ptr<osg::ShapeDrawable> _particleDrawable;
    osg::ref_ptr<osg::MatrixTransform> _particleXform;
    osg::ref_ptr<osg::Uniform> _uParticleBaseColor;
    // for bubble growth
    FollowCameraManipulator* _followCam = nullptr;
    // bubble nodes
    osg::ref_ptr<osg::MatrixTransform> _bubbleScaleXform;
    osg::ref_ptr<osg::ShapeDrawable> _bubbleDrawable;
    osg::ref_ptr<osg::Uniform> _uBubbleColor;
    // bubble parameters
    float _bubbleRadiusStart = 500.0f;
    float _bubbleRadiusEnd = 7000.0f;
    // growth window in cutaway time
    float _bubbleGrowStartSec = 45.0f;  // 75% of a 60s cutaway
    float _bubbleGrowEndSec = 48.0f;  //ease
    float _bubbleRadiusCurrent = 500.0f; //radius
    double _bubbleCutawayStartTime = -1.0;
    // tail geometry
    osg::ref_ptr<osg::Geode> _tailGeode;
    osg::ref_ptr<osg::Geometry> _tailGeom;
    osg::ref_ptr<osg::Vec3Array> _tailVtx;
    osg::ref_ptr<osg::Vec3Array> _tailNrm;
    osg::ref_ptr<osg::DrawElementsUInt> _tailIdx;
    // tail config
    std::size_t _tailMaxPoints = 300;
    std::size_t _tailBuiltRings = 0; // how many rings have indices built for
    int _tailSlices = 30;
    float _tailRadius = 350.0f;
};