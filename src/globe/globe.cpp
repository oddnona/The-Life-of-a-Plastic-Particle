#include "globe.h"

#include <osg/TexEnv>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Texture2D>
#include <osg/Uniform>
#include <osg/Array>
#include <osg/Notify>
#include <osg/PrimitiveSet>
#include <osg/Program>
#include <osg/Shader>
#include <osgDB/ReadFile>
#include <filesystem>

namespace pcve {

static osg::ref_ptr<osg::Geometry> makeSphereGeometry()
{
    const int lonSegments = 128;
    const int latSegments = 64;
    const float radius = 1.0f;

    osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec2Array> uvs = new osg::Vec2Array();
    osg::ref_ptr<osg::DrawElementsUInt> idx = new osg::DrawElementsUInt(GL_TRIANGLES);

    verts->reserve((latSegments + 1) * (lonSegments + 1));
    uvs->reserve((latSegments + 1) * (lonSegments + 1));

    for (int lat = 0; lat <= latSegments; ++lat)
    {
        const float v = static_cast<float>(lat) / static_cast<float>(latSegments);
        const float phi = (0.5f - v) * osg::PI;
        const float cosPhi = std::cos(phi);
        const float sinPhi = std::sin(phi);

        for (int lon = 0; lon <= lonSegments; ++lon)
        {
            const float u = static_cast<float>(lon) / static_cast<float>(lonSegments);
            const float theta = u * osg::PI * 2.0f + osg::PI;
            const float x = cosPhi * std::cos(theta);
            const float y = cosPhi * std::sin(theta);
            const float z = sinPhi;

            verts->push_back(osg::Vec3(x * radius, y * radius, z * radius));
            //float uu = u + 0.5f;
            //if (uu > 1.0f) uu -= 1.0f;
            uvs->push_back(osg::Vec2(u, 1.0f - v));
        }
    }

    auto indexOf = [lonSegments](int lat, int lon) {
        return static_cast<unsigned int>(lat * (lonSegments + 1) + lon);
    };

    for (int lat = 0; lat < latSegments; ++lat)
    {
        for (int lon = 0; lon < lonSegments; ++lon)
        {
            unsigned int i0 = indexOf(lat, lon);
            unsigned int i1 = indexOf(lat, lon + 1);
            unsigned int i2 = indexOf(lat + 1, lon);
            unsigned int i3 = indexOf(lat + 1, lon + 1);

            idx->push_back(i0); idx->push_back(i1); idx->push_back(i2);
            idx->push_back(i2); idx->push_back(i1); idx->push_back(i3);
        }
    }

    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    geom->setUseVertexBufferObjects(true);
    geom->setUseDisplayList(false);

    geom->setVertexArray(verts.get());
    geom->setTexCoordArray(0, uvs.get());
    geom->addPrimitiveSet(idx.get());

    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(1, 1, 1, 1));
    geom->setColorArray(colors.get(), osg::Array::BIND_OVERALL);

    return geom;
}

    static osg::ref_ptr<osg::Texture2D> makeTextureFromImage(const std::string& imagePath)
{
    std::cerr << "[Globe] trying to load texture: " << imagePath << "\n";
    std::cerr << "[Globe] exists? " << std::filesystem::exists(imagePath) << "\n";

    osg::ref_ptr<osg::Image> img = osgDB::readRefImageFile(imagePath);
    if (!img) {
        std::cerr << "[Globe] FAILED to load texture: " << imagePath << "\n";
        return nullptr;
    }

    std::cerr << "[Globe] loaded texture OK: " << imagePath
              << " size=" << img->s() << "x" << img->t() << "\n";

    osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D(img.get());
    tex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
    tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    tex->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
    tex->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
    tex->setMaxAnisotropy(8.0f);

    return tex;
}

osg::ref_ptr<osg::Node> Globe::createTexturedSphere(const std::string& imagePath)
{
    osg::ref_ptr<osg::Texture2D> tex = makeTextureFromImage(imagePath);
    if (!tex) return nullptr;
    osg::ref_ptr<osg::Geometry> geom = makeSphereGeometry();
    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geom.get());

    osg::StateSet* ss = geode->getOrCreateStateSet();
    ss->setTextureAttributeAndModes(0, tex.get(), osg::StateAttribute::ON);
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

    const char* vs =
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
        "  v_uv = gl_MultiTexCoord0.st;\n"
        "}\n";

    const char* fs =
        "uniform sampler2D u_tex;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(u_tex, v_uv);\n"
        "}\n";

    osg::ref_ptr<osg::Program> prog = new osg::Program;
    prog->addShader(new osg::Shader(osg::Shader::VERTEX, vs));
    prog->addShader(new osg::Shader(osg::Shader::FRAGMENT, fs));
    ss->setAttributeAndModes(prog.get(), osg::StateAttribute::ON);
    ss->addUniform(new osg::Uniform("u_tex", 0));

    return geode;
}

osg::ref_ptr<osg::Node> Globe::createBlendedSphere(
    const std::string& imagePath0,
    const std::string& imagePath1)
{
    osg::ref_ptr<osg::Texture2D> tex0 = makeTextureFromImage(imagePath0);
    osg::ref_ptr<osg::Texture2D> tex1 = makeTextureFromImage(imagePath1);
    if (!tex0 || !tex1) {
        std::cerr << "[Globe] createBlendedSphere failed because one or both textures are null\n";
        return nullptr;
    }

    osg::ref_ptr<osg::Geometry> geom = makeSphereGeometry();
    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geom.get());

    osg::StateSet* ss = geode->getOrCreateStateSet();
    ss->setTextureAttributeAndModes(0, tex0.get(), osg::StateAttribute::ON);
    ss->setTextureAttributeAndModes(1, tex1.get(), osg::StateAttribute::ON);
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

    const char* vs =
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
        "  v_uv = gl_MultiTexCoord0.st;\n"
        "}\n";

    const char* fs =
        "uniform sampler2D u_tex0;\n"
        "uniform sampler2D u_tex1;\n"
        "uniform float u_mix;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  vec4 c0 = texture2D(u_tex0, v_uv);\n"
        "  vec4 c1 = texture2D(u_tex1, v_uv);\n"
        "  gl_FragColor = mix(c0, c1, clamp(u_mix, 0.0, 1.0));\n"
        "}\n";

    osg::ref_ptr<osg::Program> prog = new osg::Program;
    prog->addShader(new osg::Shader(osg::Shader::VERTEX, vs));
    prog->addShader(new osg::Shader(osg::Shader::FRAGMENT, fs));
    ss->setAttributeAndModes(prog.get(), osg::StateAttribute::ON);

    ss->addUniform(new osg::Uniform("u_tex0", 0));
    ss->addUniform(new osg::Uniform("u_tex1", 1));
    ss->addUniform(new osg::Uniform("u_mix", 0.0f));

    return geode;
}

} // namespace pcve