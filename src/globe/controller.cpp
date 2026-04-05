#include "controller.h"

#include "globe.h"
#include "trajectories.h"
#include "travel.h"
#include "density.h"

#include <osg/Group>
#include <osg/Camera>
#include <osg/Viewport>
#include <osg/Notify>
#include <osg/GraphicsContext>

#include <osgDB/ReadFile>
#include <osgGA/TrackballManipulator>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include <netcdf>
#include <iostream>
#include <algorithm>

namespace pcve {

int Controller::run(const Paths& paths, const Args& args)
{
    osg::setNotifyLevel(osg::WARN);

    // Scene root
    osg::ref_ptr<osg::Group> root = new osg::Group();

    // Globe
    osg::ref_ptr<osg::Node> globe = Globe::createTexturedSphere(paths.texturePath);
    if (!globe) return 1;
    root->addChild(globe.get());

    // Viewer
    osgViewer::Viewer viewer;
    viewer.setThreadingModel(osgViewer::Viewer::SingleThreaded);
    viewer.setCameraManipulator(new osgGA::TrackballManipulator());
    viewer.addEventHandler(new osgViewer::StatsHandler());
    viewer.addEventHandler(new osgViewer::WindowSizeHandler());
    viewer.getCamera()->setClearColor(osg::Vec4(0.08f, 0.08f, 0.08f, 1.0f));
    viewer.getCamera()->setNearFarRatio(0.0001);

    // Explicit window
    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits();
    traits->x = 50;
    traits->y = 50;
    traits->width = 1280;
    traits->height = 720;
    traits->windowDecoration = true;
    traits->doubleBuffer = true;

    osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits.get());
    if (!gc.valid()) {
        std::cerr << "[err] failed to create GraphicsContext\n";
        return 1;
    }

    osg::ref_ptr<osg::Camera> cam = viewer.getCamera();
    cam->setGraphicsContext(gc.get());
    cam->setDrawBuffer(GL_BACK);
    cam->setReadBuffer(GL_BACK);
    viewer.setSceneData(root.get());
    viewer.realize();

    // viewport
    if (auto* gw = dynamic_cast<osgViewer::GraphicsWindow*>(gc.get()))
    {
        int x, y, w, h;
        gw->getWindowRectangle(x, y, w, h);
        cam->setViewport(new osg::Viewport(0, 0, w, h));
        double fovy, aspect, zNear, zFar;
        if (cam->getProjectionMatrixAsPerspective(fovy, aspect, zNear, zFar))
            cam->setProjectionMatrixAsPerspective(fovy, (double)w / (double)h, zNear, zFar);
        else
            cam->setProjectionMatrixAsPerspective(30.0, (double)w / (double)h, 0.01, 1000.0);
    }

    netCDF::NcFile file(paths.ncPath, netCDF::NcFile::read);
    // build trajectories (hidden until reveal)
    Trajectories traj;
    Trajectories::Params trajP;
    trajP.stride = std::max(1, args.stride);
    trajP.maxParticles = std::max(1, args.maxParticles);
    trajP.obsStride = std::max(1, args.obsStride);
    trajP.lineWidth = std::max(0.5f, args.lineWidth);
    trajP.radiusSlightlyAbove = 1.002f;
    const bool hasTraj = traj.build(file, trajP);
    if (hasTraj) {
        root->addChild(traj.node());
        // starts hidden because build() setCount(0) for each primitive set
    } else {
        std::cerr << "[warn] trajectories built empty or inconsistent\n";
    }

    // build travel intro
    PhaseState phase;
    Travel travel;
    Travel::Params travelP;
    travelP.particleIndex = std::max(0, args.introParticleIndex);
    travelP.stepEveryFrames = std::max(1, args.introStepEveryFrames);
    travelP.trailWidth = std::max(0.5f, args.introTrailWidth);
    travelP.markerRadius = std::max(0.001f, args.introMarkerRadius);
    travelP.radiusSlightlyAbove = 1.002f;

    const bool hasIntro = travel.build(file, travelP);
    if (hasIntro) {
        travel.addTo(root.get());
    } else {
        // if no intro, start reveal immediately
        phase.introFinished = true;
    }

    // install callbacks
    if (hasIntro) {
        travel.installCallback(root.get(), &phase);
    }

    Density density;
    Density::Params densityP;
    densityP.segmentLenVerts = 30;
    densityP.stepEveryFrames = 3;
    densityP.speedupAfterFraction = 0.30f;
    densityP.speedupFactor = 1;
    densityP.holdFullSec = 3.0;
    densityP.markDensityFinishedWhenDone = true;
    density.setParams(densityP);

    if (hasTraj) {
        density.installCallback(root.get(), &phase, &traj);
    }

    // single render loop
    while (!viewer.done())
    {
        if (!gc->valid()) break;
        viewer.frame();
    }
    return 0;
}

} // namespace pcve
