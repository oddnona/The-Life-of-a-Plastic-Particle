#include "timeCounter.h"
#include <osg/Geode>
#include <osg/StateSet>
#include <osg/Depth>
#include <osgDB/ReadFile>
#include <iomanip>
#include <sstream>

TimeCounter::TimeCounter()
{
    _root = new osg::Group;
    _root->setNodeMask(0x0); // hidden by default

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;

    _text = new osgText::Text;
    _scaleText = new osgText::Text;
    _currentDay = 0;

    _text->setText("time: 0 days");
    _scaleText->setText("scale: megameters");

    _text->setPosition(osg::Vec3(0.02f, 0.02f, 0.0f));
    _scaleText->setPosition(osg::Vec3(0.30f, 0.02f, 0.0f));

    _text->setColor(osg::Vec4(1.f, 1.f, 1.f, 1.f));
    _scaleText->setColor(osg::Vec4(1.f, 1.f, 1.f, 1.f));

    _text->setCharacterSize(0.025f);
    _scaleText->setCharacterSize(0.025f);

    _text->setFontResolution(128, 128);
    _scaleText->setFontResolution(128, 128);

    _text->setBackdropType(osgText::Text::NONE);
    _scaleText->setBackdropType(osgText::Text::NONE);

    _text->setAxisAlignment(osgText::TextBase::XY_PLANE);
    _scaleText->setAxisAlignment(osgText::TextBase::XY_PLANE);

    _text->setDataVariance(osg::Object::DYNAMIC);
    _scaleText->setDataVariance(osg::Object::DYNAMIC);

    geode->addDrawable(_text.get());
    geode->addDrawable(_scaleText.get());

    osg::StateSet* ss = geode->getOrCreateStateSet();
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);

    _root->addChild(geode.get());
}

void TimeCounter::setVisible(bool v)
{
    _visible = v;
    _root->setNodeMask(v ? 0xffffffff : 0x0);
}

void TimeCounter::setDay(int day)
{
    if (!_text.valid() || !_scaleText.valid()) return;
    if (day < 0) day = 0;
    bool reversing = (_prevDay >= 0 && day < _prevDay);
    _currentDay = day;

    std::ostringstream oss;
    oss << "time: " << _currentDay << " " << (_unit == Unit::Days ? "days" : "years");
    _text->setText(oss.str());

    if (reversing)
    {
        // force megameters when going backwards
        _scaleText->setText("scale: megameters");
    }
    else
    {
        if (_currentDay >= 540 && _currentDay < 630)
            _scaleText->setText("scale: nanometers");
        else if (_currentDay >= 340 && _currentDay < 540)
            _scaleText->setText("scale: millimeters");
        else
            _scaleText->setText("scale: megameters");
    }
    _prevDay = day;
}

void TimeCounter::setUnit(Unit u)
{
    _unit = u;
    if (!_text.valid() || !_scaleText.valid()) return;

    std::ostringstream oss;
    oss << "time: " << _currentDay << " " << (_unit == Unit::Days ? "days" : "years");
    _text->setText(oss.str());

    if (_currentDay >= 500 && _currentDay < 1000)
        _scaleText->setText("scale: millimeters");
    else
        _scaleText->setText("scale: megameters");
}