#include "RenderCompositeSystem.h"

#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/FrameBufferObject>
#include <osg/Geometry>
#include <osg/Program>
#include <osg/Shader>
#include <osg/StateSet>
#include <osg/Uniform>
#include <cstring>
#include <iostream>

namespace
{
    enum class StereoEye
    {
        Left,
        Right
    };

    static unsigned int eyeQuadMask(StereoEye eye)
    {
        return (eye == StereoEye::Left) ? 0x1 : 0x2;
    }
    struct MirrorMasterCameraMatrices : public osg::NodeCallback
    {
        osg::observer_ptr<osg::Camera> master;
        osg::observer_ptr<osg::Camera> rtt;
        osg::observer_ptr<ZoomTransitionController> zoomCtrl;

        enum class FreezeMode { None, Globe, Bathy };
        enum class Eye { Left, Right };

        FreezeMode freezeMode = FreezeMode::None;
        Eye eye = Eye::Left;

        MirrorMasterCameraMatrices(osg::Camera* m,
                                   osg::Camera* c,
                                   ZoomTransitionController* z,
                                   FreezeMode mode,
                                   Eye e)
            : master(m), rtt(c), zoomCtrl(z), freezeMode(mode), eye(e) {}

        void operator()(osg::Node* node, osg::NodeVisitor* nv) override
        {
            bool freeze = false;
            if (zoomCtrl.valid())
            {
                if (freezeMode == FreezeMode::Globe) freeze = zoomCtrl->shouldFreezeGlobeRTT();
                if (freezeMode == FreezeMode::Bathy) freeze = zoomCtrl->shouldFreezeBathyRTT();
            }

            if (!freeze && master.valid() && rtt.valid())
            {
                osg::DisplaySettings* ds = osg::DisplaySettings::instance();

                const double baseEyeSep = ds ? ds->getEyeSeparation() : 0.01;
                const double compositeStereoScale = 0.29;
//                const double eyeSep = baseEyeSep * compositeStereoScale;           FOR STEREO
                const double eyeSep = 0.0;                                         //FOR MONO
                const double screenDist = ds ? ds->getScreenDistance() : 0.5;

                osg::Vec3d eyePos, center, up;
                master->getViewMatrixAsLookAt(eyePos, center, up);

                osg::Vec3d forward = center - eyePos;
                if (forward.length2() < 1e-12)
                {
                    traverse(node, nv);
                    return;
                }
                forward.normalize();

                osg::Vec3d right = forward ^ up;
                if (right.length2() < 1e-12)
                {
                    traverse(node, nv);
                    return;
                }
                right.normalize();

                const double sign = (eye == Eye::Left) ? -1.0 : 1.0;
                const double eyeOffset = 0.5 * eyeSep * sign;
                osg::Vec3d shiftedEye = eyePos + right * eyeOffset;
                osg::Vec3d shiftedCenter = center + right * eyeOffset;
                rtt->setViewMatrixAsLookAt(shiftedEye, shiftedCenter, up);

                double fovy = 45.0;
                double aspect = 1.0;
                double zNear = 0.01;
                double zFar = 1000.0;

                if (master->getProjectionMatrixAsPerspective(fovy, aspect, zNear, zFar))
                {
                    const double top = zNear * std::tan(osg::DegreesToRadians(fovy * 0.5));
                    const double rightPlane = top * aspect;

//                    const double frustumShift = (eyeOffset * zNear) / std::max(1e-9, screenDist);                  FOR STEREO
                    const double frustumShift = 0.0;                                                               //FOR MONO

                    double leftVal = -rightPlane + frustumShift;
                    double rightVal = rightPlane + frustumShift;
                    double bottomVal = -top;
                    double topVal = top;

                    rtt->setProjectionMatrixAsFrustum(
                        leftVal,
                        rightVal,
                        bottomVal,
                        topVal,
                        zNear,
                        zFar
                    );
                }
                else
                {
                    rtt->setProjectionMatrix(master->getProjectionMatrix());
                }
            }

            traverse(node, nv);
        }
    };

    struct MarkRenderedOnce : public osg::Camera::DrawCallback
    {
        bool* flag = nullptr;
        explicit MarkRenderedOnce(bool* f) : flag(f) {}
        void operator()(osg::RenderInfo&) const override
        {
            if (flag) *flag = true;
        }
    };

    static void applyTextureShader(osg::StateSet* ss)
    {
        const char* vsrc =
            "varying vec2 vTex;\n"
            "void main(){\n"
            "  gl_Position = ftransform();\n"
            "  vTex = gl_MultiTexCoord0.st;\n"
            "}\n";

        const char* fsrc =
            "uniform sampler2D tex0;\n"
            "uniform float u_alpha;\n"
            "varying vec2 vTex;\n"
            "void main(){\n"
            "  vec4 c = texture2D(tex0, vTex);\n"
            "  c.a *= u_alpha;\n"
            "  gl_FragColor = c;\n"
            "}\n";

        osg::ref_ptr<osg::Shader> vs = new osg::Shader(osg::Shader::VERTEX, vsrc);
        osg::ref_ptr<osg::Shader> fs = new osg::Shader(osg::Shader::FRAGMENT, fsrc);
        osg::ref_ptr<osg::Program> prog = new osg::Program;
        prog->addShader(vs.get());
        prog->addShader(fs.get());
        ss->setAttributeAndModes(prog.get(), osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
        ss->addUniform(new osg::Uniform("tex0", 0));
        ss->addUniform(new osg::Uniform("u_alpha", 1.0f));
    }

    static osg::ref_ptr<osg::Camera> makeFullscreenQuadCamera(GLenum drawBuffer)
    {
        osg::ref_ptr<osg::Camera> cam = new osg::Camera;
        cam->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        cam->setRenderOrder(osg::Camera::POST_RENDER, 100000);
        cam->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER);
        cam->setClearMask(0);
        cam->setAllowEventFocus(false);
        cam->setProjectionMatrix(osg::Matrix::ortho2D(0, 1, 0, 1));
        cam->setViewMatrix(osg::Matrix::identity());
        cam->setDrawBuffer(drawBuffer);
        cam->setReadBuffer(drawBuffer);

        return cam;
    }

    static osg::ref_ptr<osg::Texture2D> makeRTTTexture(int w, int h)
    {
        osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D;
        tex->setTextureSize(w, h);
        tex->setInternalFormat(GL_RGBA);
        tex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        tex->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        tex->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        tex->setResizeNonPowerOfTwoHint(false);
        osg::ref_ptr<osg::Image> img = new osg::Image;
        img->allocateImage(w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE);
        memset(img->data(), 0, img->getTotalSizeInBytes());
        tex->setImage(img.get());
        return tex;
    }

    static osg::ref_ptr<osg::Camera> makeRTTCamera(osg::Texture2D* colorTex,
                                                   int w, int h,
                                                   int renderOrder)
    {
        osg::ref_ptr<osg::Camera> cam = new osg::Camera;

        cam->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        cam->setRenderOrder(osg::Camera::PRE_RENDER, renderOrder);
        cam->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        cam->setClearColor(osg::Vec4(0, 0, 0, 1));
        cam->setViewport(0, 0, w, h);
        cam->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        cam->attach(osg::Camera::COLOR_BUFFER0, colorTex);
        cam->attach(osg::Camera::DEPTH_BUFFER, GL_DEPTH_COMPONENT24);

        cam->setAllowEventFocus(false);

        return cam;
    }

    static osg::ref_ptr<osg::Geode> makeFullscreenTexturedQuadGeode(osg::Texture2D* tex,
                                                                    const osg::Vec4& color,
                                                                    StereoEye eye)
    {
        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        geode->setNodeMask(0xffffffff);
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;
        verts->push_back(osg::Vec3(0, 0, 0));
        verts->push_back(osg::Vec3(1, 0, 0));
        verts->push_back(osg::Vec3(0, 1, 0));
        verts->push_back(osg::Vec3(1, 1, 0));
        geom->setVertexArray(verts.get());
        osg::ref_ptr<osg::Vec2Array> uvs = new osg::Vec2Array;
        uvs->push_back(osg::Vec2(0, 0));
        uvs->push_back(osg::Vec2(1, 0));
        uvs->push_back(osg::Vec2(0, 1));
        uvs->push_back(osg::Vec2(1, 1));
        geom->setTexCoordArray(0, uvs.get());

        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;
        colors->push_back(color);
        geom->setColorArray(colors.get(), osg::Array::BIND_OVERALL);
        geom->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLE_STRIP, 0, 4));
        geode->addDrawable(geom.get());
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

        if (tex)
        {
            ss->setTextureAttributeAndModes(
                0,
                tex,
                osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE
            );
            applyTextureShader(ss);
        }

        return geode;
    }
}

bool RenderCompositeSystem::initialize(osgViewer::Viewer& viewer,
                                       osg::Group* root,
                                       const Masks& masks)
{
    if (!root) return false;
    osg::Camera* master = viewer.getCamera();
    masterCamera_ = master;
    osg::GraphicsContext* gc = master ? master->getGraphicsContext() : nullptr;
    if (!gc)
    {
        std::cerr << "Master camera has NO GraphicsContext\n";
        return false;
    }

    const osg::GraphicsContext::Traits* traits = gc->getTraits();
    if (!traits)
    {
        std::cerr << "GraphicsContext has NO traits\n";
        return false;
    }

    masks_ = masks;
    width_ = traits->width;
    height_ = traits->height;
    master->setViewport(0, 0, width_, height_);

    texGlobeL_ = makeRTTTexture(width_, height_);
    texGlobeR_ = makeRTTTexture(width_, height_);
    texBathyL_ = makeRTTTexture(width_, height_);
    texBathyR_ = makeRTTTexture(width_, height_);

    rttGlobeCamL_ = makeRTTCamera(texGlobeL_.get(), width_, height_, 0);
    rttGlobeCamR_ = makeRTTCamera(texGlobeR_.get(), width_, height_, 1);
    rttBathyCamL_ = makeRTTCamera(texBathyL_.get(), width_, height_, 2);
    rttBathyCamR_ = makeRTTCamera(texBathyR_.get(), width_, height_, 3);

    rttGlobeCamL_->setPostDrawCallback(new MarkRenderedOnce(&texGlobeReady_));
    rttGlobeCamR_->setPostDrawCallback(new MarkRenderedOnce(&texGlobeReady_));
    rttBathyCamL_->setPostDrawCallback(new MarkRenderedOnce(&texBathyReady_));
    rttBathyCamR_->setPostDrawCallback(new MarkRenderedOnce(&texBathyReady_));

    rttGlobeCamL_->setNodeMask(masks.overlay);
    rttGlobeCamR_->setNodeMask(masks.overlay);
    rttGlobeCamL_->setCullMask(masks.globe);
    rttGlobeCamR_->setCullMask(masks.globe);

    rttBathyCamL_->setNodeMask(masks.overlay);
    rttBathyCamR_->setNodeMask(masks.overlay);
    rttBathyCamL_->setCullMask(masks.scene);
    rttBathyCamR_->setCullMask(masks.scene);
    rttBathyCamL_->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    rttBathyCamR_->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    hudLeft_ = makeFullscreenQuadCamera(GL_BACK_LEFT);
    hudLeft_->setNodeMask(masks.overlay);
    hudLeft_->setViewport(0, 0, width_, height_);
    hudRight_ = makeFullscreenQuadCamera(GL_BACK_RIGHT);
    hudRight_->setNodeMask(masks.overlay);
    hudRight_->setViewport(0, 0, width_, height_);

    globeQuadL_ = makeFullscreenTexturedQuadGeode(texGlobeL_.get(), osg::Vec4(0, 1, 0, 1), StereoEye::Left);
    globeQuadR_ = makeFullscreenTexturedQuadGeode(texGlobeR_.get(), osg::Vec4(0, 1, 0, 1), StereoEye::Right);
    bathyQuadL_ = makeFullscreenTexturedQuadGeode(texBathyL_.get(), osg::Vec4(1, 0, 0, 1), StereoEye::Left);
    bathyQuadR_ = makeFullscreenTexturedQuadGeode(texBathyR_.get(), osg::Vec4(1, 0, 0, 1), StereoEye::Right);

    setGlobeAlpha(1.0f);
    setBathyAlpha(0.0f);

    setGlobeQuadVisible(false);
    setBathyQuadVisible(false);
    hudLeft_->addChild(globeQuadL_.get());
    hudRight_->addChild(globeQuadR_.get());
    hudLeft_->addChild(bathyQuadL_.get());
    hudRight_->addChild(bathyQuadR_.get());

    zoomCtrl_ = new ZoomTransitionController(
        master,
        globeQuadL_.get(),
        globeQuadR_.get(),
        bathyQuadL_.get(),
        bathyQuadR_.get()
    );
    root->addChild(hudLeft_.get());
    root->addChild(hudRight_.get());
    root->addChild(rttGlobeCamL_.get());
    root->addChild(rttGlobeCamR_.get());
    root->addChild(rttBathyCamL_.get());
    root->addChild(rttBathyCamR_.get());

    std::cerr << "RTT setup: " << width_ << "x" << height_
              << " globeCamL=" << (rttGlobeCamL_.valid() ? "yes" : "no")
              << " globeCamR=" << (rttGlobeCamR_.valid() ? "yes" : "no")
              << " bathyCamL=" << (rttBathyCamL_.valid() ? "yes" : "no")
              << " bathyCamR=" << (rttBathyCamR_.valid() ? "yes" : "no") << "\n";
    return true;
}

void RenderCompositeSystem::attachScenes(osg::Node* globeScene, osg::Node* worldScene)
{
    if (scenesAttached_) return;

    osg::Camera* master = masterCamera_.get();
    std::cerr << "[RenderCompositeSystem] attachScenes master="
          << (master ? "yes" : "no") << "\n";

    if (rttGlobeCamL_.valid() && globeScene)
    {
        rttGlobeCamL_->addChild(globeScene);
        rttGlobeCamL_->addUpdateCallback(new MirrorMasterCameraMatrices(
            master,
            rttGlobeCamL_.get(),
            zoomCtrl_.get(),
            MirrorMasterCameraMatrices::FreezeMode::Globe,
            MirrorMasterCameraMatrices::Eye::Left
        ));
    }

    if (rttGlobeCamR_.valid() && globeScene)
    {
        rttGlobeCamR_->addChild(globeScene);
        rttGlobeCamR_->addUpdateCallback(new MirrorMasterCameraMatrices(
            master,
            rttGlobeCamR_.get(),
            zoomCtrl_.get(),
            MirrorMasterCameraMatrices::FreezeMode::Globe,
            MirrorMasterCameraMatrices::Eye::Right
        ));
    }

    if (rttBathyCamL_.valid() && worldScene)
    {
        rttBathyCamL_->addChild(worldScene);
        rttBathyCamL_->addUpdateCallback(new MirrorMasterCameraMatrices(
            master,
            rttBathyCamL_.get(),
            zoomCtrl_.get(),
            MirrorMasterCameraMatrices::FreezeMode::Bathy,
            MirrorMasterCameraMatrices::Eye::Left
        ));
    }

    if (rttBathyCamR_.valid() && worldScene)
    {
        rttBathyCamR_->addChild(worldScene);
        rttBathyCamR_->addUpdateCallback(new MirrorMasterCameraMatrices(
            master,
            rttBathyCamR_.get(),
            zoomCtrl_.get(),
            MirrorMasterCameraMatrices::FreezeMode::Bathy,
            MirrorMasterCameraMatrices::Eye::Right
        ));
    }

    scenesAttached_ = true;
}

void RenderCompositeSystem::setMasterCullGlobeOnly()
{
    if (!masterCamera_) return;
    masterCamera_->setCullMask(masks_.masterCullGlobe);
}

void RenderCompositeSystem::setMasterCullSceneAndOverlay()
{
    if (!masterCamera_) return;
    masterCamera_->setCullMask(masks_.masterCullScene);
}

void RenderCompositeSystem::setMasterBlackClear()
{
    if (!masterCamera_) return;
    masterCamera_->setClearColor(osg::Vec4(0.f, 0.f, 0.f, 1.f));
    masterCamera_->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void RenderCompositeSystem::hideCompositeQuads()
{
    setGlobeQuadVisible(false);
    setBathyQuadVisible(false);
}

void RenderCompositeSystem::showGlobeCompositeOnly()
{
    setGlobeQuadVisible(true);
    setBathyQuadVisible(false);
}

void RenderCompositeSystem::showBothCompositeQuads()
{
    setGlobeQuadVisible(true);
    setBathyQuadVisible(true);
}

void RenderCompositeSystem::showBathyCompositeOnly()
{
    setGlobeQuadVisible(false);
    setBathyQuadVisible(true);
}

void RenderCompositeSystem::setGlobeAlpha(float a)
{
    if (globeQuadL_)
    {
        osg::Uniform* u = globeQuadL_->getOrCreateStateSet()->getUniform("u_alpha");
        if (u) u->set(a);
    }

    if (globeQuadR_)
    {
        osg::Uniform* u = globeQuadR_->getOrCreateStateSet()->getUniform("u_alpha");
        if (u) u->set(a);
    }
}

void RenderCompositeSystem::setBathyAlpha(float a)
{
    if (bathyQuadL_)
    {
        osg::Uniform* u = bathyQuadL_->getOrCreateStateSet()->getUniform("u_alpha");
        if (u) u->set(a);
    }

    if (bathyQuadR_)
    {
        osg::Uniform* u = bathyQuadR_->getOrCreateStateSet()->getUniform("u_alpha");
        if (u) u->set(a);
    }
}

void RenderCompositeSystem::setGlobeQuadVisible(bool visible)
{
    if (globeQuadL_) globeQuadL_->setNodeMask(visible ? 0xffffffff : 0x0);
    if (globeQuadR_) globeQuadR_->setNodeMask(visible ? 0xffffffff : 0x0);
}

void RenderCompositeSystem::setBathyQuadVisible(bool visible)
{
    if (bathyQuadL_) bathyQuadL_->setNodeMask(visible ? 0xffffffff : 0x0);
    if (bathyQuadR_) bathyQuadR_->setNodeMask(visible ? 0xffffffff : 0x0);
}