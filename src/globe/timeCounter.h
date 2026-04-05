#pragma once

#include <osg/Group>
#include <osgText/Text>
#include <osg/NodeCallback>
#include <osg/observer_ptr>

class TimeCounter : public osg::Referenced
{
public:
    enum class Unit {
        Days,
        Years
    };

public:
    TimeCounter();
    osg::Group* node() const { return _root.get(); }
    void setVisible(bool v);
    void setDay(int day);
    void setUnit(Unit u);

private:
    osg::ref_ptr<osg::Group> _root;
    osg::ref_ptr<osgText::Text> _text;
    osg::ref_ptr<osgText::Text> _scaleText;
    Unit _unit = Unit::Days;
    int _currentDay = 0;
    bool _visible = false;
    int _prevDay = -1;
};
