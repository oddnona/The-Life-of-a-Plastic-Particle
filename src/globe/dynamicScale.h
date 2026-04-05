#pragma once

#include <osg/Group>
#include <osgText/Text>
#include <osg/NodeCallback>
#include <osg/observer_ptr>

class DynamicScale : public osg::Referenced
 {
 public:
     DynamicScale();
     osg::Group* node() const { return _root.get(); }
     void setVisible(bool v);
     void start(double simStartTime);
     void setDay(int day);

 private:
     osg::ref_ptr<osg::Group> _root;
     osg::ref_ptr<osgText::Text> _text;
     bool _visible = false;
 };