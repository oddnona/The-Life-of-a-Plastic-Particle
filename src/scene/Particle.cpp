#include "Particle.h"
#include "FollowCameraManipulator.h"
#include <osg/Depth>
#include <osg/ShapeDrawable>
#include <osg/StateSet>
#include <osg/CullFace>
#include <osg/NodeCallback>
#include <osg/Geometry>
#include <osg/Array>          // for Vec3Array, Vec4Array, etc.
#include <osg/PrimitiveSet>   // for osg::DrawElementsUInt

#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <osg/Geode>
#include <osg/MatrixTransform>
#include <osg/BlendFunc>
#include <osg/Program>
#include <osg/Shader>
#include <osg/Uniform>

namespace {
    constexpr float PI = 3.14159265358979323846f;
}

// static osg::Vec4 depthToColor(float depthMeters)
// {
//     if (depthMeters < 1000.0f)
//         return osg::Vec4(1.0f, 0.65f, 0.0f, 0.85f);
//     else if (depthMeters < 2000.0f)
//         return osg::Vec4(1.0f, 0.3f, 0.0f, 0.9f);
//     else if (depthMeters < 3000.0f)
//         return osg::Vec4(1.0f, 0.0f, 0.2f, 0.9f);
//     else if (depthMeters < 4000.0f)
//         return osg::Vec4(0.8f, 0.0f, 0.8f, 0.9f);
//     else
//         return osg::Vec4(0.5f, 0.0f, 0.5f, 0.9f);
// }

static osg::Vec4 depthToColor(float depthMeters)
{
    if (depthMeters < 0.0f) depthMeters = 0.0f;
    if (depthMeters > 5000.0f) depthMeters = 5000.0f;
    // map depth to brightness
    float t = depthMeters / 5000.0f; // 0..1
    float v = 1.00f - 0.20f * t; // 1.00..0.80
    return osg::Vec4(v, v, v, 1.0f);
}

// xyz loader

static std::vector<osg::Vec3> loadXYZ(const std::string& path)
{
    std::ifstream in(path);
    std::vector<osg::Vec3> pts;

    if (!in) {
        std::cerr << "[Particle] Could not open XYZ file: " << path << "\n";
        return pts;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') continue;

        std::istringstream iss(line);
        std::string first;
        if (!(iss >> first)) continue;

        double x, y, z;
        if (iss >> x >> y >> z) {
            pts.emplace_back(static_cast<float>(x),
                             static_cast<float>(y),
                             static_cast<float>(z));
        } else {
            iss.clear();
            iss.str(line);

            if (iss >> x >> y >> z) {
                pts.emplace_back(static_cast<float>(x),
                                 static_cast<float>(y),
                                 static_cast<float>(z));
            } else {
                try {
                    x = std::stod(first);
                    if (iss >> y >> z) {
                        pts.emplace_back(static_cast<float>(x),
                                         static_cast<float>(y),
                                         static_cast<float>(z));
                    }
                } catch (...) {
                }
            }
        }
    }

    std::cerr << "[Particle] Loaded " << pts.size()
              << " points from " << path << "\n";
    return pts;
}
// build a tubular mesh along a polyline
static osg::ref_ptr<osg::Geometry> makeTube(const std::vector<osg::Vec3>& P,
                                            float radius = 20000.0f,
                                            int slices = 16)
{
    osg::ref_ptr<osg::Geometry>  geom = new osg::Geometry;
    osg::ref_ptr<osg::Vec3Array> vtx = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec3Array> norms = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec4Array> cols = new osg::Vec4Array;

    osg::Vec4 col(0.2f, 1.0f, 0.2f, 1.0f);
    cols->push_back(col);
    geom->setColorArray(cols.get(), osg::Array::BIND_OVERALL);

    if (P.size() < 2) {
        geom->setVertexArray(vtx.get());
        geom->setNormalArray(norms.get(), osg::Array::BIND_PER_VERTEX);
        return geom;
    }

    int ringStride = slices + 1;

    // one ring per point
    for (std::size_t i = 0; i < P.size(); ++i) {
        osg::Vec3 t;
        if (i == 0)
            t = P[1] - P[0];
        else if (i == P.size() - 1)
            t = P[i] - P[i - 1];
        else
            t = P[i + 1] - P[i - 1];

        t.normalize();

        osg::Vec3 up = std::fabs(t.z()) < 0.9f ? osg::Vec3(0, 0, 1) : osg::Vec3(0, 1, 0);
        osg::Vec3 n = up ^ t; n.normalize();
        osg::Vec3 b = t ^ n; b.normalize();

        for (int s = 0; s <= slices; ++s) {
            float a = 2.0f * PI * s / slices;
            osg::Vec3 R = n * std::cos(a) + b * std::sin(a);
            vtx->push_back(P[i] + R * radius);
            norms->push_back(R);
        }
    }

    // connect adjacent rings with triangle strips
    for (std::size_t i = 0; i + 1 < P.size(); ++i) {
        int base = static_cast<int>(i * ringStride);
        osg::ref_ptr<osg::DrawElementsUInt> idx =
            new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLE_STRIP);
        for (int s = 0; s < ringStride; ++s) {
            idx->push_back(base + s);
            idx->push_back(base + ringStride + s);
        }
        geom->addPrimitiveSet(idx.get());
    }

    geom->setVertexArray(vtx.get());
    geom->setNormalArray(norms.get(), osg::Array::BIND_PER_VERTEX);
    // enable blending / transparency
    osg::StateSet* ss = geom->getOrCreateStateSet();
    ss->setMode(GL_BLEND, osg::StateAttribute::ON);
    osg::ref_ptr<osg::CullFace> cf = new osg::CullFace(osg::CullFace::BACK);
    ss->setAttributeAndModes(cf.get(), osg::StateAttribute::ON);
    geom->setDataVariance(osg::Object::DYNAMIC);
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);

    return geom;
}

class ParticleUpdateCallback : public osg::NodeCallback
{
public:
    explicit ParticleUpdateCallback(Particle* owner)
        : _owner(owner) {}

    void operator()(osg::Node* node, osg::NodeVisitor* nv) override
    {
        if (_owner) {
            _owner->update();
        }
        traverse(node, nv);
    }

private:
    Particle* _owner;
};

std::vector<osg::Vec3> resampleTrajectory(const std::vector<osg::Vec3>& P, float spacing)
{
    std::vector<osg::Vec3> out;
    if (P.size() < 2) return out;
    out.push_back(P.front());
    float acc = 0.0f;

    for (size_t i = 1; i < P.size(); ++i)
    {
        osg::Vec3 a = P[i - 1];
        osg::Vec3 b = P[i];
        float d = (b - a).length();
        if (d <= 1e-6f)
            continue;

        osg::Vec3 dir = (b - a) / d;

        while (acc + spacing <= d)
        {
            acc += spacing;
            out.push_back(a + dir * acc);
        }

        acc -= d;
    }

    return out;
}

static osg::Vec3 catmullRomPoint(const osg::Vec3& P0,
                                 const osg::Vec3& P1,
                                 const osg::Vec3& P2,
                                 const osg::Vec3& P3,
                                 float u)
{
    auto tj = [](float ti, const osg::Vec3& A, const osg::Vec3& B) -> float
    {
        const float d = (B - A).length();
        return ti + std::sqrt(std::max(d, 1e-6f));
    };

    const float t0 = 0.0f;
    const float t1 = tj(t0, P0, P1);
    const float t2 = tj(t1, P1, P2);
    const float t3 = tj(t2, P2, P3);

    const float t = t1 + u * (t2 - t1);

    const osg::Vec3 A1 = P0 * ((t1 - t) / (t1 - t0)) + P1 * ((t - t0) / (t1 - t0));
    const osg::Vec3 A2 = P1 * ((t2 - t) / (t2 - t1)) + P2 * ((t - t1) / (t2 - t1));
    const osg::Vec3 A3 = P2 * ((t3 - t) / (t3 - t2)) + P3 * ((t - t2) / (t3 - t2));

    const osg::Vec3 B1 = A1 * ((t2 - t) / (t2 - t0)) + A2 * ((t - t0) / (t2 - t0));
    const osg::Vec3 B2 = A2 * ((t3 - t) / (t3 - t1)) + A3 * ((t - t1) / (t3 - t1));

    const osg::Vec3 C = B1 * ((t2 - t) / (t2 - t1)) + B2 * ((t - t1) / (t2 - t1));
    return C;
}

static std::vector<osg::Vec3> smoothCatmullRom(const std::vector<osg::Vec3>& P,
                                               int samplesPerSegment)
{
    std::vector<osg::Vec3> out;

    if (P.size() < 2)
        return out;

    if (P.size() == 2)
        return P;

    if (samplesPerSegment < 1)
        samplesPerSegment = 1;

    out.reserve((P.size() - 1) * samplesPerSegment + 1);

    for (std::size_t i = 0; i + 1 < P.size(); ++i)
    {
        const osg::Vec3& P1 = P[i];
        const osg::Vec3& P2 = P[i + 1];
        const osg::Vec3& P0 = (i == 0) ? P[i] : P[i - 1];
        const osg::Vec3& P3 = (i + 2 < P.size()) ? P[i + 2] : P[i + 1];

        for (int s = 0; s < samplesPerSegment; ++s)
        {
            const float u = static_cast<float>(s) / static_cast<float>(samplesPerSegment);
            out.push_back(catmullRomPoint(P0, P1, P2, P3, u));
        }
    }

    out.push_back(P.back());
    return out;
}

Particle::Particle(const std::string& xyzPath,
                   unsigned int frameStep)
    : _frameStep(frameStep)
    , _frameCounter(0)
    , _index(0)
    , _valid(false)
{
    _points = loadXYZ(xyzPath);
    _valid = !_points.empty();

    if (!_valid)
    {
        std::cerr << "[Particle] No points loaded, nothing to show.\n";
        return;
    }

    // Exaggeration
    const float exaggeration = 100.0f;
    for (auto& p : _points)
        p.z() *= exaggeration;

    _points = smoothCatmullRom(_points, 6);
    _points = resampleTrajectory(_points, 2000.0f);

    _index = 0;

    init();
    setUpdateCallback(new ParticleUpdateCallback(this));
}

void Particle::init()
{
    // the particle
    osg::ref_ptr<osg::Sphere> sphere =
        new osg::Sphere(osg::Vec3(0.0f, 0.0f, 0.0f), 500.0f);

    osg::ref_ptr<osg::ShapeDrawable> sd =
        _particleDrawable = new osg::ShapeDrawable(sphere.get());
    _particleDrawable->setColor(osg::Vec4(1.0f, 0.65f, 0.0f, 1.0f));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(_particleDrawable.get());
    osg::StateSet* pss = geode->getOrCreateStateSet();
    pss->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE); // keep fixed-function lighting off
    pss->setMode(GL_NORMALIZE, osg::StateAttribute::ON); // ensure normals stay correct under scaling
    // depth-stable
    pss->setMode(GL_BLEND, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    pss->setRenderingHint(osg::StateSet::OPAQUE_BIN);
    pss->setAttributeAndModes(new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, true),
                              osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    pss->setMode(GL_CULL_FACE, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    // particle plastic shading (Blinn–Phong)
    {
        static const char* vsrc = R"GLSL(
            #version 120
            varying vec3 vN;
            varying vec3 vV;

            void main()
            {
                vec4 eyePos = gl_ModelViewMatrix * gl_Vertex;
                vV = -eyePos.xyz; // vector from surface to eye in eye-space
                vN = gl_NormalMatrix * gl_Normal; // normal in eye-space
                gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
            }
        )GLSL";

        static const char* fsrc = R"GLSL(
            #version 120
            varying vec3 vN;
            varying vec3 vV;

            uniform vec4 uBaseColor; // rgba
            uniform vec3 uLightDir; // eye-space direction from surface to light (normalized)
            uniform vec3 uSpecColor; // specular color
            uniform float uAmbient; // ambient factor
            uniform float uDiffuse; // diffuse factor
            uniform float uSpecular; // specular factor
            uniform float uShininess; // shininess exponent
            uniform vec3 uWaterColor; // underwater tint (rgb)
            uniform float uFresnelPow; // rim exponent (higher = tighter rim)
            uniform float uFresnelAmt; // rim intensity
            uniform float uFogAmt; // 0..1: how much to blend toward water color

            void main()
            {
                vec3 N = normalize(vN);
                vec3 V = normalize(vV);
                vec3 L = normalize(uLightDir);
                float ndl = max(dot(N, L), 0.0);

                // Blinn–Phong half-vector
                vec3 H = normalize(L + V);
                float ndh = max(dot(N, H), 0.0);
                float spec = 0.0;
                if (ndl > 0.0)
                    spec = pow(ndh, uShininess);

                vec3 base = uBaseColor.rgb;
                // base lighting
                vec3 lit = base * (uAmbient + uDiffuse * ndl)
                         + uSpecColor * (uSpecular * spec);

                // Fresnel rim
                float ndv = max(dot(N, V), 0.0);
                float fres = pow(1.0 - ndv, uFresnelPow);
                lit += uWaterColor * (uFresnelAmt * fres);

                // underwater tint
                vec3 color = mix(lit, uWaterColor, clamp(uFogAmt, 0.0, 1.0));
                gl_FragColor = vec4(color, uBaseColor.a);
            }
        )GLSL";

        osg::ref_ptr<osg::Shader> vs = new osg::Shader(osg::Shader::VERTEX, vsrc);
        osg::ref_ptr<osg::Shader> fs = new osg::Shader(osg::Shader::FRAGMENT, fsrc);
        osg::ref_ptr<osg::Program> prog = new osg::Program;
        prog->addShader(vs.get());
        prog->addShader(fs.get());
        pss->setAttributeAndModes(prog.get(), osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

        // uniforms
        _uParticleBaseColor = new osg::Uniform("uBaseColor", osg::Vec4(1.0f, 0.65f, 0.0f, 1.0f));
        pss->addUniform(_uParticleBaseColor.get());
        pss->addUniform(new osg::Uniform("uLightDir",  osg::Vec3(0.0f, 0.0f, 1.0f)));         // headlight in eye-space
        pss->addUniform(new osg::Uniform("uSpecColor", osg::Vec3(1.0f, 1.0f, 1.0f)));         // white highlight

        pss->addUniform(new osg::Uniform("uAmbient",   0.12f));
        pss->addUniform(new osg::Uniform("uDiffuse",   1.00f));
        pss->addUniform(new osg::Uniform("uSpecular",  0.0f));
        pss->addUniform(new osg::Uniform("uShininess", 64.0f));

        pss->addUniform(new osg::Uniform("uWaterColor", osg::Vec3(0.05f, 0.35f, 0.55f))); // blue-green water
        pss->addUniform(new osg::Uniform("uFresnelPow",  3.0f));   // rim width control
        pss->addUniform(new osg::Uniform("uFresnelAmt",  0.35f));  // rim intensity
        pss->addUniform(new osg::Uniform("uFogAmt",      0.20f));  // blend toward water color
    }

    _particleXform = new osg::MatrixTransform;
    _particleXform->addChild(geode.get());
    const osg::Vec3& first = _points.front();
    _particleXform->setMatrix(osg::Matrix::translate(first));
    std::cout << "[Particle] init: before addChild(_particleXform)\n";
    addChild(_particleXform.get());
    std::cout << "[Particle] init: after addChild(_particleXform)\n";
    std::cout << "[Particle] init: before initBubble()\n";
    initBubble();
    std::cout << "[Particle] init: after initBubble()\n";
    std::cout << "[Particle] init: before initTailTube()\n";
    initTailTube();
    std::cout << "[Particle] init: after initTailTube()\n";
    std::cout << "[Particle] Created particle at first point: ("
              << first.x() << ", " << first.y() << ", " << first.z() << ")\n";
    std::cout << "[Particle] init: before first updateTailTubeWindow()\n";
    updateTailTubeWindow();
    std::cout << "[Particle] init: after first updateTailTubeWindow()\n";
}

void Particle::initTailTube()
{
    _tailGeode = new osg::Geode;
    _tailGeode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    _tailGeom = new osg::Geometry;
    _tailVtx = new osg::Vec3Array;
    _tailNrm = new osg::Vec3Array;
    _tailIdx = new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);

    const int ringStride = _tailSlices + 1;                // closed ring (duplicate seam)
    const std::size_t maxRings = _points.size();           // full path capacity
    const std::size_t maxVerts = maxRings * ringStride;
    const std::size_t maxIndices =
        (maxRings > 1) ? ((maxRings - 1) * std::size_t(_tailSlices) * 6) : 0;

    _tailBuiltRings = 0;
    _tailVtx->resize(maxVerts);
    _tailNrm->resize(maxVerts);
    _tailIdx->clear();
    _tailIdx->reserve(maxIndices);
    _tailIdx->dirty();
    _tailGeom->setVertexArray(_tailVtx.get());
    _tailGeom->setNormalArray(_tailNrm.get(), osg::Array::BIND_PER_VERTEX);

    // color (overall)
    osg::ref_ptr<osg::Vec4Array> cols = new osg::Vec4Array;
    //cols->push_back(osg::Vec4(0.2f, 1.0f, 0.2f, 0.85f));
    cols->push_back(osg::Vec4(1.0f, 0.0f, 1.0f, 0.6f));

    _tailGeom->setColorArray(cols.get(), osg::Array::BIND_OVERALL);
    _tailGeom->addPrimitiveSet(_tailIdx.get());
    osg::StateSet* ss = _tailGeom->getOrCreateStateSet();
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    ss->setMode(GL_BLEND, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    ss->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    ss->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA),
                         osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    ss->setAttributeAndModes(new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false),
                             osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    ss->setMode(GL_CULL_FACE, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    _tailGeom->setDataVariance(osg::Object::DYNAMIC);
    _tailGeom->setUseDisplayList(false);
    _tailGeom->setUseVertexBufferObjects(true);
    _tailGeode->addDrawable(_tailGeom.get());
    addChild(_tailGeode.get());
}
void Particle::initBubble()
{
    // bubble geometry
    osg::ref_ptr<osg::Sphere> s = new osg::Sphere(osg::Vec3(0,0,0), 1.0f);
    osg::ref_ptr<osg::ShapeDrawable> sd =
        _bubbleDrawable = new osg::ShapeDrawable(s.get());

    // base tint + alpha
    sd->setColor(osg::Vec4(0.85f, 1.00f, 0.10f, 0.10f));  // slight neon yellow
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(sd.get());
    osg::StateSet* ss = geode->getOrCreateStateSet();
    // transparent shell render state
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    ss->setMode(GL_BLEND, osg::StateAttribute::ON  | osg::StateAttribute::OVERRIDE);
    ss->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    ss->setAttributeAndModes(
        new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA),
        osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE
    );
    ss->setAttributeAndModes(
        new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false),
        osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE
    );

    ss->setMode(GL_CULL_FACE, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);

    // bubble Fresnel shader
    {
        static const char* vsrc = R"GLSL(
            #version 120
            varying vec3 vN;
            varying vec3 vV;
            void main()
            {
                vec4 eyePos = gl_ModelViewMatrix * gl_Vertex;
                vV = -eyePos.xyz;
                vN = gl_NormalMatrix * gl_Normal;
                gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
            }
        )GLSL";

        static const char* fsrc = R"GLSL(
            #version 120
            varying vec3 vN;
            varying vec3 vV;

            uniform vec4  uBubbleColor; // rgba
            uniform float uRimPow; // rim tightness
            uniform float uRimAmt; // rim intensity

            void main()
            {
                vec3 N = normalize(vN);
                vec3 V = normalize(vV);

                float ndv = max(dot(N, V), 0.0);
                float rim = pow(1.0 - ndv, uRimPow);

                vec3 rgb = uBubbleColor.rgb * (0.25 + uRimAmt * rim);
                float a = uBubbleColor.a * (0.45 + 0.75 * rim);
                gl_FragColor = vec4(rgb, a);
            }
        )GLSL";

        osg::ref_ptr<osg::Shader> vs = new osg::Shader(osg::Shader::VERTEX, vsrc);
        osg::ref_ptr<osg::Shader> fs = new osg::Shader(osg::Shader::FRAGMENT, fsrc);
        osg::ref_ptr<osg::Program> prog = new osg::Program;
        prog->addShader(vs.get());
        prog->addShader(fs.get());

        ss->setAttributeAndModes(prog.get(), osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

        _uBubbleColor = new osg::Uniform("uBubbleColor", osg::Vec4(0.85f, 1.00f, 0.10f, 0.10f));
        ss->addUniform(_uBubbleColor.get());
        ss->addUniform(new osg::Uniform("uRimPow", 3.5f));
        ss->addUniform(new osg::Uniform("uRimAmt", 1.15f));
    }

    // scale transform (bubble lives under the particle transform so it follows position)
    _bubbleScaleXform = new osg::MatrixTransform;
    _bubbleScaleXform->addChild(geode.get());
    _bubbleRadiusCurrent = _bubbleRadiusStart;
    _bubbleScaleXform->setMatrix(osg::Matrix::scale(_bubbleRadiusCurrent,
                                                    _bubbleRadiusCurrent,
                                                    _bubbleRadiusCurrent));
    if (_particleXform.valid())
        _particleXform->addChild(_bubbleScaleXform.get());
}

void Particle::writeTailRing(std::size_t ringIndex)
{
    if (!_tailVtx.valid() || !_tailNrm.valid()) return;
    if (_points.size() < 2) return;
    if (ringIndex >= _points.size()) return;

    const int ringStride = _tailSlices + 1;
    const osg::Vec3& Pi = _points[ringIndex];

    osg::Vec3 t;
    if (ringIndex == 0)
        t = _points[1] - _points[0];
    else if (ringIndex == _points.size() - 1)
        t = _points[ringIndex] - _points[ringIndex - 1];
    else
        t = _points[ringIndex + 1] - _points[ringIndex - 1];

    if (t.length2() < 1e-12f) t = osg::Vec3(0, 1, 0);
    t.normalize();

    osg::Vec3 up = (std::fabs(t.z()) < 0.9f) ? osg::Vec3(0, 0, 1) : osg::Vec3(0, 1, 0);

    osg::Vec3 n = up ^ t;
    if (n.length2() < 1e-12f) n = osg::Vec3(1, 0, 0);
    n.normalize();

    osg::Vec3 b = t ^ n;
    if (b.length2() < 1e-12f) b = osg::Vec3(0, 0, 1);
    b.normalize();
    const std::size_t base = ringIndex * ringStride;

    for (int s = 0; s <= _tailSlices; ++s)
    {
        const float a = 2.0f * PI * float(s) / float(_tailSlices);
        osg::Vec3 R = n * std::cos(a) + b * std::sin(a);
        (*_tailVtx)[base + s] = Pi + R * _tailRadius;
        (*_tailNrm)[base + s] = R;
    }
}
void Particle::appendTailSegment(std::size_t ringIndex)
{
    if (!_tailIdx.valid()) return;
    if (ringIndex == 0) return;

    const int ringStride = _tailSlices + 1;
    const unsigned int base0 = static_cast<unsigned int>((ringIndex - 1) * ringStride);
    const unsigned int base1 = static_cast<unsigned int>(ringIndex * ringStride);

    for (int s = 0; s < _tailSlices; ++s)
    {
        const unsigned int a = base0 + s;
        const unsigned int b = base0 + s + 1;
        const unsigned int c = base1 + s;
        const unsigned int d = base1 + s + 1;

        _tailIdx->push_back(a);
        _tailIdx->push_back(c);
        _tailIdx->push_back(b);

        _tailIdx->push_back(b);
        _tailIdx->push_back(c);
        _tailIdx->push_back(d);
    }
}

void Particle::resetBubbleGrowth()
{
    _bubbleRadiusCurrent = _bubbleRadiusStart;
    _bubbleCutawayStartTime = -1.0;

    if (_bubbleScaleXform.valid())
    {
        _bubbleScaleXform->setMatrix(osg::Matrix::scale(_bubbleRadiusCurrent,
                                                        _bubbleRadiusCurrent,
                                                        _bubbleRadiusCurrent));
    }
}

void Particle::startBubbleCutawayClock()
{
    _bubbleCutawayStartTime = osg::Timer::instance()->time_s();
}

void Particle::updateBubbleNow()
{
    updateBubble();
}
void Particle::updateBubble()
{
    static int s_bubblePrintCount = 0;
    if (s_bubblePrintCount < 20)
    {
        std::cout << "[Particle] updateBubble enter\n";
        ++s_bubblePrintCount;
    }

    if (!_bubbleScaleXform.valid()) return;

    if (_bubbleCutawayStartTime < 0.0)
    {
        _bubbleRadiusCurrent = _bubbleRadiusStart;
    }
    else
    {
        const double now = osg::Timer::instance()->time_s();
        const float elapsedSec = static_cast<float>(now - _bubbleCutawayStartTime);

        if (elapsedSec <= _bubbleGrowStartSec)
        {
            _bubbleRadiusCurrent = _bubbleRadiusStart;
        }
        else if (elapsedSec >= _bubbleGrowEndSec)
        {
            _bubbleRadiusCurrent = _bubbleRadiusEnd;
        }
        else
        {
            const float u = (elapsedSec - _bubbleGrowStartSec) / (_bubbleGrowEndSec - _bubbleGrowStartSec);
            _bubbleRadiusCurrent = _bubbleRadiusStart + u * (_bubbleRadiusEnd - _bubbleRadiusStart);
        }
    }

    if (_uBubbleColor.valid())
    {
        _uBubbleColor->set(osg::Vec4(0.85f, 1.00f, 0.10f, 0.10f));
    }

    static int s_bubbleRadiusPrintCount = 0;
    if (s_bubbleRadiusPrintCount < 200)
    {
        std::cerr << "[Particle] bubble radius=" << _bubbleRadiusCurrent
                  << " bubbleStartTime=" << _bubbleCutawayStartTime
                  << "\n";
        ++s_bubbleRadiusPrintCount;
    }

    _bubbleScaleXform->setMatrix(osg::Matrix::scale(_bubbleRadiusCurrent,
                                                    _bubbleRadiusCurrent,
                                                    _bubbleRadiusCurrent));
}

void Particle::updateTailTubeWindow()
{
    static int s_tailPrintCount = 0;
    if (s_tailPrintCount < 20)
    {
        std::cout << "[Particle] updateTailTubeWindow enter: index=" << _index
                  << " builtRings=" << _tailBuiltRings
                  << " points=" << _points.size() << "\n";
        ++s_tailPrintCount;
    }
    if (!_tailGeom.valid() || !_tailVtx.valid() || !_tailNrm.valid() || !_tailIdx.valid()) return;
    if (!_valid || _points.size() < 2) return;
    if (_index >= _points.size()) return;
    //if (_index >= _points.size() - 1) return;
    // Build ring 0 the first time
    if (_tailBuiltRings == 0)
    {
        writeTailRing(0);
        _tailBuiltRings = 1;
        _tailVtx->dirty();
        _tailNrm->dirty();
        _tailIdx->dirty();
        _tailGeom->dirtyBound();
    }

    while (_tailBuiltRings <= _index)
    {
        writeTailRing(_tailBuiltRings);
        appendTailSegment(_tailBuiltRings);
        ++_tailBuiltRings;
    }

    _tailVtx->dirty();
    _tailNrm->dirty();
    _tailIdx->dirty();
    _tailGeom->dirtyBound();
}

void Particle::update()
{
    static int s_updatePrintCount = 0;
    if (s_updatePrintCount < 20)
    {
        std::cout << "[Particle] update enter: index=" << _index
                  << " frameCounter=" << _frameCounter
                  << " points=" << _points.size() << "\n";
        ++s_updatePrintCount;
    }
    if (_paused) return;
    if (!_valid || _points.empty()) return;

    ++_frameCounter;
    if (_frameCounter % _frameStep != 0)
        return;

    const osg::Vec3& p = _points[_index];
    _particleXform->setMatrix(osg::Matrix::translate(p));
    float depthMeters = -p.z() / 100.0f;

    if (_uParticleBaseColor.valid())
        _uParticleBaseColor->set(depthToColor(depthMeters));

    updateTailTubeWindow();
    updateBubble();

    if (_index + 1 < _points.size())
        ++_index;
}
void Particle::seekNormalized(float t01)
{
    if (!_valid || _points.empty()) return;

    if (t01 < 0.0f) t01 = 0.0f;
    if (t01 > 1.0f) t01 = 1.0f;

    const size_t N = _points.size();
    const size_t idx = static_cast<size_t>(t01 * float(N - 1));
    _index = idx;
    const osg::Vec3& p = _points[_index];
    if (_particleXform.valid())
        _particleXform->setMatrix(osg::Matrix::translate(p));

    float depthMeters = -p.z() / 100.0f;
    if (_uParticleBaseColor.valid())
        _uParticleBaseColor->set(depthToColor(depthMeters));

    _tailIdx->clear();
    _tailBuiltRings = 0;

    if (_index < _points.size())
    {
        for (std::size_t i = 0; i <= _index; ++i)
        {
            writeTailRing(i);
            if (i > 0)
                appendTailSegment(i);
        }
        _tailBuiltRings = _index + 1;
    }

    _tailVtx->dirty();
    _tailNrm->dirty();
    _tailIdx->dirty();
    _tailGeom->dirtyBound();
    updateBubble();
}


osg::Vec3 Particle::getCurrentPosition() const
{
    if (_index < _points.size())
        return _points[_index];
    return osg::Vec3(0,0,0);
}

osg::Vec3 Particle::getNextPosition() const
{
    if (_points.empty()) return osg::Vec3(0,0,0);

    if (_index + 1 < _points.size())
        return _points[_index + 1];

    return _points.back();
}


bool Particle::isAtEnd() const
{
    return (_index + 1 >= _points.size());
}

osg::Vec3 Particle::getStartPosition() const
{
    if (_points.empty())
        return osg::Vec3(0, 0, 0);

    return _points.front();
}