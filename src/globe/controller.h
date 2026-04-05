//
// Created by Emil on 2/10/2026.
//

#ifndef PLASTICPARTICLEVE_CONTROLLER_H
#define PLASTICPARTICLEVE_CONTROLLER_H
#pragma once
#include <string>

namespace pcve {

    class Controller {
    public:
        struct Paths {
            std::string texturePath;
            std::string ncPath;
        };

        struct Args {
            // trajectories
            int stride = 1;
            int maxParticles = 10000;
            float lineWidth = 2.0f;
            int obsStride = 10;

            // travel (intro)
            int introParticleIndex = 532;
            int introStepEveryFrames = 2;
            float introTrailWidth = 3.0f;
            float introMarkerRadius = 0.001f;

            // density (reveal)
            double revealDurationSec = 50.0;
            size_t maxAddPerFrame = 50;
        };

        int run(const Paths& paths, const Args& args);
    };

} // namespace pcve



#endif //PLASTICPARTICLEVE_CONTROLLER_H