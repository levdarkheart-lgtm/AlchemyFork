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

bool LLGLSLShader::isTessellated() const
{
    return std::any_of(mShaderFiles.begin(), mShaderFiles.end(), [](const auto& file)
                       { return file.second == GL_TESS_CONTROL_SHADER || file.second == GL_TESS_EVALUATION_SHADER; });
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
    // This bind labels retained CPU traversal. Native encoders execute later.
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
