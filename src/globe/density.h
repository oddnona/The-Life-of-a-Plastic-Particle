#ifndef PLASTICPARTICLEVE_DENSITY_H
#define PLASTICPARTICLEVE_DENSITY_H

#pragma once
#include <osg/NodeCallback>
#include <osg/ref_ptr>
#include "travel.h"
#include <cstddef>

namespace pcve {

    struct PhaseState; // from travel.h
    class Trajectories;

    class Density {
    public:
        struct Params {
            // comet segment length in vertices (fixed)
            size_t segmentLenVerts = 2;

            // like many timing controls
            int    stepEveryFrames = 2;
            float  speedupAfterFraction = 0.30f;
            int    speedupFactor = 4;
            double holdFullSec = 3.0;
            bool   markDensityFinishedWhenDone = true;
        };

        Density() = default;
        void setParams(const Params& p) { params_ = p; }

        // update callback begins revealing after introFinished=true
        void installCallback(osg::Node* updateNode,
                             PhaseState* state,
                             Trajectories* trajectories);

    private:
        Params params_;

        class Callback : public osg::NodeCallback {
        public:
            Callback(Density* owner, PhaseState* state, Trajectories* traj)
                : owner_(owner), state_(state), traj_(traj) {}

            void operator()(osg::Node* node, osg::NodeVisitor* nv) override;

        private:
            Density* owner_;
            PhaseState* state_;
            Trajectories* traj_;

            bool started_ = false;
            double startTime_ = 0.0;
            bool finished_ = false;

            double headPos_ = 0.0;
            size_t frameCounter_ = 0;
            size_t maxVerts_ = 0;

            bool holding_ = false;
            double holdStart_ = 0.0;
        };

        osg::ref_ptr<Callback> cb_;
    };

} // namespace pcve


#endif //PLASTICPARTICLEVE_DENSITY_H