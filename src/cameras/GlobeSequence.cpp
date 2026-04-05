#include "GlobeSequence.h"

#include <osg/BlendFunc>
#include <osg/Geode>
#include <osg/Depth>
#include <osg/Geometry>
#include <osg/Point>
#include <osg/StateSet>
#include <osg/Uniform>
#include <osgDB/ReadFile>
#include <netcdf>
#include <random>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <filesystem>

#include "../globe/globe.h"

namespace
{
    static osg::ref_ptr<osg::Node> makeStarfield(unsigned int count, float radius)
    {
        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;

        osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;

        verts->reserve(count);
        colors->reserve(count);

        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> u01(0.0f, 1.0f);
        std::uniform_real_distribution<float> brightness(0.65f, 1.0f);

        for (unsigned int i = 0; i < count; ++i)
        {
            const float z = 2.0f * u01(rng) - 1.0f;
            const float a = 2.0f * osg::PI * u01(rng);
            const float rxy = std::sqrt(std::max(0.0f, 1.0f - z * z));

            const float x = rxy * std::cos(a);
            const float y = rxy * std::sin(a);

            verts->push_back(osg::Vec3(x * radius, y * radius, z * radius));

            const float b = brightness(rng);
            colors->push_back(osg::Vec4(b, b, b, 1.0f));
        }

        geom->setVertexArray(verts.get());
        geom->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
        geom->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, static_cast<GLsizei>(verts->size())));
        geom->setUseVertexBufferObjects(true);
        geom->setUseDisplayList(false);
        geode->addDrawable(geom.get());

        osg::StateSet* ss = geode->getOrCreateStateSet();
        ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        ss->setMode(GL_BLEND, osg::StateAttribute::ON);
        ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
        ss->setAttributeAndModes(new osg::Point(2.0f), osg::StateAttribute::ON);

        osg::ref_ptr<osg::Depth> depth = new osg::Depth;
        depth->setWriteMask(false);
        ss->setAttributeAndModes(depth.get(), osg::StateAttribute::ON);

        return geode;
    }
}

void GlobeSequence::initialize(unsigned int globeMask)
{
    globeMask_ = globeMask;
    globeScene_ = new osg::Group();
    globeScene_->setNodeMask(0x0);
    starfieldNode_ = makeStarfield(2000, 80.0f);
    if (starfieldNode_.valid())
    {
        starfieldNode_->setNodeMask(globeMask_);
        globeScene_->addChild(starfieldNode_.get());
    }
}

void GlobeSequence::setDependencies(ZoomTransitionController* zoomCtrl, TimeCounter* timeCounter)
{
    zoomCtrl_ = zoomCtrl;
    timeCounter_ = timeCounter;
}

void GlobeSequence::setSceneVisible(bool visible)
{
    if (!globeScene_) return;
    globeScene_->setNodeMask(visible ? globeMask_ : 0x0);
}

void GlobeSequence::setStarfieldVisible(bool visible)
{
    if (!starfieldNode_) return;
    starfieldNode_->setNodeMask(visible ? globeMask_ : 0x0);
}

void GlobeSequence::setGlobeMix(float mix)
{
    if (!globeMixUniform_) return;
    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;
    globeMixUniform_->set(mix);
}

void GlobeSequence::updateTextureFades(double nowT)
{
    if (!globeMixUniform_ || spinStartT_ < 0.0)
        return;

    // first fade: glob2 -> glob1, starting 3.5s into spin, lasting 1.5s
    if (!firstFadeDone_)
    {
        double fadeStart = spinStartT_ + firstFadeDelaySec_;
        double fadeEnd = fadeStart + fadeDurationSec_;

        if (nowT >= fadeStart)
        {
            firstFadeStarted_ = true;
            float mix = static_cast<float>((nowT - fadeStart) / fadeDurationSec_);
            if (mix >= 1.0f)
            {
                mix = 1.0f;
                firstFadeDone_ = true;
            }

            setGlobeMix(mix);
        }
    }

    // second fade: glob1 -> glob2, starting when density finishes, lasting 1.5s
    if (globePhase_.densityFinished && !secondFadeStarted_)
    {
        secondFadeStarted_ = true;
        secondFadeStartT_ = nowT;
    }

    if (secondFadeStarted_ && !secondFadeDone_)
    {
        float t = static_cast<float>((nowT - secondFadeStartT_) / fadeDurationSec_);
        if (t >= 1.0f)
        {
            t = 1.0f;
            secondFadeDone_ = true;
        }

        float mix = 1.0f - t;
        setGlobeMix(mix);
    }
}

void GlobeSequence::beginAfterIntro(osgViewer::Viewer& viewer, double nowT)
{
    if (!globeScene_) return;
    if (globeXform_.valid()) return;
    globeNode_ = pcve::Globe::createBlendedSphere("../../OceanGrid/src/coordinates/glob2.png", "../../OceanGrid/src/coordinates/glob1.png");
    if (!globeNode_) {
        std::cerr << "[GlobeSequence] globeNode_ is null\n";
    }
    if (globeNode_)
        globeNode_->setNodeMask(globeMask_);

    if (globeNode_)
    {
        globeXform_ = new osg::MatrixTransform;
        globeXform_->setNodeMask(globeMask_);

        double yawDeg = -64.0;
        double pitchDeg = -100.0;

        osg::Matrix R =
            osg::Matrix::rotate(osg::DegreesToRadians(pitchDeg), osg::Vec3(1,0,0)) *
            osg::Matrix::rotate(osg::DegreesToRadians(yawDeg), osg::Vec3(0,0,1));

        globeXform_->setMatrix(R);
        globeXform_->addChild(globeNode_.get());
        globeScene_->addChild(globeXform_.get());
        osg::Geode* geode = dynamic_cast<osg::Geode*>(globeNode_.get());
        if (geode)
        {
            osg::StateSet* ss = geode->getOrCreateStateSet();
            globeMixUniform_ = ss ? ss->getUniform("u_mix") : nullptr;
        }

        setGlobeMix(0.0f);

        firstFadeStarted_ = false;
        firstFadeDone_ = false;
        secondFadeStarted_ = false;
        secondFadeDone_ = false;
        secondFadeStartT_ = -1.0;
        osg::Camera* c = viewer.getCamera();
        osg::GraphicsContext* gc = c ? c->getGraphicsContext() : nullptr;
        if (!c || !gc || !gc->getTraits())
            return;

        osg::Vec3 eye(0.0, -2.4, 0.0);
        osg::Vec3 center(0.0, 0.0, 0.0);
        double rollDeg = 80.0;
        osg::Vec3 forward = center - eye;
        forward.normalize();
        osg::Quat rollQ(osg::DegreesToRadians(rollDeg), forward);
        osg::Vec3 up = rollQ * osg::Vec3(0.0, 0.0, 1.0);

        c->setViewMatrixAsLookAt(eye, center, up);

        c->setProjectionMatrixAsPerspective(
            45.0,
            static_cast<double>(gc->getTraits()->width) / static_cast<double>(gc->getTraits()->height),
            0.01,
            1000.0
        );

        pcve::Spin::Params sp;
        sp.durationSec = 5.0;
        sp.finalYawDeg = -64.0;
        sp.finalPitchDeg = -100.0;
        sp.startYawOffsetDeg = 180.0;
        sp.startPitchOffsetDeg = 55.0;
        sp.startRollOffsetDeg = 65.0;
        sp.startScale = 0.45;
        sp.finalScale = 1.0;

        spin_.setParams(sp);
        spin_.installCallback(globeXform_.get());
        spin_.begin(nowT);
        spinStartT_ = nowT;
        spinStarted_ = true;
        travelStartT_ = -1.0;

        c->setClearColor(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
        c->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}

void GlobeSequence::update(double nowT)
{
    if (!spinStarted_)
        return;
    updateTextureFades(nowT);
    if (!travelStarted_ && spin_.isFinished())
    {
        travelStarted_ = true;
        travelStartT_ = -1.0;
        globePhase_ = pcve::PhaseState{};

        try
        {
            globeNcFile_ = std::make_unique<netCDF::NcFile>(
                "../../OceanGrid/src/trajectories/sa_100fragscale.nc",
                netCDF::NcFile::read
            ); //PERSONAL PATH

            pcve::Travel::Params tp;
            bool ok = travel_.build(*globeNcFile_, tp);
            if (!ok)
            {
                std::cerr << "[Travel] build() failed\n";
                globePhase_.introFinished = true;
            }
            else
            {
                travel_.addTo(globeXform_.get());
                travel_.installCallback(globeXform_.get(), &globePhase_);
                travel_.setOnDayStep([this](int dayIndex) {
                    if (timeCounter_.valid()) {
                        timeCounter_->setUnit(TimeCounter::Unit::Days);
                        timeCounter_->setDay(dayIndex);
                    }
                });

                if (zoomCtrl_.valid())
                    zoomCtrl_->setPhaseATrackNode(travel_.trackNode());

                pcve::Trajectories::Params tparamsMany;
                bool tokMany = trajMany_.build(*globeNcFile_, tparamsMany);

                if (tokMany && trajMany_.node())
                {
                    globeXform_->addChild(trajMany_.node());

                    osg::StateSet* ssMany = trajMany_.node()->getOrCreateStateSet();
                    ssMany->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
                    ssMany->setRenderBinDetails(10, "RenderBin");

                    trajMany_.revealUpTo(0);
                }
                else
                {
                    std::cerr << "[GlobeSequence] trajMany NOT valid\n";
                }

                pcve::Trajectories::Params tparamsDensity;
                bool tokDensity = trajDensity_.build(*globeNcFile_, tparamsDensity);

                if (tokDensity && trajDensity_.node())
                {
                    globeXform_->addChild(trajDensity_.node());

                    osg::StateSet* ssDensity = trajDensity_.node()->getOrCreateStateSet();
                    ssDensity->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
                    ssDensity->setRenderBinDetails(20, "RenderBin");

                    ssDensity->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);

                    osg::ref_ptr<osg::Depth> depthDensity = new osg::Depth;
                    depthDensity->setFunction(osg::Depth::ALWAYS);
                    depthDensity->setWriteMask(false);
                    ssDensity->setAttributeAndModes(depthDensity.get(), osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
                    trajDensity_.revealUpTo(0);
                }
                else
                {
                    std::cerr << "[GlobeSequence] trajDensity NOT valid\n";
                }

                if (zoomCtrl_.valid())
                    zoomCtrl_->begin(nowT);

                travelStartT_ = nowT;

                if (timeCounter_.valid())
                {
                    timeCounter_->setUnit(TimeCounter::Unit::Days);
                    timeCounter_->setDay(0);
                    timeCounter_->setVisible(true);
                }
            }
        }
        catch (const netCDF::exceptions::NcException& e)
        {
            std::cerr << "[NetCDF] NcException: " << e.what() << "\n";
            globePhase_.introFinished = true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Exception] " << e.what() << "\n";
            globePhase_.introFinished = true;
        }
        catch (...)
        {
            std::cerr << "[Exception] unknown\n";
            globePhase_.introFinished = true;
        }
    }

    if (globePhase_.introFinished && !manyStarted_)
    {
        manyStarted_ = true;
        std::cerr << "[SEQ] Many started at t=" << nowT << "\n";

        pcve::Many::Params mp;
        mp.stepEveryFrames = 3;
        mp.speedupFactor = 1;
        many_.setParams(mp);

        many_.setOnYearStep([this](int yearIndex) {
            if (timeCounter_.valid()) {
                timeCounter_->setUnit(TimeCounter::Unit::Years);
                timeCounter_->setDay(yearIndex);
            }
        });

        many_.installCallback(globeXform_.get(), &globePhase_, &trajMany_);
    }

    if (globePhase_.introFinished &&
        globePhase_.manyFinished &&
        !densityStarted_)
    {
        densityStarted_ = true;
        std::cerr << "[SEQ] Density started at t=" << nowT << "\n";
        pcve::Density::Params dp;
        density_.setParams(dp);
        density_.installCallback(globeXform_.get(), &globePhase_, &trajDensity_);
    }
}