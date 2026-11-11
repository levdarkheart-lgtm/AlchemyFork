/**
 * @file llglslshader.h
 * @brief Native renderer pass identities and retained traversal metadata.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#ifndef LL_LLGLSLSHADER_H
#define LL_LLGLSLSHADER_H

#include "llgl.h"
#include "llrender.h"
#include "llstaticstringtable.h"

#include <type_traits>

class LLShaderFeatures
{
public:
    S32 mIndexedTextureChannels = 0;
    // The least #version this program's own stage files compile at (430 for a stage that reads storage buffers or
    // images); 0 the context's usual. A compute stage always compiles at 430. The shared objects a program attaches
    // keep their own version: GL links stages of different versions.
    U32 mMinGLSLVersion = 0;
    bool calculatesLighting = false;
    bool calculatesAtmospherics = false;
    bool hasLighting = false; // implies no transport (it's possible to have neither though)
    bool isAlphaLighting = false; // indicates lighting shaders need not be linked in (lighting performed directly in alpha shader to match deferred lighting functions)
    bool isSpecular = false;
    bool hasTransport = false; // implies no lighting (it's possible to have neither though)
    bool hasSkinning = false;
    bool hasObjectSkinning = false;
    bool mGLTF = false;
    bool hasAtmospherics = false;
    bool hasGamma = false;
    bool mHashLayoutByte = false; // explicit initialized byte; prevents tail padding in the byte-hashed record
    bool hasAmbientOcclusion = false;
    bool hasSrgb = false;
    bool isDeferred = false;
    bool hasFullGBuffer = false;
    bool hasScreenSpaceReflections = false;
    bool hasAlphaMask = false;
    bool hasReflectionProbes = false;
    bool attachNothing = false;
    bool hasHeroProbes = false;
    bool isPBRTerrain = false;
    bool hasTessellatedTerrain = false; // terrainSurface for the evaluation and fragment stages, and the atmospherics interface the evaluation stage owes the fragment stage
    bool hasWaterSurface = false; // waterSurface for the vertex and fragment stages: the water's field, heights and wave coordinates
    bool hasWaterWaves = false; // waterSurface and the lattice for the evaluation stage too: the waves' displacement
    bool hasTonemap = false;
    bool hasColorGrade = false;
    bool hasPostEffects = false;
};
// LLGLSLShader::hash() reads these bytes, so every byte must be a member's: padding has no value a copy need keep, and
// would make a program's binary-cache key differ from run to run. A flag that would leave some needs a filler beside it.
static_assert(std::has_unique_object_representations_v<LLShaderFeatures>, "LLShaderFeatures is hashed by its bytes");

// ============= Structure for caching shader uniforms ===============
class LLGLSLShader;

class LLShaderUniforms
{
public:

    template<typename T>
    struct UniformSetting
    {
        S32 mUniform{ 0 };
        T mValue{};
    };

    typedef UniformSetting<S32> IntSetting;
    typedef UniformSetting<F32> FloatSetting;
    typedef UniformSetting<LLVector4> VectorSetting;
    typedef UniformSetting<LLVector3> Vector3Setting;

    void clear()
    {
        mIntegers.resize(0);
        mFloats.resize(0);
        mVectors.resize(0);
        mVector3s.resize(0);
    }

    void uniform1i(S32 index, S32 value)
    {
        mIntegers.push_back({ index, value });
    }

    void uniform1f(S32 index, F32 value)
    {
        mFloats.push_back({ index, value });
    }

    void uniform4fv(S32 index, const LLVector4& value)
    {
        mVectors.push_back({ index, value });
    }

    void uniform4fv(S32 index, const F32* value)
    {
        mVectors.push_back({ index, LLVector4(value) });
    }

    void uniform3fv(S32 index, const LLVector3& value)
    {
        mVector3s.push_back({ index, value });
    }

    void uniform3fv(S32 index, const F32* value)
    {
        mVector3s.push_back({ index, LLVector3(value) });
    }

    void apply(LLGLSLShader* shader);


    std::vector<IntSetting> mIntegers;
    std::vector<FloatSetting> mFloats;
    std::vector<VectorSetting> mVectors;
    std::vector<Vector3Setting> mVector3s;
};
class LLGLSLShader
{
public:
    // NOTE: Keep gShaderConsts and LLGLSLShader::ShaderConsts_e in sync!
    enum eShaderConsts
    {
        SHADER_CONST_CLOUD_MOON_DEPTH
        , SHADER_CONST_STAR_DEPTH
        , NUM_SHADER_CONSTS
    };

    // enum primarily used to control application sky settings uniforms
    typedef enum
    {
        SG_DEFAULT = 0,  // not sky or water specific
        SG_SKY,  //
        SG_WATER,
        SG_ANY,
        SG_COUNT
    } eGroup;

    // Shared CPU constant-block indices, retained for their producer boundaries.
    // Native encoders bind their Metal buffers explicitly.
    enum UniformBlock : GLuint
    {
        UB_REFLECTION_PROBES,   // "ReflectionProbes"
        UB_GLTF_JOINTS,         // "GLTFJoints"
        UB_GLTF_NODES,          // "GLTFNodes"
        UB_GLTF_MATERIALS,      // "GLTFMaterials"
        UB_ENVIRONMENT,         // "Environment" (sky/water constants)
        UB_DEFERRED,            // "Deferred" (SSAO/specular constants)
        UB_LIGHTS,              // "Lights" (forward fixed-function light array state)
        UB_MATRICES,            // "Matrices" (modelview/projection/derived matrix stack state)
        NUM_UNIFORM_BLOCKS
    };

    // Producer-side block schema names, in UniformBlock enum order.
    static constexpr const char* UNIFORM_BLOCK_NAMES[NUM_UNIFORM_BLOCKS] =
    {
        "ReflectionProbes", // UB_REFLECTION_PROBES
        "GLTFJoints",       // UB_GLTF_JOINTS
        "GLTFNodes",        // UB_GLTF_NODES
        "GLTFMaterials",    // UB_GLTF_MATERIALS
        "Environment",      // UB_ENVIRONMENT
        "Deferred",         // UB_DEFERRED
        "Lights",           // UB_LIGHTS
        "Matrices",         // UB_MATRICES
    };

    // Every program that created successfully. Walked for name uniqueness and by unloadShaders();
    // entries are removed by unloadInternal() and by the destructor.
    //
    // A reference to a deliberately leaked set rather than a set. Programs are non-local statics
    // in other translation units and leave this registry from their destructors, and destruction
    // order across translation units is unspecified -- a registry that destroyed itself could be
    // gone before the last program left it. One empty set at process exit is the cheaper half of
    // that trade. Entries arrive only when createShader() configures a native pass.
    static std::set<LLGLSLShader*>& sInstances;
    // Every program createShader has been asked to build, linked or not, until it is destroyed: what a load configures
    // from nothing (clearConfiguration). Leaked for the reason sInstances is.
    static std::set<LLGLSLShader*>& sConfigured;

    LLGLSLShader();
    ~LLGLSLShader();

    // Compatibility state for LLRender's saved client traversal. Native pass
    // identity is the pointer; there is no GPU program name.
    static U32 sCurBoundShader;
    static LLGLSLShader* sCurBoundShaderPtr;
    static S32 sIndexedTextureChannels;

    // Hard ceiling on the indexed-GLTF slot count. setShaders() clamps
    // sIndexedGLTFChannels to this, and every fixed per-slot array that walks a batch is
    // sized from it. Those two things MUST agree: the arrays are indexed by slot up to
    // sIndexedGLTFChannels, so a clamp raised past the array width overruns them. One
    // constant so raising the ceiling moves both together instead of relying on matching
    // literals in four files.
    static constexpr S32 MAX_INDEXED_GLTF_CHANNELS = 8;

    // Hard ceiling on the indexed-texture ladder width (tex0..texN). Three things must
    // agree on this number: the tex%d samplers the indexed sources declare, the
    // texture_list[] genDrawInfo batches into, and sIndexedTextureChannels. It is a
    // declaration ceiling, not a budget -- the affordable width is a device+shader
    // property computed in setShaders().
    static constexpr S32 MAX_BATCH_TEXTURE_COUNT = 32;

    // Number of GLTF PBR materials that can be batched into one indexed draw call.
    // Each material consumes four texture units (base color, normal, ORM, emissive),
    // so this is roughly (available fragment texture units) / 4, clamped to
    // MAX_INDEXED_GLTF_CHANNELS. See PASS_GLTF_PBR_INDEXED and
    // LLVolumeGeometryManager::genDrawInfo.
    static S32 sIndexedGLTFChannels;

    // True once the indexed legacy (Blinn-Phong) material programs have loaded.
    // Gates indexed POOL_MATERIALS batching (shares sIndexedGLTFChannels for the
    // per-slot stride); independent so a material-shader failure doesn't disable PBR.
    static bool sIndexedLegacyMaterials;

    static U32 sMaxGLTFMaterials;
    static U32 sMaxGLTFNodes;

    void unload();

    // Compile-time variant axes, passed to createShader() as a mask. They are built in this
    // order because each composes over the ones before it: a program asking for all three gets
    // the full rigged x classic x mirror corner set, every corner reachable via
    // selectVariant()->bind(rigged). Each corner is derived from this program's config plus the
    // axis defines, so nothing depends on a sibling program being compiled first.
    enum EVariant : U32
    {
        VARIANT_RIGGED  = 1 << 0, // HAS_SKIN=1     -- skinned/animesh  (per-DRAW, via bind(rigged))
        VARIANT_CLASSIC = 1 << 1, // CLASSIC_MODE=1 -- classic sky lighting     (per-PASS)
        VARIANT_MIRROR  = 1 << 2, // MIRROR_CLIP=1  -- hero-probe mirror clip   (per-PASS)
    };

    // Configure this native pass and its requested variant subtrees, owned by
    // this identity and freed by unload(). An unchanged hash/axis set retains
    // the existing graph. A failed pass or corner retires the complete graph.
    bool createShader(U32 variants = 0);

    // Loaders that feed CPU-authored geometry declare the reserved attributes
    // consumed by their native pipelines; no GLSL reflection exists.
    void setNativeAttributeMask(U32 attribute_mask) { mNativeAttributeMask = attribute_mask; }

    // Clear loader configuration while preserving the current native pass
    // until createShader decides whether its configuration can be reused.
    void clearConfiguration();

    // How many times this program has linked since it was constructed, for what derives from what it
    // draws.
    U32 linkCount() const { return mLinkCount; }

    // The load running (LLViewerShaderMgr::setShaders), which every program its loaders configure is
    // stamped with by createShader: a program not stamped by a load was not asked for by it.
    static U32 sLoadGeneration;
    U32 mLoadGeneration = 0;

    // The programs linked, and those kept linked as they were, since the counts were last cleared,
    // each variant counted.
    static U32 sLinked;
    static U32 sKept;

    // Digest of shared native shader inputs, set by the viewer loader.
    static LLUUID sSharedObjectsKey;
    // Transitional calls from retained traversal. Native encoders consume
    // explicit pass constants (and LLShaderUniforms CPU records), never GL
    // default-block uniforms. These setters already performed no work on the
    // native path; remove the legacy callers with their displaced passes.
    void uniform1i(U32, GLint) {}
    void uniform1f(U32, GLfloat) {}
    void uniform1ui(U32, GLuint) {}
    void uniform2f(U32, GLfloat, GLfloat) {}
    void uniform3f(U32, GLfloat, GLfloat, GLfloat) {}
    void uniform4f(U32, GLfloat, GLfloat, GLfloat, GLfloat) {}
    void uniform1iv(U32, U32, const GLint*) {}
    void uniform4iv(U32, U32, const GLint*) {}
    void uniform1fv(U32, U32, const GLfloat*) {}
    void uniform2fv(U32, U32, const GLfloat*) {}
    void uniform3fv(U32, U32, const GLfloat*) {}
    void uniform4fv(U32, U32, const GLfloat*) {}
    void uniform4uiv(U32, U32, const GLuint*) {}
    void uniformMatrix2fv(U32, U32, GLboolean, const GLfloat*) {}
    void uniformMatrix3fv(U32, U32, GLboolean, const GLfloat*) {}
    void uniformMatrix3x4fv(U32, U32, GLboolean, const GLfloat*) {}
    void uniformMatrix4fv(U32, U32, GLboolean, const GLfloat*) {}
    void uniformMatrix4fv(U32, const LLMatrix4a&) {}
    void uniformMatrix3fv(U32, const LLMatrix3a&) {}
    void uniform1i(const LLStaticHashedString&, GLint) {}

    void setMinimumAlpha(F32 minimum);
    F32 getMinimumAlpha() const { return mMinimumAlpha; }

    GLint getUniformLocation(const LLStaticHashedString&) const { return -1; }
    GLint getUniformLocation(U32) const { return -1; }
    bool hasUniform(U32 index) const;

    void clearPermutations();
    void addPermutation(std::string name, std::string value);
    void addPermutations(const std::map<std::string, std::string>& defines)
    {
        mDefines.insert(defines.begin(), defines.end());
        mPermutationsAdded = true;
    }
    void removePermutation(std::string name);

    void addConstant(const LLGLSLShader::eShaderConsts shader_const);

    // Legacy sampler calls do not invent Metal resource bindings. Native
    // encoders own their explicit textures/samplers; retained calls previously
    // resolved to -1 because no GL reflection was performed.
    S32 enableTexture(S32) { return -1; }
    S32 disableTexture(S32, ALTextureSlot::eTextureType = ALTextureSlot::TT_TEXTURE) { return -1; }
    S32 getTextureChannel(S32) const { return -1; }
    S32 bindTexture(S32, LLTexture*, ALSampler) { return -1; }
    S32 bindTexture(S32, LLRenderTarget*, ALSampler = ALSamplers::TargetDefault, U32 = 0) { return -1; }
    S32 bindDepthTexture(S32, LLRenderTarget*, ALSampler = ALSamplers::PointClamp) { return -1; }
    S32 unbindTexture(S32) { return -1; }

    bool isCompute() const { return mIsCompute; }
    bool isTessellated() const;

    void bind();
    //helper to conditionally bind mRiggedVariant instead of this
    void bind(bool rigged);

    // Availability describes native semantic pass configuration. The legacy
    // completion gate remains false until its displaced callers are removed;
    // it must not activate the old post-processing execution paths.
    bool isComplete() const { return false; }
    bool isNativeConfigured() const { return mNativeConfigured; }
    bool isAvailable() const { return mNativeConfigured; }

    LLUUID hash();

    // Unbinds any previously bound shader by explicitly binding no shader.
    static void unbind();

    U32 mLightHash = 0xFFFFFFFF;

    U32 mAttributeMask;  //mask of which reserved attributes are set (lines up with LLVertexBuffer::getTypeMask())
    // Last value routed to MINIMUM_ALPHA on this program, so setMinimumAlpha can skip the
    // batch flush and the whole uniform path when unchanged (the alpha pool re-applies it
    // per batch; GLTF material binds re-push it per material). EVERY writer of that
    // uniform must go through setMinimumAlpha to keep this authoritative -- do not write
    // it via uniform1f directly. -1 is a live value (GLTF "no masking"), so the
    // unset sentinel sits outside the usable range and resets on reconfiguration.
    static constexpr F32 MINIMUM_ALPHA_UNSET = -1e30f;
    F32 mMinimumAlpha = MINIMUM_ALPHA_UNSET;

    S32 mShaderLevel;
    S32 mShaderGroup; // see LLGLSLShader::eGroup
    // environment-uniform generation this program last applied; see sEnvironmentGeneration.
    // A program starts one behind it when it links, so its first bind applies the set.
    U32 mEnvUniformsGeneration = 1;
    LLShaderFeatures mFeatures;
    std::vector< std::pair< std::string, GLenum > > mShaderFiles;
    std::string mName;
    typedef std::map<std::string, std::string> defines_map_t; //NOTE: this must be an ordered map to maintain hash consistency
    defines_map_t mDefines;
    // set by addPermutation(s) and cleared by createShader()/unload(); see clearPermutations()
    bool mPermutationsAdded = false;
    static defines_map_t sGlobalDefines;
    LLUUID mShaderHash;

    // This program expects its diffuse/base-colour texture already LINEAR: the bind decodes
    // sRGB on the sampler (ALSampler::SRGBDecode), the shader has dropped its own
    // srgb_to_linear on the diffuse, and its vertex-colour tint is linearised in the vertex
    // stage. Set on the forward writers that were converted to filter albedo in linear space
    // -- see the retained pool bind sites (bindIndexedTextures and pushBatch),
    // which read it to choose the diffuse sampler for a polymorphic per-draw shader. Left
    // false on the HUD programs (they output sRGB) and the FOR_IMPOSTOR alpha program (it
    // writes the sRGB sample straight to the bake).
    //
    // DERIVED in createShader() from the LINEAR_DIFFUSE permutation -- the same define the
    // shader source keys its colour-space guards on -- so engine and compiled code cannot
    // disagree. Add the permutation; never assign this.
    bool mLinearDiffuse = false;

    // Compile-time variant axes:
    //   mRiggedVariant  -- HAS_SKIN=1     (per-DRAW; chosen via bind(rigged))
    //   mClassicVariant -- CLASSIC_MODE=1 (per-PASS; classic sky lighting active)
    //   mMirrorVariant  -- MIRROR_CLIP=1  (per-PASS; hero-probe mirror clip active)
    // The per-pass variants carry their own rigged (and each other's) sub-variants, so a single
    // selectVariant() walk reaches any rigged x classic x mirror corner.
    //
    // OWNERSHIP: createShader()'s variant mask is the only thing that allocates one, and it
    // heap-allocates, so a program owns its whole variant subtree -- unload() frees it via
    // freeVariant(). mOwnedVariant records that; it exists because these members are public, so
    // pointing one at a global instead would make freeVariant() delete a static object. A
    // skinned program is its own rigged variant (a self-edge set by createShader), which
    // freeVariant() and forEachVariant() must not traverse.
    LLGLSLShader* mRiggedVariant = nullptr;
    LLGLSLShader* mClassicVariant = nullptr;
    LLGLSLShader* mMirrorVariant = nullptr;

    // The program a pass binds for skinned draws of this one: its rigged variant, or, where it has none, this program.
    // A program that did not build has none, and binding it binds the stand-in, which draws nothing, while the uniform
    // and texture calls the pass then makes set nothing.
    LLGLSLShader* riggedVariant() { return mRiggedVariant ? mRiggedVariant : this; }

    // true when this program was heap-allocated as a variant and is owned by its parent
    bool mOwnedVariant = false;

    // Release a variant reference held by this program: unload + delete it when this program
    // owns it, otherwise just clear the pointer. Idempotent, and recurses through the subtree
    // (see unload()). Never frees through a self-edge.
    void freeVariant(LLGLSLShader*& variant);

    // drop this program's whole owned variant subtree (see unload() / createShader())
    void freeOwnedVariants();

    // Apply `fn` to this program and every owned variant, depth-first. The subtree is a tree
    // (no corner is shared), so each program is visited exactly once -- use this for anything
    // that must reach every corner, e.g. post-createShader feature tweaks or sampler unit maps.
    // NOTE: skips a self-referencing child; recursing into that edge would never terminate.
    template <typename FUNC>
    void forEachVariant(FUNC fn)
    {
        fn(*this);
        if (mRiggedVariant  && mRiggedVariant  != this) { mRiggedVariant->forEachVariant(fn); }
        if (mClassicVariant && mClassicVariant != this) { mClassicVariant->forEachVariant(fn); }
        if (mMirrorVariant  && mMirrorVariant  != this) { mMirrorVariant->forEachVariant(fn); }
    }

    // Returns the variant to bind for the current global render-pass state, composing the
    // per-PASS compile-time axes in a fixed order: the MIRROR_CLIP clone during the mirror
    // pass, then the CLASSIC_MODE clone under classic sky lighting. An axis that is inactive
    // or has no variant is a no-op, so this is safe on every program (including those with no
    // variants at all). The per-DRAW rigged axis is applied afterward on the returned pointer
    // via bind(rigged) / ->mRiggedVariant. Call before bind(); route all subsequent
    // uniform*/bindTexture calls through the returned pointer.
    LLGLSLShader* selectVariant()
    {
        LLGLSLShader* s = this;
        if (LLRender::sMirrorPass  && s->mMirrorVariant)  s = s->mMirrorVariant;
        if (LLRender::sClassicMode && s->mClassicVariant) s = s->mClassicVariant;
        return s;
    }

    // Invalidate the shared environment (sky/water) uniform set for every live program; each
    // re-applies on its next bind -- see the definition
    static void dirtyEnvironmentUniforms();

    // Generation of the shared environment uniform set. LLEnvironment bumps it via
    // dirtyEnvironmentUniforms(); a program re-applies on bind when its own generation is
    // behind. A counter rather than a per-program dirty flag so the per-frame environment
    // update stays O(1) and reaches programs no list happens to hold. A program that links starts
    // distinct from the native pass/constant ownership.
    static U32 sEnvironmentGeneration;



    // hacky flag used for optimization in LLDrawPoolAlpha
    bool mCanBindFast = false;

private:
    void unloadInternal();

    // Metal keeps the loader-authored pass/variant graph without compiling
    // the GLSL files named by that graph. Native renderers own the actual MSL
    // functions, pipeline reflection and binding layout.
    bool configureNative(U32 variants);

    // Preserve the failed pass identity without leaving a prior valid one bound.
    void bindUnavailable();
    bool mUnavailableWarned = false;

    // What the program linked under, for createShader to keep it: its configuration's hash() as the
    // loader set it, its variants, and the level it linked at, which a fallback may have lowered.
    LLUUID mLinkedHash;
    U32    mLinkedVariants = 0;
    S32    mLinkedLevel    = 0;
    U32    mLinkCount      = 0;
    bool   mNativeConfigured = false;
    U32    mNativeAttributeMask = 0;

    bool mIsCompute = false;

    // ---- compile-time variant construction (see EVariant / createShader) ----
    // build one axis's corner set into the matching member; see the definition
    bool createVariant(EVariant axis);
    // heap-allocate one corner: this program's config plus the requested defines, compiled
    LLGLSLShader* makeVariantCorner(const std::string& name, const char* perm_key,
                                    bool add_classic, bool add_rigged) const;
    // copy this program's whole configuration into `dst` under `name` (no defines added)
    void configureVariantClone(LLGLSLShader& dst, const std::string& name) const;

    // gDebugGL-only: warn when an active per-pass axis had a corner this bind did not select
    void warnIfVariantMissed() const;
};

//UI shader (declared here so llui_libtest will link properly)
extern LLGLSLShader         gUIProgram;
//output vec4(color.rgb,color.a*tex0[tc0].a)
extern LLGLSLShader         gSolidColorProgram;
//Alpha mask shader (declared here so llappearance can access properly)
extern LLGLSLShader         gAlphaMaskProgram;

#endif
