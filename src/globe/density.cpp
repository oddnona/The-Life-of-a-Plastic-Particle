#include "density.h"
#include "travel.h"        // PhaseState
#include "trajectories.h"

#include <osg/NodeVisitor>
#include <osg/FrameStamp>
#include <algorithm>
#include <cmath>

namespace pcve {

void Density::installCallback(osg::Node* updateNode,
                              PhaseState* state,
                              Trajectories* trajectories)
{
    if (!updateNode || !state || !trajectories) return;
    cb_ = new Callback(this, state, trajectories);
    updateNode->addUpdateCallback(cb_.get());
}

void Density::Callback::operator()(osg::Node* node, osg::NodeVisitor* nv)
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
        startTime_ = now;
        // start from nothing visible
        traj_->hideAll();
        // determine the longest trajectory (in emitted vertices)
        maxVerts_ = traj_->maxVertexCount();

        // if nothing exists, finish immediately
        if (maxVerts_ < 2) {
            if (owner_->params_.markDensityFinishedWhenDone) {
                traj_->hideAll();
                traj_->disableDeathFade();
                state_->densityFinished = true;
            }
            finished_ = true;
            traverse(node, nv);
            return;
        }

        frameCounter_ = 0;
        holding_ = false;
        holdStart_ = 0.0;

        // show initial comet at head=0
        traj_->setVisiblePoint(0.0);

        traverse(node, nv);
        return;
    }

    if (finished_) {
        traverse(node, nv);
        return;
    }

    // hold at full progress, then reset and finish
    if (holding_) {
        const double holdSec = owner_->params_.holdFullSec;
        if (holdSec <= 0.0 || (now - holdStart_) >= holdSec) {
            traj_->hideAll();
            finished_ = true;
            if (owner_->params_.markDensityFinishedWhenDone) {
                state_->densityFinished = true;
            }
        }

        traverse(node, nv);
        return;
    }

    // total animation duration (seconds)
    const double duration = 3.0;

    // elapsed time
    double elapsed = now - startTime_;

    // progress in [0..1]
    double progress = elapsed / duration;
    if (progress > 1.0) progress = 1.0;

    // linear motion
    double headPos = progress * (double)(maxVerts_ - 1);
    traj_->setVisiblePoint(headPos);

    if (progress >= 1.0)
    {
        traj_->hideAll();
        finished_ = true;

        if (owner_->params_.markDensityFinishedWhenDone) {
            state_->densityFinished = true;
        }

        traverse(node, nv);
        return;
    }

    ++frameCounter_;
    traverse(node, nv);
}

} // namespace pcve