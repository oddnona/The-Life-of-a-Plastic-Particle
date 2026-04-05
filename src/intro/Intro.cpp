#include "Intro.h"

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/BlendFunc>
#include <osg/NodeCallback>
#include <osg/FrameStamp>
#include <algorithm>

constexpr double FADE_DURATION = 1.0; // seconds

static osg::Geode* createOverlay(osg::Vec4Array** outBgColorArray, osg::Vec4Array** outTriangleColorArray)
{
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    osg::ref_ptr<osg::Geometry> bgGeom = new osg::Geometry;
    osg::ref_ptr<osg::Vec3Array> bgVerts = new osg::Vec3Array;
    bgVerts->push_back(osg::Vec3(0, 0, 0));
    bgVerts->push_back(osg::Vec3(1, 0, 0));
    bgVerts->push_back(osg::Vec3(0, 1, 0));
    bgVerts->push_back(osg::Vec3(1, 1, 0));
    bgGeom->setVertexArray(bgVerts.get());
    bgGeom->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLE_STRIP, 0, 4));
    osg::ref_ptr<osg::Vec4Array> bgColors = new osg::Vec4Array;
    bgColors->push_back(osg::Vec4(0, 0, 0, 1));
    bgGeom->setColorArray(bgColors.get(), osg::Array::BIND_OVERALL);
    if (outBgColorArray) *outBgColorArray = bgColors.get();
    geode->addDrawable(bgGeom.get());
    // Centered grey "play" triangle
    osg::ref_ptr<osg::Geometry> triGeom = new osg::Geometry;
    osg::ref_ptr<osg::Vec3Array> triVerts = new osg::Vec3Array;
    float h = 0.08f;
    float w = 0.045f;
    float cx = 0.5f;
    float cy = 0.5f;
    triVerts->push_back(osg::Vec3(cx - w * 0.5f, cy - h * 0.5f, 0.0f)); // left-bottom
    triVerts->push_back(osg::Vec3(cx - w * 0.5f, cy + h * 0.5f, 0.0f)); // left-top
    triVerts->push_back(osg::Vec3(cx + w * 0.5f, cy, 0.0f)); // right tip
    triGeom->setVertexArray(triVerts.get());
    triGeom->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, 3));
    osg::ref_ptr<osg::Vec4Array> triColors = new osg::Vec4Array;
    triColors->push_back(osg::Vec4(0.35f, 0.35f, 0.35f, 1.0f));
    triGeom->setColorArray(triColors.get(), osg::Array::BIND_OVERALL);
    if (outTriangleColorArray) *outTriangleColorArray = triColors.get();
    geode->addDrawable(triGeom.get());
    osg::StateSet* ss = geode->getOrCreateStateSet();
    ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    ss->setMode(GL_CULL_FACE, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    ss->setMode(GL_BLEND, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    ss->setAttributeAndModes(
        new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA),
        osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE
    );
    ss->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    return geode.release();
}

class Intro::IntroCallback : public osg::NodeCallback
{
public:
    IntroCallback(Intro* intro)
        : _intro(intro), _t0(-1.0) {}
    void resetTime() { _t0 = -1.0; }
    void operator()(osg::Node* node, osg::NodeVisitor* nv) override
    {
        if (_intro->_finished)
        {
            traverse(node, nv);
            return;
        }

        const osg::FrameStamp* fs = nv->getFrameStamp();
        if (!fs)
        {
            traverse(node, nv);
            return;
        }

        if (_t0 < 0.0)
            _t0 = fs->getSimulationTime();

        // Stage 0: hold black + play triangle
        if (_intro->_stage == 0)
        {
            _intro->setOverlayAlpha(1.0);
            traverse(node, nv);
            return;
        }

        double t = (fs->getSimulationTime() - _t0) / FADE_DURATION;
        t = std::clamp(t, 0.0, 1.0);
        double s = t * t * (3.0 - 2.0 * t);
        _intro->setOverlayAlpha(1.0 - s);
        if (t >= 1.0)
            _intro->finish();
        traverse(node, nv);
    }

private:
    Intro* _intro;
    double _t0;
};

Intro::Intro(osgViewer::Viewer* viewer, osg::Group* mainScene)
    : _viewer(viewer),
      _mainScene(mainScene),
      _stage(0),
      _finished(false)
{
    _overlayCam = new osg::Camera;
    _overlayCam->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    _overlayCam->setRenderOrder(osg::Camera::POST_RENDER, 100000);
    _overlayCam->setClearMask(0);
    _overlayCam->setAllowEventFocus(false);
    _overlayCam->setProjectionMatrix(osg::Matrix::ortho2D(0, 1, 0, 1));
    _overlayCam->setViewMatrix(osg::Matrix::identity());

    osg::Vec4Array* bgColorArrayRaw = nullptr;
    osg::Vec4Array* triColorArrayRaw = nullptr;
    osg::ref_ptr<osg::Geode> overlay = createOverlay(&bgColorArrayRaw, &triColorArrayRaw);

    _bgColorArray = bgColorArrayRaw;
    _triangleColorArray = triColorArrayRaw;
    _overlayCam->addChild(overlay.get());
    addChild(_overlayCam.get());
    _callback = new IntroCallback(this);
    _overlayCam->addUpdateCallback(_callback.get());
}

void Intro::start()
{
    _stage = 0;
    _finished = false;

    if (_mainScene.valid())
        _mainScene->setNodeMask(0x0);

    _overlayCam->setNodeMask(0xffffffff);
    setOverlayAlpha(1.0);

    if (_callback.valid())
        _callback->resetTime();
}

void Intro::advanceStage()
{
    if (_finished) return;
    if (_stage >= 1) return;

    _stage = 1;

    if (_mainScene.valid())
        _mainScene->setNodeMask(0xffffffff);

    if (_callback.valid())
        _callback->resetTime();
}

bool Intro::isFinished() const
{
    return _finished;
}

void Intro::setOverlayAlpha(double a)
{
    a = std::clamp(a, 0.0, 1.0);
    const float alpha = static_cast<float>(a);

    if (_bgColorArray.valid() && !_bgColorArray->empty())
    {
        (*_bgColorArray)[0].set(0.0f, 0.0f, 0.0f, alpha);
        _bgColorArray->dirty();
    }

    if (_triangleColorArray.valid() && !_triangleColorArray->empty())
    {
        (*_triangleColorArray)[0].set(0.35f, 0.35f, 0.35f, alpha);
        _triangleColorArray->dirty();
    }
}

void Intro::finish()
{
    _finished = true;
    _overlayCam->setNodeMask(0x0);
    if (_mainScene.valid())
        _mainScene->setNodeMask(0xffffffff);
}