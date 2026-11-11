/**
 * @file llglslshader.cpp
 * @brief GLSL helper functions and state.
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llglslshader.h"

#include "alrendercapabilities.h"
#include "llrender.h"
#include "llthread.h"

#include "hbxxh.h"

#include <algorithm>

// Lots of STL stuff in here, using namespace std to keep things more readable
U32 LLGLSLShader::sCurBoundShader = 0;
LLGLSLShader* LLGLSLShader::sCurBoundShaderPtr = NULL;
S32 LLGLSLShader::sIndexedTextureChannels = 0;
S32 LLGLSLShader::sIndexedGLTFChannels = 0;
bool LLGLSLShader::sIndexedLegacyMaterials = false;
U32 LLGLSLShader::sMaxGLTFMaterials = 0;
U32 LLGLSLShader::sMaxGLTFNodes = 0;
bool LLGLSLShader::sProfileEnabled = false;
std::set<LLGLSLShader*>& LLGLSLShader::sInstances = *(new std::set<LLGLSLShader*>());
std::set<LLGLSLShader*>& LLGLSLShader::sConfigured = *(new std::set<LLGLSLShader*>());
U32 LLGLSLShader::sLoadGeneration = 0;
U32 LLGLSLShader::sLinked = 0;
U32 LLGLSLShader::sKept = 0;
LLUUID LLGLSLShader::sSharedObjectsKey;
LLGLSLShader::defines_map_t LLGLSLShader::sGlobalDefines;
// Starts at 1, and a default-constructed program starts level with it (see the member's
// initialiser) rather than behind: a program must not apply the environment uniform set before
// there is an environment. Wrapping is harmless -- this is only ever compared for equality.
U32 LLGLSLShader::sEnvironmentGeneration = 1;
U64 LLGLSLShader::sTotalTimeElapsed = 0;
U32 LLGLSLShader::sTotalTrianglesDrawn = 0;
U64 LLGLSLShader::sTotalSamplesDrawn = 0;
U32 LLGLSLShader::sTotalBinds = 0;
LLSD LLGLSLShader::sDefaultStats;

namespace
{
    // One bind of a profile: the program's time and counts over the draws until the next bind, from gGL's pool, read
    // when the profile finishes.
    struct ProfileRecord
    {
        LLGLSLShader* mShader = nullptr; // null once the program has gone
        ALQuerySpan   mTime;
        ALQueryTicket mStatistics;       // the pipeline statistics, where the profile counts them
        ALQueryTicket mSamples;          // samples passed, elsewhere
        ALQueryTicket mPrimitives;       // primitives generated, elsewhere
        bool          mBind = true;      // false for the rest of a bind, after a render pass ended its first record
    };

    // Leaked, as sInstances is: a program destroyed at static destruction, in whatever translation unit, still
    // forgets its records here.
    std::vector<ProfileRecord>& profile_records()
    {
        static std::vector<ProfileRecord>* records = new std::vector<ProfileRecord>();
        return *records;
    }

    constexpr size_t NO_RECORD   = size_t(-1);
    size_t           sOpenRecord = NO_RECORD;

    // What this profile counts: the pipeline statistics, where the context has them and its caller did not ask for
    // samples. The occlusion targets and the primitives target stay free to the frame's own queries then.
    bool sProfileStatistics = false;

    // Holds the targets the profile counts with for its whole length, since each bind opens them again: the frame's
    // other queries of the same targets are refused until it finishes. Or gives them back.
    void reserve_profile_targets(bool reserved)
    {
        ALQueryPool& queries = gGL.queries();
        if (sProfileStatistics)
        {
            queries.reserve(ALQueryKind::Statistics, reserved);
        }
        else
        {
            queries.reserve(ALQueryKind::Samples, reserved);
            queries.reserve(ALQueryKind::Primitives, reserved);
        }
    }

    void open_profile_record(LLGLSLShader& shader, bool bind)
    {
        ALQueryPool& queries = gGL.queries();
        ProfileRecord record;
        record.mShader = &shader;
        record.mBind   = bind;
        record.mTime   = queries.beginSpan();
        if (sProfileStatistics)
        {
            record.mStatistics = queries.beginReserved(ALQueryKind::Statistics);
        }
        else
        {
            record.mSamples = queries.beginReserved(ALQueryKind::Samples);
            if (gGLManager.canQueryPrimitives(shader.isTessellated()))
            {
                record.mPrimitives = queries.beginReserved(ALQueryKind::Primitives);
            }
        }
        sOpenRecord = profile_records().size();
        profile_records().push_back(record);
    }

    void forget_profile_records(const LLGLSLShader* shader)
    {
        for (ProfileRecord& record : profile_records())
        {
            if (record.mShader == shader)
            {
                record.mShader = nullptr;
            }
        }
    }
}

//UI shader -- declared here so llui_libtest will link properly
LLGLSLShader    gUIProgram;
LLGLSLShader    gSolidColorProgram;

// NOTE: Keep gShaderConsts* and LLGLSLShader::ShaderConsts_e in sync!
const std::string gShaderConstsKey[LLGLSLShader::NUM_SHADER_CONSTS] =
{
      "LL_SHADER_CONST_CLOUD_MOON_DEPTH"
    , "LL_SHADER_CONST_STAR_DEPTH"
};

// NOTE: Keep gShaderConsts* and LLGLSLShader::ShaderConsts_e in sync!
const std::string gShaderConstsVal[LLGLSLShader::NUM_SHADER_CONSTS] =
{
      "0.99998" // SHADER_CONST_CLOUD_MOON_DEPTH // SL-14113
    , "0.99999" // SHADER_CONST_STAR_DEPTH       // SL-14113
};

//===============================
// LLGLSL Shader implementation
//===============================

//static
void LLGLSLShader::initProfile(bool count_samples)
{
    // One still open would leave its queries counting, and its targets held, for the rest of the session.
    if (sProfileEnabled)
    {
        abandonProfile();
    }
    sProfileStatistics = !count_samples && ALQueryPool::supports(ALQueryKind::Statistics);
    reserve_profile_targets(true);
    profile_records().clear();
    sOpenRecord = NO_RECORD;

    sProfileEnabled = true;
    sTotalTimeElapsed = 0;
    sTotalTrianglesDrawn = 0;
    sTotalSamplesDrawn = 0;
    sTotalBinds = 0;

    for (auto ptr : sInstances)
    {
        ptr->clearStats();
    }
}


struct LLGLSLShaderCompareTimeElapsed
{
    bool operator()(const LLGLSLShader* const& lhs, const LLGLSLShader* const& rhs)
    {
        return lhs->mTimeElapsed < rhs->mTimeElapsed;
    }
};

//static
void LLGLSLShader::finishProfile(LLSD& stats)
{
    // The bind still open ends here, then every bind's queries are read: the first wait is for the GPU to finish the
    // profile's draws, and the rest are in by then.
    endProfileQuery();
    ALQueryPool& queries = gGL.queries();
    for (const ProfileRecord& record : profile_records())
    {
        if (!record.mShader)
        {
            continue;
        }
        U64 time       = 0;
        U64 fragments  = 0;
        U64 primitives = 0;
        queries.wait(record.mTime, time);
        ALStatistics statistics;
        if (queries.wait(record.mStatistics, statistics))
        {
            fragments  = statistics[ALStatistic::FragmentInvocations];
            primitives = statistics[ALStatistic::ClippingPrimitives];
        }
        queries.wait(record.mSamples, fragments);
        queries.wait(record.mPrimitives, primitives);

        LLGLSLShader& shader = *record.mShader;
        shader.mTimeElapsed += time;
        sTotalTimeElapsed += time;
        shader.mSamplesDrawn += fragments;
        sTotalSamplesDrawn += fragments;
        shader.mTrianglesDrawn += (U32)primitives;
        sTotalTrianglesDrawn += (U32)primitives;
        if (record.mBind)
        {
            ++shader.mBinds;
            ++sTotalBinds;
        }
    }
    profile_records().clear();
    reserve_profile_targets(false);

    sProfileEnabled = false;

    if (stats.isDefined())
    {
        // Which counters the samples and triangles are: they differ, invocations counting fragments a depth test or a
        // discard then dropped.
        stats["counters"] = sProfileStatistics ? "fragment shader invocations, clipping input primitives"
                                                : "samples passed, primitives generated";

        std::vector<LLGLSLShader*> sorted(sInstances.begin(), sInstances.end());
        std::sort(sorted.begin(), sorted.end(), LLGLSLShaderCompareTimeElapsed());

        LLSD& shaders = stats["shaders"];
        shaders = LLSD::emptyArray();
        bool unbound = false;
        for (auto ptr : sorted)
        {
            if (ptr->mBinds == 0)
            {
                unbound = true;
            }
            else
            {
                LLSD& shaderit = shaders.append(LLSD::emptyMap());
                ptr->dumpStats(shaderit);
            }
        }

        constexpr float mega = 1'000'000.f;
        float totalTimeMs = sTotalTimeElapsed / mega;
        LL_INFOS() << "-----------------------------------" << LL_ENDL;
        LL_INFOS() << "Total rendering time: " << llformat("%.4f ms", totalTimeMs) << LL_ENDL;
        LL_INFOS() << "Total samples drawn: " << llformat("%.4f million", sTotalSamplesDrawn / mega) << LL_ENDL;
        LL_INFOS() << "Total triangles drawn: " << llformat("%.3f million", sTotalTrianglesDrawn / mega) << LL_ENDL;
        LL_INFOS() << "-----------------------------------" << LL_ENDL;
        LLSD& totals = stats["totals"];
        totals = LLSD::emptyMap();
        totals["time"] = totalTimeMs / 1000.0;
        totals["binds"] = LLSD::Integer(sTotalBinds);
        // sample counters are 64-bit; store as Real to avoid S32 overflow
        totals["samples"] = F64(sTotalSamplesDrawn);
        totals["triangles"] = LLSD::Integer(sTotalTrianglesDrawn);

        LLSD& unused = stats["unused"];
        unused = LLSD::emptyArray();
        if (unbound)
        {
            LL_INFOS() << "The following shaders were unused: " << LL_ENDL;
            for (auto ptr : sorted)
            {
                if (ptr->mBinds == 0)
                {
                    LL_INFOS() << ptr->mName << LL_ENDL;
                    unused.append(ptr->mName);
                }
            }
        }
    }
}

void LLGLSLShader::clearStats()
{
    mTrianglesDrawn = 0;
    mTimeElapsed = 0;
    mSamplesDrawn = 0;
    mBinds = 0;
}

void LLGLSLShader::dumpStats(LLSD& stats)
{
    stats["name"] = mName;
    LLSD& files = stats["files"];
    files = LLSD::emptyArray();
    LL_INFOS() << "=============================================" << LL_ENDL;
    LL_INFOS() << mName << LL_ENDL;
    for (U32 i = 0; i < mShaderFiles.size(); ++i)
    {
        LL_INFOS() << mShaderFiles[i].first << LL_ENDL;
        files.append(mShaderFiles[i].first);
    }
    LL_INFOS() << "=============================================" << LL_ENDL;

    constexpr float  mega = 1'000'000.f;
    constexpr double giga = 1'000'000'000.0;
    F32 ms = mTimeElapsed / mega;
    F32 seconds = ms / 1000.f;

    F32 pct_tris = (F32)mTrianglesDrawn / (F32)sTotalTrianglesDrawn * 100.f;
    F32 tris_sec = (F32)(mTrianglesDrawn / mega);
    tris_sec /= seconds;

    F32 pct_samples = (F32)((F64)mSamplesDrawn / (F64)sTotalSamplesDrawn) * 100.f;
    F32 samples_sec = (F32)(mSamplesDrawn / giga);
    samples_sec /= seconds;

    F32 pct_binds = (F32)mBinds / (F32)sTotalBinds * 100.f;

    LL_INFOS() << "Triangles Drawn: " << mTrianglesDrawn << " " << llformat("(%.2f pct of total, %.3f million/sec)", pct_tris, tris_sec) << LL_ENDL;
    LL_INFOS() << "Binds: " << mBinds << " " << llformat("(%.2f pct of total)", pct_binds) << LL_ENDL;
    LL_INFOS() << "SamplesDrawn: " << mSamplesDrawn << " " << llformat("(%.2f pct of total, %.3f billion/sec)", pct_samples, samples_sec) << LL_ENDL;
    LL_INFOS() << "Time Elapsed: " << mTimeElapsed << " " << llformat("(%.2f pct of total, %.5f ms)\n", (F32)((F64)mTimeElapsed / (F64)sTotalTimeElapsed) * 100.f, ms) << LL_ENDL;
    stats["time"] = seconds;
    stats["binds"] = LLSD::Integer(mBinds);
    // sample counters are 64-bit; store as Real to avoid S32 overflow
    stats["samples"] = F64(mSamplesDrawn);
    stats["triangles"] = LLSD::Integer(mTrianglesDrawn);
}

bool LLGLSLShader::isTessellated() const
{
    return std::any_of(mShaderFiles.begin(), mShaderFiles.end(), [](const auto& file)
                       { return file.second == GL_TESS_CONTROL_SHADER || file.second == GL_TESS_EVALUATION_SHADER; });
}

//static
void LLGLSLShader::abandonProfile()
{
    endProfileQuery();
    profile_records().clear();
    reserve_profile_targets(false);
    sProfileEnabled = false;
}

void LLGLSLShader::placeProfileQuery()
{
    // A profile is the main thread's: a worker's binds are on another context, with its own gGL and pool.
    if (!sProfileEnabled || !on_main_thread())
    {
        return;
    }
    endProfileQuery();
    open_profile_record(*this, true);
}

//static
void LLGLSLShader::endProfileQuery()
{
    if (sOpenRecord == NO_RECORD || !on_main_thread())
    {
        return;
    }
    ProfileRecord& record = profile_records()[sOpenRecord];
    sOpenRecord = NO_RECORD;
    ALQueryPool& queries = gGL.queries();
    queries.endSpan(record.mTime);
    queries.end(record.mStatistics);
    queries.end(record.mSamples);
    queries.end(record.mPrimitives);
}

//static
LLGLSLShader* LLGLSLShader::suspendProfileQuery()
{
    if (sOpenRecord == NO_RECORD || !on_main_thread())
    {
        return nullptr;
    }
    LLGLSLShader* shader = profile_records()[sOpenRecord].mShader;
    endProfileQuery();
    return shader;
}

//static
void LLGLSLShader::resumeProfileQuery(LLGLSLShader* shader)
{
    if (shader && sProfileEnabled && sOpenRecord == NO_RECORD && on_main_thread())
    {
        open_profile_record(*shader, false);
    }
}



LLGLSLShader::LLGLSLShader()
    : mAttributeMask(0), mShaderLevel(0), mShaderGroup(SG_DEFAULT)
{
}

LLGLSLShader::~LLGLSLShader()
{
    // Leave the registry, which needs no GL context. unloadInternal() is the usual way out of
    // it, but a program can be destroyed without ever being unloaded -- and this one is about
    // to delete its children the same way -- so every pointer sInstances holds would otherwise
    // dangle. It is walked for name uniqueness, so a stale entry is a use-after-free rather
    // than a leak. Recursion covers the subtree: each child erases itself on the way out.
    sInstances.erase(this);
    sConfigured.erase(this);
    forget_profile_records(this);

    // Nor may the bound-program statics name this object. They are cleared without a GL call,
    // for the reason below.
    if (sCurBoundShaderPtr == this)
    {
        sCurBoundShaderPtr = nullptr;
        sCurBoundShader = 0;
    }

    // Global pass identities may be destroyed after render infrastructure.
    // Release CPU-owned variant objects without flushing or issuing GPU work.
    for (LLGLSLShader** v : { &mRiggedVariant, &mClassicVariant, &mMirrorVariant })
    {
        if (*v && *v != this && (*v)->mOwnedVariant)
        {
            delete *v;
        }
        *v = nullptr;
    }
}

// static
// Invalidate the shared environment (sky/water) uniform set for EVERY program: each re-applies it
// on its next bind, when it notices its own generation is behind. Bumping one counter keeps this
// O(1) instead of walking every live program every frame, and it reaches programs that no list
// happens to hold -- the indexed writers, avatar rigid and the deferred lighting programs were
// never in mShaderList, so they never saw a change of sky at all. A program with no environment
// uniforms resolves them to -1 and skips, so re-applying is cheap.
void LLGLSLShader::dirtyEnvironmentUniforms()
{
    ++sEnvironmentGeneration;
}

void LLGLSLShader::freeVariant(LLGLSLShader*& variant)
{
    if (variant)
    {
        // Two things must never be freed through:
        //  - a SELF-edge: a skinned program is its own rigged variant (see createShader), so
        //    freeing here would recurse forever and delete this mid-unload;
        //  - a pointer aimed at a global rather than a createShader() allocation, which would
        //    free a static object (mOwnedVariant marks the ones we allocated).
        if (variant != this && variant->mOwnedVariant)
        {
            variant->unload();
            delete variant;
        }
        variant = nullptr;
    }
}

void LLGLSLShader::freeOwnedVariants()
{
    freeVariant(mRiggedVariant);
    freeVariant(mClassicVariant);
    freeVariant(mMirrorVariant);
}

void LLGLSLShader::configureVariantClone(LLGLSLShader& dst, const std::string& name) const
{
    dst.mName        = name;
    dst.mFeatures    = mFeatures;
    dst.mDefines     = mDefines;    // NOTE: must come before the caller's addPermutation()s
    dst.mShaderFiles = mShaderFiles;
    dst.mShaderLevel = mShaderLevel;
    dst.mShaderGroup = mShaderGroup;
    dst.mNativeAttributeMask = mNativeAttributeMask;
}

// Build one corner: this program's config plus the requested defines, compiled. Deriving every
// corner from this program (rather than from a hand-configured sibling) is what makes
// construction independent of the order a caller happens to build its programs in, and it puts
// the axis defines after every addPermutation() the caller made -- the ordering that hand-wiring
// got wrong whenever a define was added after the clone was configured.
LLGLSLShader* LLGLSLShader::makeVariantCorner(const std::string& name, const char* perm_key,
                                             bool add_classic, bool add_rigged) const
{
    LLGLSLShader* corner = new LLGLSLShader();
    corner->mOwnedVariant = true;   // parent frees it in unload(); see freeVariant()
    configureVariantClone(*corner, name);

    if (perm_key)
    {
        corner->addPermutation(perm_key, "1");
    }
    if (add_classic)
    {
        corner->addPermutation("CLASSIC_MODE", "1");
    }
    if (add_rigged)
    {   // HAS_SKIN=1 selects the skinned path in the vertex source; hasObjectSkinning is the
        // matching feature flag, which is what attaches the objectSkin module.
        corner->addPermutation("HAS_SKIN", "1");
        corner->mFeatures.hasObjectSkinning = true;
    }

    if (corner->createShader())
    {
        return corner;
    }
    delete corner;
    return nullptr;
}

// Build `axis`'s corner set into the matching member. RIGGED is the innermost axis: one corner,
// nothing to compose over. Each per-PASS axis gets one corner per rigged x classic combination
// already present, so a program asking for all three ends up with every corner reachable by
// selectVariant()->bind(rigged). On any corner failure the partial subtree is dropped so the
// base is used.
bool LLGLSLShader::createVariant(EVariant axis)
{
    LLGLSLShader* LLGLSLShader::* member =
        (axis == VARIANT_RIGGED)  ? &LLGLSLShader::mRiggedVariant  :
        (axis == VARIANT_CLASSIC) ? &LLGLSLShader::mClassicVariant :
                                    &LLGLSLShader::mMirrorVariant;
    freeVariant(this->*member);

    if (axis == VARIANT_RIGGED)
    {
        mRiggedVariant = makeVariantCorner(llformat("Skinned %s", mName.c_str()), nullptr, false, true);
        return mRiggedVariant != nullptr;
    }

    const char* perm   = (axis == VARIANT_CLASSIC) ? "CLASSIC_MODE" : "MIRROR_CLIP";
    const char* suffix = (axis == VARIANT_CLASSIC) ? "(Classic)"    : "(Mirror)";

    // classic never composes over itself (the member was just cleared, and classic is built
    // before mirror), so a classic sibling only exists when the axis being added is mirror.
    const bool has_rigged  = mRiggedVariant  != nullptr;
    const bool has_classic = mClassicVariant != nullptr;

    LLGLSLShader* v = makeVariantCorner(llformat("%s %s", mName.c_str(), suffix), perm, false, false);
    if (!v)
    {
        return false;
    }

    bool ok = true;
    if (has_rigged)
    {
        v->mRiggedVariant = makeVariantCorner(llformat("Skinned %s %s", mName.c_str(), suffix), perm, false, true);
        ok = ok && v->mRiggedVariant != nullptr;
    }
    if (ok && has_classic)
    {
        v->mClassicVariant = makeVariantCorner(llformat("%s (Classic) %s", mName.c_str(), suffix), perm, true, false);
        ok = ok && v->mClassicVariant != nullptr;

        if (ok && mClassicVariant->mRiggedVariant)
        {
            v->mClassicVariant->mRiggedVariant =
                makeVariantCorner(llformat("Skinned %s (Classic) %s", mName.c_str(), suffix), perm, true, true);
            ok = ok && v->mClassicVariant->mRiggedVariant != nullptr;
        }
    }

    if (!ok)
    {   // drop the partial subtree; selectVariant() falls back to this program
        freeVariant(v);
        return false;
    }

    this->*member = v;
    return true;
}

void LLGLSLShader::unload()
{
    freeOwnedVariants();

    unloadInternal();

    // After unloadInternal: unbinding a bound program ends its profile queries by the
    // features they were begun with.
    clearConfiguration();
}

void LLGLSLShader::clearConfiguration()
{
    mShaderFiles.clear();
    mDefines.clear();
    mPermutationsAdded = false;
    mFeatures = LLShaderFeatures();
    mNativeAttributeMask = 0;
}

void LLGLSLShader::unloadInternal()
{
    sInstances.erase(this);
    if (sCurBoundShaderPtr == this)
    {
        unbind();
    }
    forget_profile_records(this);
    mAttributeMask = 0;
    mMinimumAlpha = MINIMUM_ALPHA_UNSET;
    mIsCompute = false;
    mNativeConfigured = false;
    mLinkedHash.setNull();
    mLinkedVariants = 0;
}

bool LLGLSLShader::createShader(U32 variants)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_SHADER;

    sConfigured.insert(this);
    mLoadGeneration = sLoadGeneration;

    // Keep an unchanged native pass and its complete variant graph.
    const LLUUID configured = hash();
    if (mNativeConfigured &&
        configured == mLinkedHash && variants == mLinkedVariants)
    {
        mShaderLevel       = mLinkedLevel;
        mPermutationsAdded = false;
        forEachVariant([](LLGLSLShader&) { ++sKept; });
        return true;
    }

    // Loaders describe semantic pass identities. Executable shaders and
    // pipeline/binding layouts belong to the native Metal owners.
    if (!configureNative(variants))
    {
        return false;
    }
    mLinkedHash     = configured;
    mLinkedVariants = variants;
    return true;
}

bool LLGLSLShader::configureNative(U32 variants)
{
    freeOwnedVariants();
    unloadInternal();

    sInstances.insert(this);
    mLightHash = 0xFFFFFFFF;
    mLinearDiffuse = mDefines.contains("LINEAR_DIFFUSE");
    mAttributeMask = mNativeAttributeMask;

    // A loader entry without a pass definition is incomplete.
    // Paths are retained as semantic source provenance only;
    // no GLSL file is opened or compiled here.
    if (mShaderFiles.empty())
    {
        unloadInternal();
        return false;
    }

    mIsCompute = std::any_of(
        mShaderFiles.begin(), mShaderFiles.end(),
        [](const auto& file) { return file.second == GL_COMPUTE_SHADER; });
    if (mIsCompute && !std::all_of(
            mShaderFiles.begin(), mShaderFiles.end(),
            [](const auto& file) { return file.second == GL_COMPUTE_SHADER; }))
    {
        unloadInternal();
        return false;
    }

    mShaderHash = hash();
    mPermutationsAdded = false;
    mNativeConfigured = true;
    mLinkedLevel = mShaderLevel;
    mUnavailableWarned = false;
    ++mLinkCount;
    ++sLinked;

    // Preserve the exact loader-authored variant graph. Native encoders use
    // these objects as pass identities even though none owns a GL name.
    if (mFeatures.hasObjectSkinning)
    {
        mRiggedVariant = this;
    }
    bool success = true;
    for (EVariant axis : { VARIANT_RIGGED, VARIANT_CLASSIC, VARIANT_MIRROR })
    {
        if (success && (variants & axis))
        {
            success = createVariant(axis);
        }
    }
    if (!success)
    {
        freeOwnedVariants();
        unloadInternal();
    }
    return success;
}

void LLGLSLShader::clearPermutations()
{
    // Clearing permutations the caller just added silently drops them: the define never reaches
    // the compiled program, and anything derived from it (mLinearDiffuse) reads false. Three
    // programs lost LINEAR_DIFFUSE exactly this way. Configuration must clear first, then add.
    if (mPermutationsAdded)
    {
        LL_WARNS("Shader") << "clearPermutations() on " << mName
                           << " discarded permutations added since the last createShader()" << LL_ENDL;
    }
    mDefines.clear();
    mPermutationsAdded = false;
}

void LLGLSLShader::addPermutation(std::string name, std::string value)
{
    mDefines[name] = value;
    mPermutationsAdded = true;
}

void LLGLSLShader::addConstant(const LLGLSLShader::eShaderConsts shader_const)
{
    addPermutation(gShaderConstsKey[shader_const], gShaderConstsVal[shader_const]);
}

void LLGLSLShader::removePermutation(std::string name)
{
    mDefines.erase(name);
}

bool LLGLSLShader::hasUniform(U32) const
{
    // Legacy uniform calls are not native resource declarations. Native pass
    // owners bind their explicit Metal constant layouts directly.
    return false;
}

void LLGLSLShader::bindUnavailable()
{
    gGL.flush();
    sCurBoundShader = 0;
    sCurBoundShaderPtr = this;
    if (!mUnavailableWarned)
    {
        LL_WARNS("Shader") << mName << " is not configured for native rendering" << LL_ENDL;
        mUnavailableWarned = true;
    }
}

void LLGLSLShader::bind()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_SHADER;
    if (!mNativeConfigured)
    {
        bindUnavailable();
        return;
    }
    gGL.flush();
    // This bind labels retained CPU traversal. Native encoders execute later
    // and do not consume ALQueryPool shader-profile tokens, so opening a GPU
    // profile here would publish zero counters as if they measured this pass.
    sCurBoundShaderPtr = this;
    sCurBoundShader = 0;
    warnIfVariantMissed();
}

// A per-PASS axis only applies if the caller routed through selectVariant() before binding.
// Holding a variant for an axis that is ACTIVE means this program is the base and its corner was
// never selected -- the pass silently runs at the wrong gamma, or stops clipping. Both are
// invisible in a normal frame, so this names the program instead of leaving it to be noticed. A
// corner has no variant of its own axis, so it passes. Gated on gDebugGL rather than SHOW_ASSERT:
// the builds that ship are the ones where a missed site would go unseen.
void LLGLSLShader::warnIfVariantMissed() const
{
    if (!gDebugGL) [[likely]]
    {
        return;
    }

    if (LLRender::sMirrorPass && mMirrorVariant)
    {
        LL_WARNS("Shader") << mName << " bound during the mirror pass without selectVariant()" << LL_ENDL;
    }
    if (LLRender::sClassicMode && mClassicVariant)
    {
        LL_WARNS("Shader") << mName << " bound under classic lighting without selectVariant()" << LL_ENDL;
    }
}

void LLGLSLShader::bind(bool rigged)
{
    if (rigged && !mRiggedVariant)
    {
        // This program did not build, or the skinned variant asked of it did not: a skinned draw draws nothing.
        bindUnavailable();
    }
    else if (rigged)
    {
        // Checked on THIS program, not on the corner about to be bound. A rigged corner carries
        // no per-pass variants of its own, so the check inside its bind() can never fire -- which
        // would let base.bind(true) skip a classic or mirror corner silently, the one route the
        // check would otherwise miss entirely.
        warnIfVariantMissed();
        mRiggedVariant->bind();
    }
    else
    {
        bind();
    }
}

void LLGLSLShader::unbind()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_SHADER;
    gGL.flush();
    sCurBoundShader = 0;
    sCurBoundShaderPtr = nullptr;
}

void LLGLSLShader::setMinimumAlpha(F32 minimum)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_SHADER;
#if LL_DARWIN
    if (gGL.isMetalUITextureCaptureAuthoritative())
    {
        gGL.setMetalUITextureCaptureMinimumAlpha(this, minimum);
        return;
    }
#endif
    if (mMinimumAlpha == minimum)
    {
        // Unchanged: skip the immediate-mode flush (which would pointlessly split any
        // pending batch) and the whole uniform routing below. mMinimumAlpha is the
        // single authority because every writer of MINIMUM_ALPHA comes through here
        // (see its declaration).
        return;
    }
    gGL.flush();
    mMinimumAlpha = minimum;
}

void LLShaderUniforms::apply(LLGLSLShader* shader)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_SHADER;
    for (auto& uniform : mIntegers)
    {
        shader->uniform1i(uniform.mUniform, uniform.mValue);
    }

    for (auto& uniform : mFloats)
    {
        shader->uniform1f(uniform.mUniform, uniform.mValue);
    }

    for (auto& uniform : mVectors)
    {
        shader->uniform4fv(uniform.mUniform, 1, uniform.mValue.mV);
    }

    for (auto& uniform : mVector3s)
    {
        shader->uniform3fv(uniform.mUniform, 1, uniform.mValue.mV);
    }
}

LLUUID LLGLSLShader::hash()
{
    HBXXH128 hash_obj;
    hash_obj.update(mName);
    hash_obj.update(&mShaderGroup, sizeof(mShaderGroup));
    hash_obj.update(&mShaderLevel, sizeof(mShaderLevel));
    for (const auto& shdr_pair : mShaderFiles)
    {
        hash_obj.update(shdr_pair.first);
        hash_obj.update(&shdr_pair.second, sizeof(GLenum));
    }
    for (const auto& define_pair : mDefines)
    {
        hash_obj.update(define_pair.first);
        hash_obj.update(define_pair.second);

    }
    for (const auto& define_pair : LLGLSLShader::sGlobalDefines)
    {
        hash_obj.update(define_pair.first);
        hash_obj.update(define_pair.second);

    }
    // Pass provenance includes the depth convention and normal attachment
    // layout, independently of the loader's explicit defines.
    hash_obj.update(&LLRender::sReverseZ, sizeof(LLRender::sReverseZ));
    hash_obj.update(&LLRender::sGBufferNormHDR, sizeof(LLRender::sGBufferNormHDR));
    // Shared native shader inputs may change without a local define change.
    hash_obj.update(sSharedObjectsKey.mData, UUID_BYTES);
    hash_obj.update(&mFeatures, sizeof(LLShaderFeatures));
    // Native attribute declarations replace GLSL reflection and therefore
    // participate in the semantic configuration cache key.
    hash_obj.update(&mNativeAttributeMask, sizeof(mNativeAttributeMask));
    // The selected native device participates in semantic configuration reuse.
    hash_obj.update(gRenderCapabilities.identity());
    return hash_obj.digest();
}
