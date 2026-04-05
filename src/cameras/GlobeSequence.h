#ifndef PLASTICPARTICLEVE_GLOBESEQUENCE_H
#define PLASTICPARTICLEVE_GLOBESEQUENCE_H

#include <osg/ref_ptr>
#include <osg/observer_ptr>
#include <memory>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/Uniform>
#include <osgViewer/Viewer>

#include "../globe/timeCounter.h"
#include "../globe/spin.h"
#include "../globe/travel.h"
#include "../globe/density.h"
#include "../globe/many.h"
#include "../globe/trajectories.h"
#include "../globe/ZoomTransitionController.h"

namespace netCDF
{
    class NcFile;
}

class GlobeSequence
{
public:
    GlobeSequence() = default;
    void initialize(unsigned int globeMask);
    void setDependencies(ZoomTransitionController* zoomCtrl, TimeCounter* timeCounter);

    osg::Group* scene() const { return globeScene_.get(); }

    void setSceneVisible(bool visible);
    void setStarfieldVisible(bool visible);
    void beginAfterIntro(osgViewer::Viewer& viewer, double nowT);
    void update(double nowT);

    bool travelStarted() const { return travelStarted_; }
    double travelStartTime() const { return travelStartT_; }

    bool introFinished() const { return globePhase_.introFinished; }
    bool manyFinished() const { return globePhase_.manyFinished; }
    bool densityFinished() const { return globePhase_.densityFinished; }
    void setGlobeMix(float mix);
    void updateTextureFades(double nowT);

private:
    unsigned int globeMask_ = 0;

    osg::ref_ptr<osg::Group> globeScene_;
    osg::ref_ptr<osg::Node> starfieldNode_;
    osg::ref_ptr<osg::MatrixTransform> globeXform_;
    osg::ref_ptr<osg::Node> globeNode_;
    osg::ref_ptr<osg::Uniform> globeMixUniform_;

    osg::observer_ptr<ZoomTransitionController> zoomCtrl_;
    osg::observer_ptr<TimeCounter> timeCounter_;

    pcve::PhaseState globePhase_;
    pcve::Spin spin_;
    pcve::Travel travel_;
    pcve::Density density_;
    pcve::Many many_;
    pcve::Trajectories trajMany_;
    pcve::Trajectories trajDensity_;

    std::unique_ptr<netCDF::NcFile> globeNcFile_;

    bool spinStarted_ = false;
    bool travelStarted_ = false;
    bool manyStarted_ = false;
    bool densityStarted_ = false;

    bool firstFadeStarted_ = false;
    bool firstFadeDone_ = false;
    bool secondFadeStarted_ = false;
    bool secondFadeDone_ = false;

    double spinStartT_ = -1.0;
    double travelStartT_ = -1.0;
    double secondFadeStartT_ = -1.0;

    double firstFadeDelaySec_ = 3.5;
    double fadeDurationSec_ = 1.5;
};

#endif // PLASTICPARTICLEVE_GLOBESEQUENCE_H