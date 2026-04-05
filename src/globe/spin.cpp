#include "spin.h"

#include <osg/NodeVisitor>
#include <osg/Matrix>
#include <osg/Quat>
#include <osg/Math>

#include <algorithm>

namespace pcve {

void Spin::setParams(const Params& p)
{
    params_ = p;
}

void Spin::installCallback(osg::MatrixTransform* target)
{
    target_ = target;
    if (!target_) return;
    cb_ = new Callback(this);
    target_->addUpdateCallback(cb_.get());
}

void Spin::begin(double simTime)
{
    startTime_ = simTime;
    started_ = true;
    finished_ = false;
    finalQuat_ = makeFinalQuat();
    startQuat_ = makeStartQuat();
    applyRotation(0.0);
}

bool Spin::isFinished() const
{
    return finished_;
}

osg::Quat Spin::makeFinalQuat() const
{
    const osg::Quat pitchQ(
        osg::DegreesToRadians(params_.finalPitchDeg),
        osg::Vec3(1.0f, 0.0f, 0.0f)
    );

    const osg::Quat yawQ(
        osg::DegreesToRadians(params_.finalYawDeg),
        osg::Vec3(0.0f, 0.0f, 1.0f)
    );

    return pitchQ * yawQ;
}

osg::Quat Spin::makeStartQuat() const
{
    const osg::Quat yawOffsetQ(
        osg::DegreesToRadians(params_.startYawOffsetDeg),
        osg::Vec3(0.0f, 0.0f, 1.0f)
    );

    const osg::Quat pitchOffsetQ(
        osg::DegreesToRadians(params_.startPitchOffsetDeg),
        osg::Vec3(1.0f, 0.0f, 0.0f)
    );

    const osg::Quat rollOffsetQ(
        osg::DegreesToRadians(params_.startRollOffsetDeg),
        osg::Vec3(0.0f, 1.0f, 0.0f)
    );

    const osg::Quat cinematicOffset = yawOffsetQ * pitchOffsetQ * rollOffsetQ;
    return cinematicOffset * finalQuat_;
}
    void Spin::applyRotation(double t)
{
    if (!target_) return;

    const double u = std::clamp(t, 0.0, 1.0);

    // smoothstep easing: slow start, faster middle, slow finish
    const double eased = u * u * (3.0 - 2.0 * u);

    // decay extra motion toward the final pose
    const double remain = 1.0 - eased;
    // strong yaw + noticeable pitch, both fading to zero
    const osg::Quat extraYaw(
        osg::DegreesToRadians(params_.startYawOffsetDeg * remain),
        osg::Vec3(0.0f, 0.0f, 1.0f)
    );

    const osg::Quat extraPitch(
        osg::DegreesToRadians(params_.startPitchOffsetDeg * remain),
        osg::Vec3(1.0f, 0.0f, 0.0f)
    );

    const osg::Quat extraRoll(
        osg::DegreesToRadians(params_.startRollOffsetDeg * remain),
        osg::Vec3(0.0f, 1.0f, 0.0f)
    );

    const osg::Quat q = extraYaw * extraPitch * extraRoll * finalQuat_;

    const double scale =
        params_.startScale + (params_.finalScale - params_.startScale) * eased;

    target_->setMatrix(
        osg::Matrix::scale(scale, scale, scale) *
        osg::Matrix::rotate(q)
    );
}

void Spin::Callback::operator()(osg::Node* node, osg::NodeVisitor* nv)
{
    if (!owner_ || !owner_->target_.valid() || !owner_->started_ || owner_->finished_)
    {
        traverse(node, nv);
        return;
    }

    double simTime = 0.0;
    if (nv && nv->getFrameStamp())
    {
        simTime = nv->getFrameStamp()->getSimulationTime();
    }

    const double duration = std::max(0.0001, owner_->params_.durationSec);
    const double t = (simTime - owner_->startTime_) / duration;

    owner_->applyRotation(t);

    if (t >= 1.0)
    {
        owner_->applyRotation(1.0);
        owner_->finished_ = true;

        if (owner_->target_.valid())
            owner_->target_->removeUpdateCallback(this);
    }

    traverse(node, nv);
}

} // namespace pcve