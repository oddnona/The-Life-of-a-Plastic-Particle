#include "trajectories.h"
#include "utils_netcdf.h"
#include "utils_geo_color.h"
#include <osg/LineWidth>
#include <osg/Point>
#include <osg/Array>
#include <osg/StateSet>
#include <osg/Notify>
#include <osg/BlendFunc>
#include <osg/Texture2D>
#include <osg/Depth>
#include <vector>
#include <cmath>
#include <algorithm>

namespace pcve {

    static Trajectories::Params sanitize(const Trajectories::Params& p) {
        Trajectories::Params o = p;
        o.stride = std::max(1, o.stride);
        o.maxParticles = std::max(1, o.maxParticles);
        o.obsStride = std::max(1, o.obsStride);
        o.lineWidth = std::max(0.5f, o.lineWidth);
        o.fadeSteps = std::max(0, o.fadeSteps);
        return o;
    }
    //hue jitter
    static float clamp01(float x) {
        return (x < 0.0f) ? 0.0f : (x > 1.0f ? 1.0f : x);
    }
    //luma weights (approx brightness)
    static float luminance709(const osg::Vec3& rgb)
    {
        return 0.2126f * rgb.x() + 0.7152f * rgb.y() + 0.0722f * rgb.z();
    }

    static osg::Vec3 enforceIsoluminance(const osg::Vec3& rgb, float targetY)
    {
        const float y = luminance709(rgb);
        if (y <= 1e-6f) {
            return rgb;
        }

        const float k = targetY / y;

        osg::Vec3 out(rgb.x() * k, rgb.y() * k, rgb.z() * k);

        out.x() = clamp01(out.x());
        out.y() = clamp01(out.y());
        out.z() = clamp01(out.z());

        return out;
    }

    static osg::Vec3 rgbToHsv(const osg::Vec3& rgb)
    {
        const float r = rgb.x(), g = rgb.y(), b = rgb.z();
        const float mx = std::max(r, std::max(g, b));
        const float mn = std::min(r, std::min(g, b));
        const float d  = mx - mn;
        float h = 0.0f;
        float s = (mx <= 0.0f) ? 0.0f : (d / mx);
        float v = mx;

        if (d > 0.0f) {
            if (mx == r) {
                h = (g - b) / d;
                if (g < b) h += 6.0f;
            } else if (mx == g) {
                h = 2.0f + (b - r) / d;
            } else { // mx == b
                h = 4.0f + (r - g) / d;
            }
            h /= 6.0f; // to [0..1)
        }

        return osg::Vec3(h, s, v);
    }

    static osg::Vec3 hsvToRgb(const osg::Vec3& hsv)
    {
        float h = hsv.x();
        float s = hsv.y();
        float v = hsv.z();
        h = h - std::floor(h);

        if (s <= 0.0f) {
            return osg::Vec3(v, v, v);
        }

        const float hf = h * 6.0f;
        const int i = (int)std::floor(hf);
        const float f = hf - (float)i;

        const float p = v * (1.0f - s);
        const float q = v * (1.0f - s * f);
        const float t = v * (1.0f - s * (1.0f - f));

        switch (i % 6) {
            case 0: return osg::Vec3(v, t, p);
            case 1: return osg::Vec3(q, v, p);
            case 2: return osg::Vec3(p, v, t);
            case 3: return osg::Vec3(p, q, v);
            case 4: return osg::Vec3(t, p, v);
            default:return osg::Vec3(v, p, q);
        }
    }

    static void applyHueOffset(osg::Vec4& c, float hueOffset)
    {
        osg::Vec3 rgb(c.r(), c.g(), c.b());
        osg::Vec3 hsv = rgbToHsv(rgb);
        hsv.x() = hsv.x() + hueOffset; // wrap handled in hsvToRgb
        osg::Vec3 rgb2 = hsvToRgb(hsv);

        c.r() = clamp01(rgb2.x());
        c.g() = clamp01(rgb2.y());
        c.b() = clamp01(rgb2.z());
    }

    static osg::Vec3 lerpVec3(const osg::Vec3& a, const osg::Vec3& b, float t)
    {
        t = clamp01(t);
        return osg::Vec3(
            a.x() + (b.x() - a.x()) * t,
            a.y() + (b.y() - a.y()) * t,
            a.z() + (b.z() - a.z()) * t
        );
    }

    static osg::Vec3 cet12Approx(float t)
    {
        t = clamp01(t);
        // 3-stop approximation of ColorCET cwr / CET_CBTD1
        const osg::Vec3 c0(0.25f, 0.75f, 0.85f); // cyan
        const osg::Vec3 c1(0.92f, 0.92f, 0.92f); // light neutral center
        const osg::Vec3 c2(0.93f, 0.66f, 0.60f); // salmon

        if (t < 0.5f) {
            return lerpVec3(c0, c1, t * 2.0f);
        } else {
            return lerpVec3(c1, c2, (t - 0.5f) * 2.0f);
        }
    }

bool Trajectories::build(netCDF::NcFile& file, const Params& paramsIn)
{
    const Params p = sanitize(paramsIn);
    netCDF::NcVar lonVar = pcve::nc::getVarAny(file, {"lon", "longitude"});
    netCDF::NcVar latVar = pcve::nc::getVarAny(file, {"lat", "latitude"});

    if (lonVar.isNull() || latVar.isNull() || lonVar.getDimCount() != 2 || latVar.getDimCount() != 2) {
        osg::notify(osg::WARN) << "[Trajectories] lon/lat not found or wrong dims\n";
        return false;
    }

    const size_t nTrajAll = lonVar.getDim(0).getSize();
    const size_t nObs = lonVar.getDim(1).getSize();

    // choose trajectories
    std::vector<size_t> selected;
    selected.reserve((size_t)p.maxParticles);
    for (size_t idx = 0; idx < nTrajAll && (int)selected.size() < p.maxParticles; ++idx) {
        if (p.stride > 1 && (idx % (size_t)p.stride) != 0) continue;
        selected.push_back(idx);
    }
    if (selected.empty()) {
        osg::notify(osg::WARN) << "[Trajectories] selection empty\n";
        return false;
    }

    verts_ = new osg::Vec3Array();
    colors_ = new osg::Vec4Array();
        baseAlpha_.clear();
        globalAlpha_ = 1.0f;
        prefixEndPrimCount_.clear();
        drawArrays_.clear();
        primOriginalCounts_.clear();
        primBaseFirst_.clear();
        densityPointDrawArrays_.clear();
        densityTrajBaseFirst_.clear();
        densityTrajCounts_.clear();
        enabledPrimSets_ = 0;
    prefixEndPrimCount_.reserve(selected.size());

    const size_t estObs = (nObs + (size_t)p.obsStride - 1) / (size_t)p.obsStride;
    verts_->reserve(selected.size() * estObs);
    colors_->reserve(selected.size() * estObs);
    baseAlpha_.reserve(selected.size() * estObs);
    geom_ = new osg::Geometry();
    geom_->setUseVertexBufferObjects(true);
    geom_->setUseDisplayList(false);
    geom_->setDataVariance(osg::Object::DYNAMIC);

    geom_->setVertexArray(verts_.get());
    geom_->setColorArray(colors_.get(), osg::Array::BIND_PER_VERTEX);
        densityVerts_ = new osg::Vec3Array();
        densityColors_ = new osg::Vec4Array();

        densityGeom_ = new osg::Geometry();
        densityGeom_->setUseVertexBufferObjects(true);
        densityGeom_->setUseDisplayList(false);
        densityGeom_->setDataVariance(osg::Object::DYNAMIC);

        densityGeom_->setVertexArray(densityVerts_.get());
        densityGeom_->setColorArray(densityColors_.get(), osg::Array::BIND_PER_VERTEX);

    // fast membership
    std::vector<uint8_t> want(nTrajAll, 0);
    for (size_t idx : selected) want[idx] = 1;

    const size_t blockTrajCount = 256;
    std::vector<float> lonBlock, latBlock;
    size_t processedSelected = 0;

    for (size_t blockStart = 0;
         blockStart < nTrajAll && processedSelected < selected.size();
         blockStart += blockTrajCount)
    {
        size_t thisBlock = std::min(blockTrajCount, nTrajAll - blockStart);

        if (!pcve::nc::readTrajBlock2D(lonVar, blockStart, thisBlock, lonBlock)) break;
        if (!pcve::nc::readTrajBlock2D(latVar, blockStart, thisBlock, latBlock)) break;

        for (size_t k = 0; k < thisBlock && processedSelected < selected.size(); ++k)
        {
            size_t pIndex = blockStart + k;
            if (!want[pIndex]) continue;

            const float* lonRow = lonBlock.data() + k * nObs;
            const float* latRow = latBlock.data() + k * nObs;
            // collect valid points first so we can fade out near the death
            std::vector<osg::Vec3> tmpVerts;
            tmpVerts.reserve((nObs + (size_t)p.obsStride - 1) / (size_t)p.obsStride);

            bool started = false;
            for (size_t t = 0; t < nObs; t += (size_t)p.obsStride)
            {
                float lo = lonRow[t];
                float la = latRow[t];

                const bool ok = std::isfinite(lo) && std::isfinite(la);
                if (!ok) {
                    if (started) break;
                    continue;
                }

                started = true;
                tmpVerts.push_back(pcve::util::lonLat_toXYZ(lo, la, p.radiusSlightlyAbove));
            }

            const int count = (int)tmpVerts.size();
            if (count >= 2)
            {
                const int first = (int)verts_->size();
                // small hue offset per trajectory
                float hueOffset = (float)((pIndex * 37) % 1000) / 1000.0f; // 0..1
                hueOffset = (hueOffset - 0.5f) * 0.08f; // range [-0.04, +0.04]
                for (int i = 0; i < count; ++i) {
                    verts_->push_back(tmpVerts[(size_t)i]);
                }

                // fade alpha over the last vertices
                const int fadeLen = (p.fadeSteps <= 0) ? 0 : std::min(p.fadeSteps, count);

                for (int i = 0; i < count; ++i)
                {
                    float u = (count > 1) ? (float)i / (float)(count - 1) : 0.0f;
                    float t = u;

                    osg::Vec3 rgb = cet12Approx(t);
                    osg::Vec4 c(rgb.x(), rgb.y(), rgb.z(), 0.75f);
                    applyHueOffset(c, hueOffset);
                    float a = 1.0f;
                    if (fadeLen >= 2) {
                        const int fadeStart = count - fadeLen; // index where fade begins
                        if (i >= fadeStart) {
                            // i == fadeStart -> a = 1
                            // i == count-1 -> a = 0
                            a = (float)(count - 1 - i) / (float)(fadeLen - 1);
                            if (a < 0.0f) a = 0.0f;
                            if (a > 1.0f) a = 1.0f;
                        }
                    }
                    c.a() = c.a() * a;
                    colors_->push_back(c);
                    baseAlpha_.push_back(c.a());
                }

                osg::ref_ptr<osg::DrawArrays> da = new osg::DrawArrays(GL_LINE_STRIP, first, count);
                da->setCount(0); // hidden initially

                geom_->addPrimitiveSet(da.get());
                drawArrays_.push_back(da);
                primOriginalCounts_.push_back(count);
                primBaseFirst_.push_back(first);
                const int densityIndex = (int)densityVerts_->size();
                densityVerts_->push_back(tmpVerts[0]);
                densityColors_->push_back(osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
                osg::ref_ptr<osg::DrawArrays> dp = new osg::DrawArrays(GL_POINTS, densityIndex, 1);
                dp->setCount(0); // hidden initially

                densityGeom_->addPrimitiveSet(dp.get());
                densityPointDrawArrays_.push_back(dp);
                densityTrajBaseFirst_.push_back(first);
                densityTrajCounts_.push_back(count);
            }

            prefixEndPrimCount_.push_back((unsigned int)geom_->getNumPrimitiveSets());
            ++processedSelected;
        }
    }

        geode_ = new osg::Geode();
        geode_->addDrawable(geom_.get());
        geode_->addDrawable(densityGeom_.get());

        osg::StateSet* ss = geode_->getOrCreateStateSet();
        ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        ss->setMode(GL_LINE_SMOOTH, osg::StateAttribute::OFF);

        // depth test ON, and allow equal depth fragments to pass (so overlaps can blend)
        osg::ref_ptr<osg::Depth> depth = new osg::Depth;
        depth->setFunction(osg::Depth::LEQUAL);
        depth->setWriteMask(true);
        ss->setAttributeAndModes(depth.get(), osg::StateAttribute::ON);

        //overlap becomes more opaque
        ss->setMode(GL_BLEND, osg::StateAttribute::ON);
        ss->setAttributeAndModes(
            new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA),
            osg::StateAttribute::ON
        );
        ss->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        ss->setAttributeAndModes(new osg::LineWidth(p.lineWidth), osg::StateAttribute::ON);

        osg::StateSet* dss = densityGeom_->getOrCreateStateSet();
        dss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        dss->setMode(GL_BLEND, osg::StateAttribute::OFF);
        dss->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
        dss->setRenderingHint(osg::StateSet::OPAQUE_BIN);
        dss->setRenderBinDetails(999, "RenderBin");

        // enable point sprites
        dss->setMode(GL_POINT_SPRITE, osg::StateAttribute::ON);
        dss->setMode(GL_VERTEX_PROGRAM_POINT_SIZE, osg::StateAttribute::ON);
        dss->setAttributeAndModes(new osg::Point(p.lineWidth), osg::StateAttribute::ON);
        osg::ref_ptr<osg::Image> img = new osg::Image();
        const int size = 16;
        img->allocateImage(size, size, 1, GL_RGBA, GL_UNSIGNED_BYTE);

        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const float dx = (x + 0.5f) / (float)size - 0.5f;
                const float dy = (y + 0.5f) / (float)size - 0.5f;
                const float r = std::sqrt(dx * dx + dy * dy);

                unsigned char* ptr = img->data(x, y);

                if (r > 0.5f) {
                    ptr[0] = 0;
                    ptr[1] = 0;
                    ptr[2] = 0;
                    ptr[3] = 0;
                    continue;
                }

                const float t = r / 0.5f;
                if (t <= 0.35f) {
                    ptr[0] = 255; // R
                    ptr[1] = 250; // G
                    ptr[2] = 210; // B
                    ptr[3] = 255; // A
                } else {
                    const float u = (t - 0.35f) / (1.0f - 0.35f);

                    const float r0 = 255.0f;
                    const float g0 = 220.0f;
                    const float b0 = 80.0f;

                    const float r1 = 255.0f;
                    const float g1 = 110.0f;
                    const float b1 = 0.0f;

                    const float rr = r0 + (r1 - r0) * u;
                    const float gg = g0 + (g1 - g0) * u;
                    const float bb = b0 + (b1 - b0) * u;

                    ptr[0] = (unsigned char)rr;
                    ptr[1] = (unsigned char)gg;
                    ptr[2] = (unsigned char)bb;
                    ptr[3] = 255;
                }
            }
        }

        osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D(img.get());
        tex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        dss->setTextureAttributeAndModes(0, tex.get(), osg::StateAttribute::ON);
        return valid();
}

    bool Trajectories::valid() const
    {
        return geode_.valid() && geom_.valid() && densityGeom_.valid() &&
               !drawArrays_.empty() &&
               !densityPointDrawArrays_.empty() &&
               !prefixEndPrimCount_.empty() &&
               drawArrays_.size() == primOriginalCounts_.size() &&
               drawArrays_.size() == primBaseFirst_.size() &&
               densityPointDrawArrays_.size() == densityTrajBaseFirst_.size() &&
               densityPointDrawArrays_.size() == densityTrajCounts_.size() &&
               drawArrays_.size() == densityPointDrawArrays_.size();
    }

    void Trajectories::revealUpTo(size_t k)
    {
        if (!valid()) return;
        const size_t nSelected = prefixEndPrimCount_.size();
        if (k > nSelected) k = nSelected;
        //hide everything
        unsigned int targetPrimSets = 0;
        if (k > 0) {
            targetPrimSets = prefixEndPrimCount_[k - 1];
        }
        // if we are increasing, enable new primitive sets
        if (targetPrimSets > enabledPrimSets_)
        {
            for (unsigned int i = enabledPrimSets_; i < targetPrimSets; ++i)
            {
                osg::DrawArrays* da = drawArrays_[i].get();
                if (!da) continue;

                // restore base first in case a window animation moved it
                if (i < primBaseFirst_.size()) da->setFirst(primBaseFirst_[i]);

                da->setCount(primOriginalCounts_[i]);
            }
        }
        // if we are decreasing, disable primitive sets that are beyond target
        else if (targetPrimSets < enabledPrimSets_)
        {
            for (unsigned int i = targetPrimSets; i < enabledPrimSets_; ++i)
            {
                osg::DrawArrays* da = drawArrays_[i].get();
                if (!da) continue;
                if (i < primBaseFirst_.size()) da->setFirst(primBaseFirst_[i]);
                da->setCount(0);
            }
        }

        enabledPrimSets_ = targetPrimSets;
        if (geom_.valid()) geom_->dirtyBound();
    }

    size_t Trajectories::maxVertexCount() const
    {
        if (!valid()) return 0;
        size_t m = 0;
        for (int c : primOriginalCounts_) {
            if (c > 0) m = std::max(m, (size_t)c);
        }
        return m;
    }

    void Trajectories::setVisiblePrefix(size_t nVerts)
    {
        if (!valid()) return;
        const unsigned int nPrim = (unsigned int)drawArrays_.size();
        if (nPrim == 0) return;
        for (unsigned int i = 0; i < nPrim; ++i)
        {
            osg::DrawArrays* da = drawArrays_[i].get();
            if (!da) continue;

            const int full = primOriginalCounts_[i];
            if (full <= 0) { da->setCount(0); continue; }

            int cnt = 0;
            if (nVerts > 0)
            {
                cnt = (int)std::min(nVerts, (size_t)full);
            }

            da->setCount(cnt);
        }

        enabledPrimSets_ = nPrim;

        if (geom_.valid()) geom_->dirtyBound();
    }

    void Trajectories::setVisibleWindow(size_t headVert, size_t segLenVerts)
    {
        if (!valid()) return;
        const unsigned int nPrim = (unsigned int)drawArrays_.size();
        if (nPrim == 0) return;
        if (segLenVerts < 1) segLenVerts = 1;

        for (unsigned int i = 0; i < nPrim; ++i)
        {
            osg::DrawArrays* da = drawArrays_[i].get();
            if (!da) continue;
            const int full = primOriginalCounts_[i];
            if (full <= 0) { da->setCount(0); continue; }
            const int baseFirst = (i < primBaseFirst_.size()) ? primBaseFirst_[i] : da->getFirst();

            int head = (int)headVert;
            if (head < 0) head = 0;
            if (head > full - 1) head = full - 1;

            const int segLen = std::max(2, (int)segLenVerts);
            int tail = head - (segLen - 1);
            if (tail < 0) tail = 0;
            const int count = head - tail + 1;

            da->setFirst(baseFirst + tail);
            da->setCount(count);

            for (int j = 0; j < count; ++j)
            {
                int v = baseFirst + tail + j;
                osg::Vec4& c = (*colors_)[v];
                c.set(1.0f, 0.5f, 0.0f, 1.0f);
            }
        }

        enabledPrimSets_ = nPrim;
        colors_->dirty();
        if (geom_.valid()) geom_->dirtyBound();
    }

    void Trajectories::setVisiblePoint(double headPos)
    {
        if (!valid()) return;
        const unsigned int nTraj = (unsigned int)densityPointDrawArrays_.size();
        if (nTraj == 0) return;

        for (unsigned int i = 0; i < nTraj; ++i)
        {
            osg::DrawArrays* lineDa = drawArrays_[i].get();
            if (lineDa) {
                if (i < primBaseFirst_.size())
                    lineDa->setFirst(primBaseFirst_[i]);
                lineDa->setCount(0);
            }

            osg::DrawArrays* pointDa = densityPointDrawArrays_[i].get();
            if (!pointDa) continue;

            const int full = densityTrajCounts_[i];
            if (full <= 0) {
                pointDa->setCount(0);
                continue;
            }

            if (headPos < 0.0 || headPos > (double)(full - 1)) {
                pointDa->setCount(0);
                continue;
            }

            const int baseFirst = densityTrajBaseFirst_[i];

            int i0 = (int)std::floor(headPos);
            if (i0 < 0) i0 = 0;
            if (i0 > full - 1) i0 = full - 1;
            int i1 = i0 + 1;
            if (i1 > full - 1) i1 = full - 1;

            const double u = headPos - (double)i0;

            const osg::Vec3& p0 = (*verts_)[baseFirst + i0];
            const osg::Vec3& p1 = (*verts_)[baseFirst + i1];

            osg::Vec3 p(
                (float)((1.0 - u) * p0.x() + u * p1.x()),
                (float)((1.0 - u) * p0.y() + u * p1.y()),
                (float)((1.0 - u) * p0.z() + u * p1.z())
            );

            (*densityVerts_)[i] = p;
            (*densityColors_)[i].set(1.0f, 1.0f, 1.0f, 1.0f);

            pointDa->setFirst((int)i);
            pointDa->setCount(1);
        }

        densityVerts_->dirty();
        densityColors_->dirty();
        if (densityGeom_.valid()) densityGeom_->dirtyBound();
        if (geom_.valid()) geom_->dirtyBound();
    }

    void Trajectories::hideAll()
    {
        if (!valid()) return;

        const unsigned int nLine = (unsigned int)drawArrays_.size();
        for (unsigned int i = 0; i < nLine; ++i)
        {
            osg::DrawArrays* da = drawArrays_[i].get();
            if (!da) continue;

            if (i < primBaseFirst_.size())
                da->setFirst(primBaseFirst_[i]);

            da->setCount(0);
        }

        const unsigned int nPoint = (unsigned int)densityPointDrawArrays_.size();
        for (unsigned int i = 0; i < nPoint; ++i)
        {
            osg::DrawArrays* dp = densityPointDrawArrays_[i].get();
            if (!dp) continue;
            dp->setFirst((int)i);
            dp->setCount(0);
        }

        enabledPrimSets_ = 0;
        if (geom_.valid()) geom_->dirtyBound();
        if (densityGeom_.valid()) densityGeom_->dirtyBound();
    }

    void Trajectories::setGlobalAlpha(float a)
    {
        if (a < 0.0f) a = 0.0f;
        if (a > 1.0f) a = 1.0f;
        globalAlpha_ = a;

        if (!colors_.valid()) return;
        if (baseAlpha_.size() != colors_->size()) return;

        const size_t n = colors_->size();
        for (size_t i = 0; i < n; ++i)
        {
            osg::Vec4& c = (*colors_)[i];
            c.a() = baseAlpha_[i] * globalAlpha_;
        }

        colors_->dirty();
    }
    void Trajectories::setColorOverride(const osg::Vec4& color)
    {
        if (!colors_.valid()) return;
        colorOverrideActive_ = true;
        overrideColor_ = color;
        const size_t n = colors_->size();

        for (size_t i = 0; i < n; ++i)
        {
            osg::Vec4& c = (*colors_)[i];
            float a = baseAlpha_[i];
            c = osg::Vec4(color.r(), color.g(), color.b(), a);
        }

        colors_->dirty();
    }

    void Trajectories::clearColorOverride()
    {
        if (!colors_.valid()) return;

        colorOverrideActive_ = false;

        const size_t n = colors_->size();

        for (size_t i = 0; i < n; ++i)
        {
            osg::Vec4& c = (*colors_)[i];
            c.a() = baseAlpha_[i];
        }

        colors_->dirty();
    }
    void Trajectories::disableDeathFade()
    {
        if (!colors_.valid()) return;
        const size_t n = colors_->size();
        for (size_t i = 0; i < n; ++i)
        {
            osg::Vec4& c = (*colors_)[i];
            c.a() = 1.0f;
        }

        colors_->dirty();
    }

    void Trajectories::restoreDeathFade()
    {
        if (!colors_.valid()) return;
        if (baseAlpha_.size() != colors_->size()) return;

        const size_t n = colors_->size();
        for (size_t i = 0; i < n; ++i)
        {
            osg::Vec4& c = (*colors_)[i];
            c.a() = baseAlpha_[i];
        }

        colors_->dirty();
    }
} // namespace pcve