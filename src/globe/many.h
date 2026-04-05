#ifndef PLASTICPARTICLEVE_MANY_H
#define PLASTICPARTICLEVE_MANY_H

#pragma once
#include <osg/NodeCallback>
#include <osg/ref_ptr>
#include <cstddef>
#include <algorithm>
#include <functional>
#include "trajectories.h"
#include "travel.h" // PhaseState

namespace pcve {

    struct PhaseState; // from travel.h
    class Trajectories;

    class Many {
    public:
        struct Params {
            int stepEveryFrames = 2; //advance every N frames
            float speedupAfterFraction = 0.30f; // after 30% progress, speed up (until last 10%)
            int speedupFactor = 4; // how many steps per tick when sped up
            double holdFullSec = 3.0; // freeze at full length for this many seconds
            bool markManyFinishedWhenDone = true; // sets manyFinished after reset
        };

        Many() = default;
        void setParams(const Params& p) { params_ = p; }
        void installCallback(osg::Node* updateNode,
                             PhaseState* state,
                             Trajectories* trajectories);
        void setOnYearStep(std::function<void(int yearIndex)> cb);
    private:
        Params params_;

        class Callback : public osg::NodeCallback {
        public:
            Callback(Many* owner, PhaseState* state, Trajectories* traj)
                : owner_(owner), state_(state), traj_(traj) {}

            void operator()(osg::Node* node, osg::NodeVisitor* nv) override;

        private:
            Many* owner_ = nullptr;
            PhaseState* state_ = nullptr;
            Trajectories* traj_ = nullptr;

            bool started_ = false;
            bool finished_ = false;

            size_t i_ = 0;
            size_t frameCounter_ = 0;
            size_t maxVerts_ = 0; // maximum full length among all trajectories
            bool holding_ = false;
            double holdStart_ = 0.0;
        };
        std::function<void(int)> onYearStep_;
        osg::ref_ptr<Callback> cb_;
    };

} // namespace pcve

#endif // PLASTICPARTICLEVE_MANY_H