#ifndef PLASTICPARTICLEVE_TRAVEL_H
#define PLASTICPARTICLEVE_TRAVEL_H

#pragma once
#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/ref_ptr>
#include <osg/NodeCallback>
#include <netcdf>
#include <vector>
#include <functional>
#include <cstddef>
#include <osg/Node>

namespace pcve {

    struct PhaseState {
        bool introFinished   = false;  // Travel done
        bool densityFinished = false;  // Density done
        bool manyFinished    = false;  // Many done
    };

    class Travel {
    public:
        struct Params {
            int particleIndex = 532; //longest trajectory
            //int particleIndex = 300; //the bathy trajectory
            int stepEveryFrames = 5;

            // Speed-up behavior:
            // After this fraction of the trajectory is completed, advance speedupFactor samples per tick.
            float speedupAfterFraction = 0.27f; // 25%
            int   speedupFactor        = 6;     // 3x faster
            float holdSeconds          = 1.0f;  // hold at end before reversing
            int   reverseSpeedFactor   = 17;     // reverse is 3x faster than forward
            float trailWidth = 3.0f;
            float markerRadius = 0.001f;
            float radiusSlightlyAbove = 1.002f;
        };

        bool build(netCDF::NcFile& file, const Params& p);
        void addTo(osg::Group* root);
        // we attach the update callback to any node that gets updated each frame (root is fine)
        void installCallback(osg::Node* updateNode, PhaseState* state);
        osg::Node* trackNode() const;
        // Called when the travel advances to the next observation (1 per day in your dataset).
        void setOnDayStep(std::function<void(int dayIndex)> cb);

    private:
        osg::ref_ptr<osg::Vec3Array> introTrailVerts_;
        osg::ref_ptr<osg::Geometry> introTrailGeom_;
        osg::ref_ptr<osg::Geode> introTrailGeode_;
        osg::ref_ptr<osg::MatrixTransform> introMarkerXform_;
        std::vector<osg::Vec3> introSamples_;
        bool hasIntro_ = false;
        Params params_;
        std::function<void(int)> onDayStep_;
        class Callback : public osg::NodeCallback {
        public:
            Callback(Travel* owner, PhaseState* state) : owner_(owner), state_(state) {}
            void operator()(osg::Node* node, osg::NodeVisitor* nv) override;

        private:
            enum class Phase { Forward, Hold, Reverse, Done };

            Travel* owner_;
            PhaseState* state_;
            Phase phase_ = Phase::Forward;

            size_t i_ = 0;
            size_t frameCounter_ = 0;
            double holdStartTime_ = 0.0;
            bool   holdStartSet_ = false;
        };

        osg::ref_ptr<Callback> cb_;
    };

} // namespace pcve


#endif //PLASTICPARTICLEVE_TRAVEL_H