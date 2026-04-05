#ifndef PLASTICPARTICLEVE_SPIN_H
#define PLASTICPARTICLEVE_SPIN_H

#pragma once

#include <osg/MatrixTransform>
#include <osg/NodeCallback>
#include <osg/ref_ptr>
#include <osg/observer_ptr>
#include <osg/Quat>

namespace pcve {

    class Spin {
    public:
        struct Params {
            double durationSec = 5.0;
            double finalYawDeg = -64.0;
            double finalPitchDeg = -100.0;

            double startYawOffsetDeg   = 180.0;
            double startPitchOffsetDeg = 55.0;
            double startRollOffsetDeg  = 30.0;

            // start smaller, end at normal size
            double startScale = 0.45;
            double finalScale = 1.0;
        };

        Spin() = default;
        void setParams(const Params& p);
        void installCallback(osg::MatrixTransform* target);
        void begin(double simTime);
        bool isFinished() const;

    private:
        class Callback : public osg::NodeCallback {
        public:
            explicit Callback(Spin* owner) : owner_(owner) {}
            void operator()(osg::Node* node, osg::NodeVisitor* nv) override;

        private:
            Spin* owner_ = nullptr;
        };

        osg::Quat makeFinalQuat() const;
        osg::Quat makeStartQuat() const;
        void applyRotation(double t);

        Params params_;

        osg::observer_ptr<osg::MatrixTransform> target_;
        osg::ref_ptr<Callback> cb_;
        osg::Quat startQuat_;
        osg::Quat finalQuat_;

        double startTime_ = 0.0;
        bool started_ = false;
        bool finished_ = false;
    };

} // namespace pcve

#endif // PLASTICPARTICLEVE_SPIN_H