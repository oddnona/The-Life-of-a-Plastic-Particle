#include "many.h"
#include "trajectories.h"

#include <osg/NodeVisitor>
#include <osg/FrameStamp>
#include <cmath>

namespace pcve {

void Many::installCallback(osg::Node* updateNode,
                           PhaseState* state,
                           Trajectories* trajectories)
{
    if (!updateNode || !state || !trajectories) return;
    cb_ = new Callback(this, state, trajectories);
    updateNode->addUpdateCallback(cb_.get());
}

    void Many::setOnYearStep(std::function<void(int yearIndex)> cb)
{
    onYearStep_ = std::move(cb);
}

void Many::Callback::operator()(osg::Node* node, osg::NodeVisitor* nv)
{
    // always traverse; decide whether to act
    if (!owner_ || !state_ || !traj_ || !traj_->valid()) {
        traverse(node, nv);
        return;
    }

    // wait until intro finishes
    if (!state_->introFinished) {
        traverse(node, nv);
        return;
    }

    const osg::FrameStamp* fs = nv ? nv->getFrameStamp() : nullptr;
    const double now = fs ? fs->getReferenceTime() : 0.0;

    if (!started_) {
        started_ = true;
        if (owner_->onYearStep_) {
            owner_->onYearStep_(0);
        }
        // start from nothing visible
        traj_->hideAll();

        // determine the longest trajectory (in emitted vertices)
        maxVerts_ = traj_->maxVertexCount();

        // if nothing exists, finish immediately
        if (maxVerts_ < 2) {
            if (owner_->onYearStep_) {
                owner_->onYearStep_(10);
            }

            if (owner_->params_.markManyFinishedWhenDone) {
                traj_->hideAll();
                state_->manyFinished = true;
            }
            finished_ = true;
            traverse(node, nv);
            return;
        }

        // start at the first vertex: no segment yet, but consistent with travel
        i_ = 0;
        frameCounter_ = 0;
        holding_ = false;
        holdStart_ = 0.0;

        traj_->setVisiblePrefix(1);
        traverse(node, nv);
        return;
    }

    if (finished_) {
        traverse(node, nv);
        return;
    }

    // leave the full trajectories visible and finish
    if (holding_) {
        if (owner_->onYearStep_) {
            owner_->onYearStep_(10);
        }

        finished_ = true;
        if (owner_->params_.markManyFinishedWhenDone) {
            state_->manyFinished = true;
        }
        traverse(node, nv);
        return;
    }

    // step every N frames, like travel
    if (frameCounter_ % (size_t)std::max(1, owner_->params_.stepEveryFrames) == 0)
    {
        const double denom = (maxVerts_ > 1) ? double(maxVerts_ - 1) : 1.0;
        const double progress = double(i_) / denom;

        size_t stepsThisTick = 1;

        // speed up after speedupAfterFraction, but return to normal speed in the last 10%
        if (progress >= owner_->params_.speedupAfterFraction && progress < 0.90) {
            stepsThisTick = (size_t)std::max(1, owner_->params_.speedupFactor);
        } else {
            stepsThisTick = 1;
        }

        int latestYearThisTick = -1;

        for (size_t s = 0; s < stepsThisTick; ++s)
        {
            if (i_ + 1 < maxVerts_) {
                ++i_;
                traj_->setVisiblePrefix(i_ + 1);
                const double yearProgress = (maxVerts_ > 1) ? (double(i_) / double(maxVerts_ - 1)) : 1.0;
                int fakeYear = (int)std::floor(yearProgress * 10.0);

                if (fakeYear < 0) fakeYear = 0;
                if (fakeYear > 10) fakeYear = 10;
                latestYearThisTick = fakeYear;
            }
            else {
                // reached full length: ensure fully visible, then start holding
                traj_->setVisiblePrefix(maxVerts_);

                if (owner_->onYearStep_) {
                    owner_->onYearStep_(10);
                }

                holding_ = true;
                holdStart_ = now;
                break;
            }
        }

        if (latestYearThisTick >= 0 && owner_->onYearStep_) {
            owner_->onYearStep_(latestYearThisTick);
        }
    }

    ++frameCounter_;
    traverse(node, nv);
}
} // namespace pcve