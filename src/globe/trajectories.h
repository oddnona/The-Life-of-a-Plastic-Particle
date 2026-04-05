//
// Created by Emil on 2/10/2026.
//

#ifndef PLASTICPARTICLEVE_TRAJECTORIES_H
#define PLASTICPARTICLEVE_TRAJECTORIES_H
#pragma once
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/PrimitiveSet>
#include <osg/ref_ptr>
#include <netcdf>
#include <vector>
#include <cstddef>

namespace pcve {

    class Trajectories {
    public:
        struct Params {
            int stride = 1;
            int maxParticles = 10000;
            int obsStride = 10;
            float lineWidth = 2.0f;
            float radiusSlightlyAbove = 1.002f;
            int fadeSteps = 20;
        };

        bool build(netCDF::NcFile& file, const Params& p);
        osg::Node* node() const { return geode_.get(); }
        // number of selected trajectories (reveal units)
        size_t selectedTrajectoryCount() const { return prefixEndPrimCount_.size(); }
        void revealUpTo(size_t k);

        // show the first N vertices of every trajectory
        void setVisiblePrefix(size_t nVerts);
        void setVisibleWindow(size_t headVert, size_t segLenVerts);
        void setVisiblePoint(double headPos);
        // returns the maximum vertex count among all trajectories (primitive sets)
        size_t maxVertexCount() const;
        void hideAll();
        void setGlobalAlpha(float a);
        void disableDeathFade();
        void restoreDeathFade();
        void setColorOverride(const osg::Vec4& color);
        void clearColorOverride();
        bool valid() const;

    private:
        osg::ref_ptr<osg::Geode> geode_;
        osg::ref_ptr<osg::Geometry> geom_;
        osg::ref_ptr<osg::Uniform> globalAlphaU_;
        float globalAlpha_ = 1.0f;
        osg::ref_ptr<osg::Vec3Array> verts_;
        osg::ref_ptr<osg::Vec4Array> colors_;
        std::vector<float> baseAlpha_;
        std::vector< osg::ref_ptr<osg::DrawArrays> > drawArrays_;
        std::vector<int> primOriginalCounts_;
        std::vector<int> primBaseFirst_;

        osg::ref_ptr<osg::Geometry> densityGeom_;
        osg::ref_ptr<osg::Vec3Array> densityVerts_;
        osg::ref_ptr<osg::Vec4Array> densityColors_;
        std::vector< osg::ref_ptr<osg::DrawArrays> > densityPointDrawArrays_;
        std::vector<int> densityTrajBaseFirst_;
        std::vector<int> densityTrajCounts_;

        std::vector<unsigned int> prefixEndPrimCount_; // after traj k, how many line primsets exist
        bool colorOverrideActive_ = false;
        osg::Vec4 overrideColor_;
        // how many primitive sets are currently enabled
        unsigned int enabledPrimSets_ = 0;
    };

} // namespace pcve
#endif //PLASTICPARTICLEVE_TRAJECTORIES_H