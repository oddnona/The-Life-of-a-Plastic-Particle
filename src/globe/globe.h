#ifndef PLASTICPARTICLEVE_GLOBE_H
#define PLASTICPARTICLEVE_GLOBE_H
#pragma once

#include <osg/Node>
#include <osg/ref_ptr>
#include <string>

namespace pcve {

    class Globe {
    public:
        static osg::ref_ptr<osg::Node> createTexturedSphere(const std::string& imagePath);

        static osg::ref_ptr<osg::Node> createBlendedSphere(
            const std::string& imagePath0,
            const std::string& imagePath1
        );
    };

}

#endif // PLASTICPARTICLEVE_GLOBE_H