#include "travel.h"

#include "utils_netcdf.h"
#include "utils_geo_color.h"

#include <osg/LineWidth>
#include <osg/Array>
#include <osg/ShapeDrawable>
#include <osg/StateSet>
#include <osg/PrimitiveSet>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <iostream>

namespace pcve {

bool Travel::build(netCDF::NcFile& file, const Params& p)
{
    params_ = p;

    netCDF::NcVar lonVar = pcve::nc::getVarAny(file, {"lon", "longitude"});
    netCDF::NcVar latVar = pcve::nc::getVarAny(file, {"lat", "latitude"});

    if (lonVar.isNull() || latVar.isNull() || lonVar.getDimCount() != 2 || latVar.getDimCount() != 2) {
        std::cerr << "[warn] Travel: lon/lat not found or wrong dims\n";
        hasIntro_ = false;
        return false;
    }

    std::vector<float> introLon, introLat;
    auto fillLon = pcve::nc::getFillValue(lonVar);
    auto fillLat = pcve::nc::getFillValue(latVar);

    if (!pcve::nc::readTrajSeries2D(lonVar, p.particleIndex, introLon) ||
        !pcve::nc::readTrajSeries2D(latVar, p.particleIndex, introLat))
    {
        std::cerr << "[warn] Travel: failed reading intro traj " << p.particleIndex << "\n";
        hasIntro_ = false;
        return false;
    }

    introSamples_ = pcve::util::buildSphereSamplesIntro(introLon, introLat, fillLon, fillLat, p.radiusSlightlyAbove);
    if (introSamples_.size() < 2) {
        std::cerr << "[warn] Travel: intro trajectory too short after filtering\n";
        hasIntro_ = false;
        return false;
    }

    introTrailVerts_ = new osg::Vec3Array();
    introTrailVerts_->reserve(introSamples_.size());
    introTrailVerts_->push_back(introSamples_.front());

    introTrailGeom_ = new osg::Geometry();
    introTrailGeom_->setUseVertexBufferObjects(true);
    introTrailGeom_->setUseDisplayList(false);
    introTrailGeom_->setDataVariance(osg::Object::DYNAMIC);
    introTrailGeom_->setVertexArray(introTrailVerts_.get());
    introTrailGeom_->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP, 0, 1));

    // color array for time gradient
    osg::ref_ptr<osg::Vec4Array> introTrailColors = new osg::Vec4Array();
    introTrailColors->reserve(introSamples_.size());
    introTrailColors->push_back(osg::Vec4(0.25f, 0.75f, 0.85f, 0.75f));
    introTrailGeom_->setColorArray(introTrailColors.get(), osg::Array::BIND_PER_VERTEX);
    introTrailGeode_ = new osg::Geode();
    introTrailGeode_->addDrawable(introTrailGeom_.get());

    {
        osg::StateSet* ss = introTrailGeode_->getOrCreateStateSet();
        ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        ss->setMode(GL_LINE_SMOOTH, osg::StateAttribute::OFF);
        ss->setAttributeAndModes(new osg::LineWidth(p.trailWidth), osg::StateAttribute::ON);
        osg::ref_ptr<osg::Depth> depth = new osg::Depth;
        depth->setFunction(osg::Depth::LEQUAL);
        depth->setWriteMask(true);
        ss->setAttributeAndModes(depth.get(), osg::StateAttribute::ON);
        ss->setMode(GL_BLEND, osg::StateAttribute::ON);
        ss->setAttributeAndModes(
            new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA),
            osg::StateAttribute::ON
        );
        ss->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    }

    // Marker
    introMarkerXform_ = new osg::MatrixTransform();
    {
        osg::ref_ptr<osg::Sphere> s = new osg::Sphere(osg::Vec3(0,0,0), p.markerRadius);
        osg::ref_ptr<osg::ShapeDrawable> sd = new osg::ShapeDrawable(s.get());
        sd->setColor(osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f)); //particle color
        osg::ref_ptr<osg::Geode> g = new osg::Geode();
        g->addDrawable(sd.get());
        osg::StateSet* ss = g->getOrCreateStateSet();
        ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
        introMarkerXform_->addChild(g.get());
        introMarkerXform_->setMatrix(osg::Matrix::translate(introSamples_.front()));
    }

    hasIntro_ = true;
    return true;
}

void Travel::addTo(osg::Group* root)
{
    if (!hasIntro_) return;
    root->addChild(introTrailGeode_.get());
    root->addChild(introMarkerXform_.get());
}

void Travel::installCallback(osg::Node* updateNode, PhaseState* state)
{
    if (!hasIntro_ || !updateNode) return;
    cb_ = new Callback(this, state);
    updateNode->addUpdateCallback(cb_.get());
}

void Travel::Callback::operator()(osg::Node* node, osg::NodeVisitor* nv)
{
    // if already finished, just traverse
    if (!owner_ || !owner_->hasIntro_) {
        traverse(node, nv);
        return;
    }
    if ((state_ && state_->introFinished) || phase_ == Phase::Done) {
        traverse(node, nv);
        return;
    }

    double simTime = 0.0;
    if (nv && nv->getFrameStamp()) {
        simTime = nv->getFrameStamp()->getSimulationTime();
    }
        if (frameCounter_ % (size_t)std::max(1, owner_->params_.stepEveryFrames) == 0)
    {
        const size_t n = owner_->introSamples_.size();
        bool advanced = false;
            int latestDayThisTick = -1;

        // PHASE: FORWARD
        if (phase_ == Phase::Forward)
        {
            const double denom = (n > 1) ? double(n - 1) : 1.0;
            const double progress = double(i_) / denom;

            size_t stepsThisTick = 1;
            // speed up after fraction, but return to normal speed in the last 10%
            if (progress >= owner_->params_.speedupAfterFraction && progress < 0.94)
            {
                stepsThisTick = (size_t)std::max(1, owner_->params_.speedupFactor);
            }
            else
            {
                stepsThisTick = 1;
            }

            for (size_t s = 0; s < stepsThisTick; ++s)
            {
                if (i_ + 1 < n)
                {
                    ++i_;
                    latestDayThisTick = (int)i_;
                    owner_->introTrailVerts_->push_back(owner_->introSamples_[i_]);
                    // append matching per-vertex color based on time index
                    auto* colors = static_cast<osg::Vec4Array*>(owner_->introTrailGeom_->getColorArray());
                    if (colors)
                    {
                        const double t = (n > 1) ? (double(i_) / double(n - 1)) : 1.0;
                        const osg::Vec4 cStart(0.25f, 0.75f, 0.85f, 0.80f);  // cyan
                        const osg::Vec4 cMid (0.92f, 0.92f, 0.92f, 0.85f);  // light neutral
                        const osg::Vec4 cEnd (0.93f, 0.66f, 0.60f, 0.75f);  // salmon
                        osg::Vec4 c;

                        if (t < 0.5)
                        {
                            const float tt = float(t / 0.5);
                            c = cStart * (1.0f - tt) + cMid * tt;
                        }
                        else
                        {
                            const float tt = float((t - 0.5) / 0.5);
                            c = cMid * (1.0f - tt) + cEnd * tt;
                        }

                        colors->push_back(c);
                        colors->dirty();
                    }

                    advanced = true;
                }
                else
                {
                    phase_ = Phase::Hold;
                    holdStartSet_ = false;
                    break;
                }
            }

            if (advanced)
            {
                auto* da = static_cast<osg::DrawArrays*>(owner_->introTrailGeom_->getPrimitiveSet(0));
                da->setCount((GLint)owner_->introTrailVerts_->size());
                owner_->introMarkerXform_->setNodeMask(0xFFFFFFFFu);
                owner_->introMarkerXform_->setMatrix(osg::Matrix::translate(owner_->introSamples_[i_]));
                if (latestDayThisTick >= 0 && owner_->onDayStep_)
                    owner_->onDayStep_(latestDayThisTick);
                owner_->introTrailVerts_->dirty();
                owner_->introTrailGeom_->dirtyBound();
            }
        }

        // PHASE: HOLD
        else if (phase_ == Phase::Hold)
        {
            if (!holdStartSet_)
            {
                holdStartTime_ = simTime;
                holdStartSet_ = true;
            }

            const double elapsed = simTime - holdStartTime_;
            if (elapsed >= (double)owner_->params_.holdSeconds)
            {
                phase_ = Phase::Reverse;
            }
        }

        // PHASE: REVERSE
                else if (phase_ == Phase::Reverse)
        {
            const size_t reverseSteps = (size_t)std::max(1, owner_->params_.reverseSpeedFactor);
            auto* da = static_cast<osg::DrawArrays*>(owner_->introTrailGeom_->getPrimitiveSet(0));
            GLint count = da ? da->getCount() : 0;
            bool reversedThisTick = false;
            int latestDayThisTick = -1;
            for (size_t s = 0; s < reverseSteps; ++s)
            {
                if (count > 0)
                {
                    // decrease visible vertex count
                    --count;
                    if (i_ > 0) --i_;
                    reversedThisTick = true;
                    latestDayThisTick = (int)i_;
                }
                else
                {
                    break;
                }
            }

            if (da)
            {
                da->setCount(count);
            }

            if (count <= 1)
            {
                // hide marker and finish
                owner_->introMarkerXform_->setNodeMask(0x0u);
                phase_ = Phase::Done;
                // force final HUD state to day 0 when reverse finished
                if (owner_->onDayStep_)
                    owner_->onDayStep_(0);
                if (state_) state_->introFinished = true;
            }
            else
            {
                // keep marker visible and move it to current index
                owner_->introMarkerXform_->setNodeMask(0xFFFFFFFFu);
                owner_->introMarkerXform_->setMatrix(osg::Matrix::translate(owner_->introSamples_[i_]));

                // keep HUD in sync while reversing
                if (reversedThisTick && latestDayThisTick >= 0 && owner_->onDayStep_)
                    owner_->onDayStep_(latestDayThisTick);
            }
            owner_->introTrailVerts_->dirty();
            owner_->introTrailGeom_->dirtyBound();
        }
    }

    ++frameCounter_;
    traverse(node, nv);
}
    osg::Node* Travel::trackNode() const
    {
        return introMarkerXform_.get();
    }
    void Travel::setOnDayStep(std::function<void(int dayIndex)> cb)
    {
        onDayStep_ = std::move(cb);
    }

} // namespace pcve
