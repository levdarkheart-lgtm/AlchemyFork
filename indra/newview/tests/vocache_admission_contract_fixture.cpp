/**
 * @file vocache_admission_contract_fixture.cpp
 * @brief Controlled boundaries for actual PPD475 cache/occlusion production bodies.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
// Frustum/projection/back-sphere tests accept one child group. Query results are
// supplied explicitly. Real scheduling, cache cull decisions, occlusion state
// propagation/check/issue bodies are inserted verbatim by the Python driver.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>
static bool capture_ok = true;
static int captured = 0;
using U32 = uint32_t;
using U64 = uint64_t;
using S32 = int32_t;
using F32 = float;
#define LL_DARWIN 1
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_OCTREE
#define LL_PROFILE_ZONE_NAMED_CATEGORY_OCTREE(x)
#define llassert(x) assert(x)
#define assert_states_valid(x) ((void)0)
template <class T> T llmax(T a, T b)
{
    return std::max(a, b);
}
template <class T> T llmin(T a, T b)
{
    return std::min(a, b);
}
void add(int &, int)
{
}
U32 gFrameCount = 10000;
struct LLVector3
{
    float mV[3]{};
    LLVector3() = default;
    LLVector3(float x, float y, float z) : mV{x, y, z}
    {
    }
    float operator[](int i) const
    {
        return mV[i];
    }
    bool isExactlyZero() const
    {
        return mV[0] == 0 && mV[1] == 0 && mV[2] == 0;
    }
    LLVector3 operator-(const LLVector3 &b) const
    {
        return {mV[0] - b.mV[0], mV[1] - b.mV[1], mV[2] - b.mV[2]};
    }
};
struct Bits
{
    int v;
    int getGatheredBits() const
    {
        return v;
    }
};
struct LLVector4a
{
    float v[4]{};
    LLVector4a() = default;
    LLVector4a(float x, float y, float z) : v{x, y, z, 0}
    {
    }
    const float *getF32ptr() const
    {
        return v;
    }
    float operator[](int i) const
    {
        return v[i];
    }
    void add(const LLVector4a &b)
    {
        for (int i = 0; i < 4; ++i)
            v[i] += b.v[i];
    }
    void splat(float a)
    {
        for (float &x : v)
            x = a;
    }
    void set(float a, float b, float c, float d)
    {
        v[0] = a;
        v[1] = b;
        v[2] = c;
        v[3] = d;
    }
    void setAdd(const LLVector4a &a, const LLVector4a &b)
    {
        for (int i = 0; i < 4; ++i)
            v[i] = a.v[i] + b.v[i];
    }
    void setSub(const LLVector4a &a, const LLVector4a &b)
    {
        for (int i = 0; i < 4; ++i)
            v[i] = a.v[i] - b.v[i];
    }
    void load3(const float *x)
    {
        for (int i = 0; i < 3; ++i)
            v[i] = x[i];
        v[3] = 0;
    }
    Bits lessThan(const LLVector4a &b) const
    {
        int m = 0;
        for (int i = 0; i < 4; ++i)
            if (v[i] < b.v[i])
                m |= 1 << i;
        return {m};
    }
    Bits greaterThan(const LLVector4a &b) const
    {
        int m = 0;
        for (int i = 0; i < 4; ++i)
            if (v[i] > b.v[i])
                m |= 1 << i;
        return {m};
    }
};
struct LLCamera
{
    LLVector3 pos{100, 0, 0};
    const LLVector3 &getOrigin() const
    {
        return pos;
    }
    int AABBInFrustumNoFarClip(const LLVector4a &, const LLVector4a &) const
    {
        return 2;
    }
    void calcRegionFrustumPlanes(LLVector3, float)
    {
    }
};
struct LLViewerCamera
{
    enum
    {
        CAMERA_WORLD = 0,
        NUM_CAMERAS = 2
    };
    static inline U32 sCurCameraID = 0;
};
struct LLDrawable
{
    static U32 getCurrentFrame()
    {
        return gFrameCount;
    }
};
struct LLViewerOctreeEntryData
{
    static U32 getCurrentFrame()
    {
        return gFrameCount;
    }
};
struct LLVOCacheEntry
{
    static inline float sNearRadius = 10, sRearFarRadius = 10;
    static inline U32 sMinFrameRange = 64;
    static float getSquaredPixelThreshold(bool)
    {
        return 0;
    }
};
struct CubeVB
{
    void drawRange(int, int, int, int, int)
    {
        ++captured;
    }
};
struct LLPipeline
{
    static bool isMainWorldPass()
    {
        return false;
    }
    std::shared_ptr<CubeVB> mCubeVB = std::make_shared<CubeVB>();
    static inline int sUseOcclusion = 2;
    static inline bool sShadowRender = false;
    bool captureMetalVisibilityQuery(auto &, auto, const LLVector4a &, const LLVector4a &, LLCamera *, bool);
};
struct LLViewerOctreeGroup;
struct Node
{
    std::vector<Node *> children;
    Node *parent{};
    U32 getChildCount() const
    {
        return children.size();
    }
    Node *getChild(U32 i) const
    {
        return children[i];
    }
    LLViewerOctreeGroup *listener{};
    Node *getParent() const
    {
        return parent;
    }
    LLViewerOctreeGroup *getListener(int) const
    {
        return listener;
    }
};
using OctreeNode = Node;
struct OctreeTraveler
{
    virtual void visit(const Node *)
    {
    }
    virtual void traverse(const Node *n)
    {
        visit(n);
        for (Node *c : n->children)
            traverse(c);
    }
};
struct LLViewerOctreeGroup
{
    Node node;
    Node *mOctreeNode = &node;
    U32 mVisible[2]{};
    LLVector4a ext[2]{};
    LLViewerOctreeGroup()
    {
        static Node root;
        node.parent = &root;
        node.listener = this;
    }
    Node *getOctreeNode()
    {
        return &node;
    }
    void rebound()
    {
    }
    void setVisible()
    {
        mVisible[LLViewerCamera::sCurCameraID] = gFrameCount;
    }
    const LLVector4a *getObjectExtents() const
    {
        return ext;
    }
    const LLVector4a *getExtents() const
    {
        return ext;
    }
};
struct ALQueryTicket
{
    bool valid = false;
    bool isValid() const
    {
        return valid;
    }
};
enum class ALQueryKind
{
    Occlusion,
    OcclusionExact
};
struct ALQueryPool
{
    static bool diagnosticsEnabled()
    {
        return false;
    }
    void recordMainWorldGroupTransition(bool)
    {
    }
    enum class Status
    {
        Pending,
        Ready,
        Expired
    };
    Status status = Status::Expired;
    U64 value = 0;
    bool allow = true;
    int polls = 0;
    Status result(ALQueryTicket &, U64 &v)
    {
        ++polls;
        v = value;
        return status;
    }
};
struct Render
{
    ALQueryPool pool;
    ALQueryPool &queries()
    {
        return pool;
    }
} gGL;
struct ALQueryScope
{
    ALQueryTicket t;
    ALQueryScope(ALQueryPool &p, ALQueryKind) : t{p.allow}
    {
    }
    const ALQueryTicket &ticket()
    {
        return t;
    }
};
struct LLViewerOctreePartition
{
    U32 mLODPeriod = 16;
    bool isOcclusionEnabled() const
    {
        return true;
    }
};
struct LLOcclusionCullingGroup : LLViewerOctreeGroup
{
    enum
    {
        OCCLUDED = 0x00010000,
        QUERY_PENDING = 0x00020000,
        ACTIVE_OCCLUSION = 0x00040000,
        DISCARD_QUERY = 0x00080000,
        STATE_MODE_SINGLE = 0,
        STATE_MODE_BRANCH = 1,
        STATE_MODE_DIFF = 2,
        STATE_MODE_ALL_CAMERAS = 3
    };
    LLViewerOctreePartition *mSpatialPartition{};
    U32 mLODHash = 0, mOcclusionIssued[2]{}, mOcclusionState[2]{};
    ALQueryTicket mOcclusionQuery[2]{};
    LLVector4a mBounds[2]{};
    static inline U32 sOcclusionForgotten[2]{};
    static inline int sOcclusionQueries = 0, sNumObjectsOccluded = 0, sNumObjectsUnoccluded = 0;
    LLOcclusionCullingGroup *parent = nullptr;
    LLOcclusionCullingGroup *getParent()
    {
        return parent;
    }
    bool isOcclusionState(U32 state) const
    {
        return mOcclusionState[LLViewerCamera::sCurCameraID] & state;
    }
    U32 getOcclusionState()
    {
        return mOcclusionState[LLViewerCamera::sCurCameraID];
    }
    void setOcclusionState(U32 state, S32 mode = 0);
    void clearOcclusionState(U32 state, S32 mode = 0);
    void discardStaleQueriesBelow();
    ALQueryKind queryKind() const
    {
        return ALQueryKind::OcclusionExact;
    }
    bool needsUpdate();
    bool isOcclusionRetestDue();
    bool isRecentlyVisible() const;
    bool earlyFail(LLCamera *, const LLVector4a *);
    bool checkOcclusion();
    void doOcclusion(LLCamera *, const LLVector4a *);
};
using LLVOCacheGroup = LLOcclusionCullingGroup;
struct LLViewerRegion
{
    static inline bool sVOCacheCullingEnabled = true;
    bool paused = false;
    int admissions = 0, frame_visible = 0;
    bool isPaused()
    {
        return paused;
    }
    static inline bool camera_static = true;
    static bool isViewerCameraStatic()
    {
        return camera_static;
    }
    LLVector3 getOriginAgent()
    {
        return {};
    }
    bool addVisibleGroup(LLViewerOctreeGroup *)
    {
        ++admissions;
        ++frame_visible;
        return true;
    }
    U32 getNumOfVisibleGroups()
    {
        return frame_visible;
    }
};
struct LLVOCachePartition : LLViewerOctreePartition
{
    LLViewerRegion *mRegionp;
    Node *mOctree;
    LLVOCacheGroup *group;
    U32 mCulledTime[2]{}, mCullHistory = 0, mIdleHash = 0;
    S32 mBackSlectionEnabled = -1;
    bool mFrontCull = false;
    std::set<LLVOCacheGroup *> mOccludedGroups;
    static inline bool sNeedsOcclusionCheck = false;
    LLVOCachePartition(LLViewerRegion *r, LLVOCacheGroup *g)
        : mRegionp(r), mOctree(g->getOctreeNode()), group(g)
    {
        mOctree->listener = g;
        g->mSpatialPartition = this;
    }
    void selectBackObjects(LLCamera &, F32, bool);
    S32 cull(LLCamera &, bool);
    void addOccluders(LLViewerOctreeGroup *);
    void processOccluders(LLCamera *);
};
struct LLViewerOctreeCull
{
    LLCamera *mCamera;
    bool mNearFirst = false;
    LLVector3 mLocalShift;
    LLViewerOctreeCull(LLCamera *c) : mCamera(c)
    {
    }
    virtual bool earlyFail(LLViewerOctreeGroup *)
    {
        return false;
    }
    virtual S32 frustumCheck(const LLViewerOctreeGroup *)
    {
        return 2;
    }
    virtual S32 frustumCheckObjects(const LLViewerOctreeGroup *)
    {
        return 2;
    }
    virtual void processGroup(LLViewerOctreeGroup *)
    {
    }
    void traverse(Node *n)
    {
        if (!earlyFail(n->listener) && frustumCheck(n->listener) && frustumCheckObjects(n->listener))
            processGroup(n->listener);
    }
    int AABBInRegionFrustumNoFarClipGroupBounds(const LLViewerOctreeGroup *)
    {
        return 2;
    }
    int AABBInRegionFrustumNoFarClipObjectBounds(const LLViewerOctreeGroup *)
    {
        return 2;
    }
    int AABBRegionSphereIntersectGroupExtents(const LLViewerOctreeGroup *, LLVector3)
    {
        return 2;
    }
    int AABBRegionSphereIntersectObjectExtents(const LLViewerOctreeGroup *, LLVector3)
    {
        return 2;
    }
    int checkProjectionArea(const LLVector4a &, const LLVector4a &, LLVector3, F32, F32)
    {
        return 2;
    }
};
int AABBSphereIntersect(const LLVector4a &, const LLVector4a &, const LLVector3 &, float)
{
    return 2;
}
struct AgentCamera
{
    float mDrawDistance = 112;
} gAgentCamera;
int gSavedSettings;
template <class T> struct LLCachedControl
{
    LLCachedControl(int &, const char *)
    {
    }
    operator T() const
    {
        return true;
    }
};
constexpr float SG_OCCLUSION_FUDGE = .25f;
bool LLPipeline::captureMetalVisibilityQuery(auto &, auto, const LLVector4a &, const LLVector4a &, LLCamera *,
                                             bool)
{
    ++captured;
    return capture_ok;
}
LLPipeline gPipeline;

struct LLGLSLShader
{
    static inline LLGLSLShader *sCurBoundShaderPtr = nullptr;
    void uniform3fv(int, int, const float *)
    {
    }
    void uniform3f(int, float, float, float)
    {
    }
};
struct LLShaderMgr
{
    enum
    {
        BOX_CENTER,
        BOX_SIZE
    };
};
struct LLRender
{
    enum
    {
        TRIANGLE_FAN
    };
};
[[maybe_unused]] constexpr int b111 = 7;
int get_box_fan_indices(LLCamera *, const LLVector4a &)
{
    return 0;
}

// @PRODUCTION_BODIES@

namespace
{
int checks = 0;
int failures = 0;
void check(bool accepted, const std::string &label)
{
    ++checks;
    if (!accepted)
    {
        ++failures;
        std::cerr << "FAILED: " << label << '\n';
    }
}
void reset()
{
    LLViewerCamera::sCurCameraID = 0;
    LLPipeline::sUseOcclusion = 2;
    LLPipeline::sShadowRender = false;
    for (U32 &frame : LLOcclusionCullingGroup::sOcclusionForgotten)
        frame = 0;
    gGL.pool = {};
    capture_ok = true;
    captured = 0;
    LLViewerRegion::camera_static = true;
}
void bounds(LLVOCacheGroup &group)
{
    group.mBounds[0] = LLVector4a(10, 10, 2000);
    group.mBounds[1] = LLVector4a(20, 20, 8.5);
}
void scheduleControls()
{
    const std::array<std::string, 7> modes = {"inside",     "expired",      "refused", "capture_failed",
                                              "ready_seen", "ready_hidden", "pending"};
    for (U32 phase = 0; phase < 16; ++phase)
    {
        for (U32 hash = 0; hash < 16; ++hash)
        {
            for (const auto &mode : modes)
            {
                reset();
                LLViewerRegion region;
                LLVOCacheGroup group;
                LLVOCachePartition partition(&region, &group);
                LLCamera camera;
                camera.pos = mode == "inside" ? LLVector3(10, 10, 2003) : LLVector3(100, 0, 0);
                bounds(group);
                group.mLODHash = hash;
                const U32 first = 10000 + phase;
                // A full-front-first phase is possible after the prior frame
                // traversed an empty tree. Back selection still runs normally.
                partition.mIdleHash = first % 8 ? first % 8 : 7;
                gGL.pool.status = mode == "ready_seen" || mode == "ready_hidden" ? ALQueryPool::Status::Ready
                                  : mode == "pending" ? ALQueryPool::Status::Pending
                                                      : ALQueryPool::Status::Expired;
                gGL.pool.value = mode == "ready_seen" ? 1 : 0;
                gGL.pool.allow = mode != "refused";
                capture_ok = mode != "capture_failed";
                int first_admission = -1;
                const std::string label =
                    mode + "/phase=" + std::to_string(phase) + "/hash=" + std::to_string(hash);
                for (gFrameCount = first; gFrameCount < first + 512; ++gFrameCount)
                {
                    region.frame_visible = 0;
                    const int front = partition.cull(camera, true);
                    if (gFrameCount == first)
                    {
                        check(front == 1, label + "/starts-front");
                        check(region.admissions == 0, label + "/fresh-needs-decision");
                    }
                    partition.processOccluders(&camera);
                    if (region.admissions && first_admission < 0)
                        first_admission = gFrameCount;
                }
                if (mode == "ready_hidden" || mode == "pending")
                    check(region.admissions == 0, label + "/retains-occlusion-or-pending");
                else
                    check(first_admission >= 0, label + "/bounded-visible-admission");
            }

            // Complementary legal back-first phase: clear state remains
            // eligible for the actual back-sphere selection, before any query.
            reset();
            LLViewerRegion region;
            LLVOCacheGroup group;
            LLVOCachePartition partition(&region, &group);
            LLCamera camera;
            group.mLODHash = hash;
            gFrameCount = 10000 + phase;
            partition.mIdleHash = (gFrameCount % 8 + 2) % 8;
            check(partition.cull(camera, true) == 0, "back-first/schedule");
            check(region.admissions == 1, "back-first/admission");
        }
    }
}
void decisionControls()
{
    reset();
    LLViewerRegion region;
    LLVOCacheGroup group;
    LLVOCachePartition partition(&region, &group);
    gFrameCount = 1000;
    check(!group.checkOcclusion(), "fresh unknown is not an immediate visibility result");
    group.mOcclusionQuery[0] = {true};
    group.setOcclusionState(group.QUERY_PENDING | group.OCCLUDED);
    gGL.pool.status = ALQueryPool::Status::Pending;
    check(!group.checkOcclusion(), "pending is unresolved");
    check(group.getOcclusionState() == (group.QUERY_PENDING | group.OCCLUDED),
          "pending retains prior occlusion");
    check(group.mOcclusionQuery[0].isValid(), "pending retains ticket");
    gGL.pool.status = ALQueryPool::Status::Expired;
    check(group.checkOcclusion(), "expired resolves conservatively visible");
    check(!group.getOcclusionState(), "expired clears hidden and pending states");
    check(!group.mOcclusionQuery[0].isValid(), "expired releases ticket");

    group.setOcclusionState(group.DISCARD_QUERY | group.QUERY_PENDING | group.OCCLUDED);
    check(!group.mOcclusionQuery[0].isValid(), "actual discard setter invalidates ticket");
    check(group.checkOcclusion(), "discard without ticket resolves visible");
    check(!group.getOcclusionState(), "discard clears terminal state");

    LLCamera camera;
    bounds(group);
    camera.pos = LLVector3(10, 10, 2003);
    group.doOcclusion(&camera, nullptr);
    check(group.isOcclusionState(group.DISCARD_QUERY), "inside box produces discard");
    check(!group.mOcclusionQuery[0].isValid(), "inside box issues no query");
    camera.pos = LLVector3(100, 0, 0);
    ++gFrameCount;
    group.doOcclusion(&camera, nullptr);
    check(group.mOcclusionQuery[0].isValid(), "leaving box issues new query");
    check(group.isOcclusionState(group.QUERY_PENDING) && !group.isOcclusionState(group.DISCARD_QUERY),
          "new issue supersedes inside decision");
    gGL.pool.status = ALQueryPool::Status::Ready;
    gGL.pool.value = 0;
    check(!group.checkOcclusion(), "new known zero supersedes earlier inside decision");
    check(group.isOcclusionState(group.OCCLUDED), "known zero remains hidden");

    // Current parent occlusion wins over every child conservative decision.
    for (const std::string mode : {"discard", "expired", "forgotten"})
    {
        reset();
        LLVOCacheGroup parent;
        parent.setOcclusionState(parent.OCCLUDED);
        parent.mOcclusionIssued[0] = 20;
        group.parent = &parent;
        group.mOcclusionIssued[0] = 10;
        group.mOcclusionState[0] = group.OCCLUDED | group.QUERY_PENDING;
        group.mOcclusionQuery[0] = {true};
        if (mode == "discard")
            group.setOcclusionState(group.DISCARD_QUERY);
        if (mode == "forgotten")
            group.sOcclusionForgotten[0] = 15;
        gGL.pool.status = ALQueryPool::Status::Expired;
        check(!group.checkOcclusion(), mode + "/known-hidden-parent-wins");
        check(parent.isOcclusionState(parent.OCCLUDED), mode + "/parent-preserved");
        group.parent = nullptr;
    }
    reset();
    group.mOcclusionIssued[0] = 10;
    group.mOcclusionState[0] = group.OCCLUDED | group.QUERY_PENDING;
    group.mOcclusionQuery[0] = {true};
    group.sOcclusionForgotten[0] = 15;
    check(group.checkOcclusion(), "forgotten without hidden parent releases visibility");
    check(!group.getOcclusionState(), "forgotten clears only stale own state");

    reset();
    group.mOcclusionState[0] = group.DISCARD_QUERY;
    group.mOcclusionState[1] = group.OCCLUDED | group.QUERY_PENDING;
    group.mOcclusionQuery[0] = {};
    group.mOcclusionQuery[1] = {true};
    check(group.checkOcclusion(), "camera0 discard resolves visible");
    check(group.mOcclusionState[1] == (group.OCCLUDED | group.QUERY_PENDING) &&
              group.mOcclusionQuery[1].isValid(),
          "camera0 leaves camera1 unchanged");
    LLViewerCamera::sCurCameraID = 1;
    gGL.pool.status = ALQueryPool::Status::Ready;
    gGL.pool.value = 0;
    check(!group.checkOcclusion(), "camera1 known zero stays hidden");
    check(!group.mOcclusionState[0], "camera1 leaves camera0 resolved");

    // Keep the actual descendant invalidation walk when releasing a parent.
    reset();
    group.mOcclusionState[0] = group.OCCLUDED;
    LLVOCacheGroup child;
    child.mSpatialPartition = &partition;
    child.parent = &group;
    child.node.parent = &group.node;
    group.node.children.push_back(&child.node);
    child.setOcclusionState(child.OCCLUDED | child.QUERY_PENDING);
    child.mOcclusionQuery[0] = {true};
    group.setOcclusionState(group.DISCARD_QUERY);
    check(group.checkOcclusion(), "discarded parent resolves visible");
    check(!child.mOcclusionQuery[0].isValid(), "parent release invalidates stale child ticket");
    check(child.isOcclusionState(child.DISCARD_QUERY) && !child.isOcclusionState(child.OCCLUDED),
          "parent release clears hidden child and retains discard receipt");
    check(child.checkOcclusion(), "discarded stale child also makes conservative progress");
    group.node.children.clear();
}
} // namespace
int main()
{
    LLGLSLShader shader;
    LLGLSLShader::sCurBoundShaderPtr = &shader;
    scheduleControls();
    decisionControls();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
