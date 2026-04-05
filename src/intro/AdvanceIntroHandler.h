    #include <osg/ref_ptr>
    class AdvanceIntroHandler : public osgGA::GUIEventHandler
    {
    public:
        explicit AdvanceIntroHandler(Intro* intro) : _intro(intro) {}

        bool handle(const osgGA::GUIEventAdapter& ea,
                    osgGA::GUIActionAdapter&) override
        {
            if (!_intro.valid() || _intro->isFinished())
                return false;

            if (ea.getEventType() == osgGA::GUIEventAdapter::KEYDOWN &&
                ea.getKey() == osgGA::GUIEventAdapter::KEY_Space)
            {
                _intro->advanceStage();
                return true;
            }
            return false;
        }

    private:
        osg::ref_ptr<Intro> _intro;
    };
