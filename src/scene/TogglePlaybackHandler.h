#pragma once

#include <osgGA/GUIEventHandler>
#include <iostream>
#include "scene/Particle.h"

class TogglePlaybackHandler : public osgGA::GUIEventHandler
{
public:
    TogglePlaybackHandler(Particle* particle)
        : _particle(particle) {}

    bool handle(const osgGA::GUIEventAdapter& ea,
                osgGA::GUIActionAdapter&) override
    {
        if (!_particle) return false;

        if (ea.getEventType() == osgGA::GUIEventAdapter::KEYDOWN &&
            ea.getKey() == osgGA::GUIEventAdapter::KEY_Space)
        {
            bool paused = _particle->isPaused();
            _particle->setPaused(!paused);

            std::cout << (paused ? "PLAY\n" : "PAUSE\n");
            return true;
        }
        return false;
    }

private:
    Particle* _particle{};
};
