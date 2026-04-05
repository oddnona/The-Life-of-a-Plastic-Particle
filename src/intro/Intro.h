#pragma once

#include <osg/Group>
#include <osg/Camera>
#include <osg/Array>
#include <osgViewer/Viewer>

class Intro : public osg::Group
{
public:
    Intro(osgViewer::Viewer* viewer, osg::Group* mainScene);

    void start();
    void advanceStage();
    bool isFinished() const;

private:
    class IntroCallback;
    void setOverlayAlpha(double a);
    void finish();

    osgViewer::Viewer* _viewer;
    osg::observer_ptr<osg::Group> _mainScene;
    osg::ref_ptr<osg::Camera> _overlayCam;
    osg::ref_ptr<osg::Vec4Array> _bgColorArray;
    osg::ref_ptr<osg::Vec4Array> _triangleColorArray;
    osg::ref_ptr<IntroCallback> _callback;

    int _stage;
    bool _finished;
};