#include "dynamicScale.h"
#include <osg/Geode>
#include <osg/StateSet>
#include <osg/Depth>
#include <osgDB/ReadFile>
#include <iomanip>
#include <sstream>

DynamicScale::DynamicScale()
{
    _root = new osg::Group;
    _root->setNodeMask(0x0); // hidden by default

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;

    _text = new osgText::Text;
    _text->setText("scale: 0 km");

    _text->setAlignment(osgText::TextBase::RIGHT_BOTTOM);
    _text->setPosition(osg::Vec3(0.98f, 0.02f, 0.0f));

    // white text
    _text->setColor(osg::Vec4(1.f, 1.f, 1.f, 1.f));

    // character size in normalized HUD units
    _text->setCharacterSize(0.025f);
    _text->setFontResolution(128, 128);
    _text->setBackdropType(osgText::Text::NONE);

    // ensure the text draws as 2D overlay
    _text->setAxisAlignment(osgText::TextBase::XY_PLANE);
    _text->setDataVariance(osg::Object::DYNAMIC);

    geode->addDrawable(_text.get());

    osg::StateSet* ss = geode->getOrCreateStateSet();
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);

    _root->addChild(geode.get());
}

void DynamicScale::setVisible(bool v)
{
    _visible = v;
    _root->setNodeMask(v ? 0xffffffff : 0x0);
}

void DynamicScale::setDay(int day)
{
    if (!_text.valid()) return;
    const char* unit = "km";

    if (day >= 1100)
    {
        unit = "km";
    }
    else if (day >= 800)
    {
        unit = "m";
    }
    else if (day >= 700)
    {
        unit = "nm";
    }
    else if (day >= 530)
    {
        unit = "um";
    }
    else if (day >= 410)
    {
        unit = "mm";
    }
    else if (day >= 320)
    {
        unit = "cm";
    }
    else if (day >= 270)
    {
        unit = "m";
    }

    int value = 1;

    if (day <= 269)
    {
        const int startValue = 8300;
        const int endValue = 1;
        const int startDay = 0;
        const int endDay = 269;
        value = startValue - ((startValue - endValue) * (day - startDay)) / (endDay - startDay);
    }
    else if (day <= 319)
    {
        const int startValue = 999;
        const int endValue = 1;
        const int startDay = 270;
        const int endDay = 319;
        value = startValue - ((startValue - endValue) * (day - startDay)) / (endDay - startDay);
    }
    else if (day <= 409)
    {
        const int startValue = 999;
        const int endValue = 1;
        const int startDay = 320;
        const int endDay = 409;
        value = startValue - ((startValue - endValue) * (day - startDay)) / (endDay - startDay);
    }
    else if (day <= 529)
    {
        const int startValue = 9;
        const int endValue = 1;
        const int startDay = 410;
        const int endDay = 529;
        value = startValue - ((startValue - endValue) * (day - startDay)) / (endDay - startDay);
    }
    else if (day <= 699)
    {
        const int startValue = 999;
        const int endValue = 1;
        const int startDay = 530;
        const int endDay = 699;
        value = startValue - ((startValue - endValue) * (day - startDay)) / (endDay - startDay);
    }
    else if (day <= 799)
    {
        const int startValue = 999;
        const int endValue = 50;
        const int startDay = 700;
        const int endDay = 799;
        value = startValue - ((startValue - endValue) * (day - startDay)) / (endDay - startDay);
    }
    else if (day <= 1099)
    {
        const int startValue = 1;
        const int endValue = 999;
        const int startDay = 800;
        const int endDay = 1099;
        value = startValue + ((endValue - startValue) * (day - startDay)) / (endDay - startDay);
    }
    else if (day <= 2300)
    {
        const int startValue = 1;
        const int endValue = 8300;
        const int startDay = 1100;
        const int endDay = 2300;
        value = startValue + ((endValue - startValue) * (day - startDay)) / (endDay - startDay);
    }
    else
    {
        value = 8300;
    }

    if (value < 1)
    {
        value = 1;
    }

    std::ostringstream oss;
    oss << "scale: " << value << " " << unit;
    _text->setText(oss.str());
}