// PPD463: actual HUD demand branch and observation body; controlled CPU owners.
#include "alhudtextureinputs.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>
using S8 = std::int8_t; using S32 = std::int32_t; using U32 = std::uint32_t; using U64 = std::uint64_t; using F32 = float;
static int metadata_reads = 0, slot_reads = 0, emits = 0;
static bool fail_emit = false;
static std::ostringstream logs;
static std::ostream& logOutput() { if (fail_emit) throw std::runtime_error("controlled emitter failure"); ++emits; return logs; }
#define LL_INFOS(...) logOutput()
#define LL_ENDL '\n'
struct LLUUID { std::string value; std::string asString() const { ++metadata_reads; return value; } };
struct LLImageRaw {
    S32 width=256, height=128; S8 components=4;
    S32 getWidth() const { ++metadata_reads; return width; }
    S32 getHeight() const { ++metadata_reads; return height; }
    S8 getComponents() const { ++metadata_reads; return components; }
};
struct LLGLTexture { enum { BOOST_HUD=18 }; };
struct LLViewerTexture {
    enum { LOCAL_TEXTURE, MEDIA_TEXTURE, DYNAMIC_TEXTURE, FETCHED_TEXTURE, LOD_TEXTURE, INVALID_TEXTURE_TYPE };
    virtual ~LLViewerTexture() = default;
    S8 type=LOCAL_TEXTURE, components=4; S32 boost=0, width=1024, height=512, discard=-1, state=1;
    bool resident=false, parcel_media=false, fail_collect=false;
    LLUUID id{"10000000-0000-0000-0000-000000000001"};
    S8 getType() const { ++metadata_reads; if(fail_collect) throw std::runtime_error("controlled collection failure"); return type; }
    const LLUUID& getID() const { ++metadata_reads; return id; }
    S32 getBoostLevel() const { ++metadata_reads; return boost; }
    S32 getFullWidth() const { ++metadata_reads; return width; }
    S32 getFullHeight() const { ++metadata_reads; return height; }
    S8 getComponents() const { ++metadata_reads; return components; }
    S32 getTextureState() const { ++metadata_reads; return state; }
    bool hasGPUTexture() const { ++metadata_reads; return resident; }
    bool hasParcelMedia() const { ++metadata_reads; return parcel_media; }
    S32 getDiscardLevel() const {
        ++metadata_reads;
        if(type!=FETCHED_TEXTURE && type!=LOD_TEXTURE && !resident) throw std::runtime_error("unsafe backing getter");
        return discard;
    }
};
struct LLViewerFetchedTexture: LLViewerTexture {
    S8 desired=0; S32 raw_discard=-1; bool raw_valid=false, has_fetcher=false, fetching=false, missing=false;
    bool mCreatePending=false, mDownScalePending=false; U64 mCreateQueuedFrame=0;
    LLImageRaw* raw=nullptr;
    LLViewerFetchedTexture() { type=FETCHED_TEXTURE; }
    S32 getDesiredDiscardLevel() const { ++metadata_reads; return desired; }
    S32 getRawImageLevel() const { ++metadata_reads; return raw_discard; }
    bool isRawImageValid() const { ++metadata_reads; return raw_valid; }
    bool hasFetcher() const { ++metadata_reads; return has_fetcher; }
    bool isFetching() const { ++metadata_reads; return fetching; }
    bool isMissingAsset() const { ++metadata_reads; return missing; }
    LLImageRaw* getRawImage() const { ++metadata_reads; return raw; }
};
struct ALTextureBoost {
    LLViewerTexture* texture=nullptr;
    ALTextureBoost(LLViewerTexture* p, S32 boost) { hold(p,boost); }
    void hold(LLViewerTexture* p, S32 boost) { texture=p; p->boost=boost; }
    LLViewerTexture* get() const { return texture; }
};
struct LLFace {
    std::array<LLViewerTexture*,8> textures{};
    F32 area=0, virtual_size=0, ordinary_size=33;
    void setPixelArea(F32 value) { area=value; }
    void setVirtualSize(F32 value) { virtual_size=value; }
    F32 getTextureVirtualSize() const { return ordinary_size; }
    LLViewerTexture* getTexture(U32 channel) const { ++slot_reads; return textures.at(channel); }
};
struct LLViewerCamera { S32 getScreenPixelArea() const { return 2000000; } } camera_owner;
U32 gFrameCount=1;
// @ACTUAL_OBSERVATION@
struct LLVOVolume {
    LLUUID object{"20000000-0000-0000-0000-000000000001"};
    bool hud=true; F32 observed_vsize=0;
    std::vector<ALTextureBoost> mHUDBoosts;
    bool isHUDAttachment() const { return hud; }
    const LLUUID& getID() const { return object; }
    void apply(LLFace* face, LLViewerTexture* imagep, [[maybe_unused]] U32 ch_min=0,
               [[maybe_unused]] U32 ch_max=2, [[maybe_unused]] S32 i=0) {
        auto* camera=&camera_owner;
        std::size_t hud_held=0;
        F32 vsize;
// @ACTUAL_DEMAND_BRANCH@
        observed_vsize=vsize;
        mHUDBoosts.erase(mHUDBoosts.begin()+hud_held,mHUDBoosts.end());
    }
};
static unsigned checks=0, failures=0;
void check(bool ok, const char* message) { ++checks; if(!ok){++failures;std::cerr<<"FAIL "<<message<<'\n';} }
bool contains(const std::string& text) { return logs.str().find(text)!=std::string::npos; }
void policy(const LLVOVolume& owner,const LLFace& face,const LLViewerFetchedTexture& texture) {
    check(face.area==2000000 && face.virtual_size==2000000 && owner.observed_vsize==2000000,"actual HUD face demand unchanged");
    check(owner.mHUDBoosts.size()==1 && owner.mHUDBoosts.front().get()==&texture && texture.boost==LLGLTexture::BOOST_HUD,"actual boost owner unchanged");
}
void disabled() {
    LLVOVolume owner; LLFace face; LLViewerFetchedTexture atlas; face.textures[0]=&atlas;
    for(unsigned n=0;n<20;++n){++gFrameCount;owner.apply(&face,&atlas);}
    check(metadata_reads==0 && slot_reads==0 && emits==0 && logs.str().empty(),"disabled gate precedes collection, slot walk and logging");
    policy(owner,face,atlas);
    atlas.fail_collect=true; owner.apply(&face,&atlas); policy(owner,face,atlas);
}
void enabled() {
    LLVOVolume owner; LLFace face; LLViewerFetchedTexture atlas; LLViewerTexture media;
    media.type=LLViewerTexture::MEDIA_TEXTURE; media.parcel_media=true;
    media.id.value="30000000-0000-0000-0000-000000000001";
    face.textures[0]=&atlas; face.textures[1]=&media;
    owner.apply(&face,&atlas); policy(owner,face,atlas);
    check(emits==2,"selected atlas and existing media slot both observed");
    check(contains("kind=hud_texture asset=") && contains("slot=0 frame=1 texture_type=3 type=fetched hud_boost_owner=1 boost=18"),"real fetched owner schema and numeric S8 type");
    check(contains("components=4 texture_state=1") && contains("fetch_details_known=1 desired_discard=0 raw_discard=-1"),"components and signed discard serialize numerically");
    check(contains("slot=1 frame=1 texture_type=1 type=media hud_boost_owner=0 boost=0"),"non-selected media does not acquire a HUD boost");
    check(contains("current_discard_known=0 current_discard=-1 parcel_media=1 fetch_details_known=0 desired_discard=unknown") && contains("has_fetcher=unknown fetching=unknown missing=unknown"),"media without backing avoids fetched casts/backing getters and marks unknowns");
    const int initial=emits;
    for(unsigned n=0;n<20;++n){++gFrameCount;owner.object.value="20000000-0000-0000-0000-000000000002";owner.apply(&face,&atlas,0,2,99);}
    check(emits==initial,"frames, representative face and owner identity do not consume asset-state slots");
    atlas.has_fetcher=true;atlas.fetching=true;owner.apply(&face,&atlas);
    check(emits==initial+1 && contains("has_fetcher=1 fetching=1 missing=0"),"new actual fetch state is admitted");
    LLImageRaw raw;raw.components=1;atlas.raw=&raw;atlas.raw_valid=true;atlas.raw_discard=0;atlas.mCreatePending=true;atlas.mCreateQueuedFrame=20;
    owner.apply(&face,&atlas);check(emits==initial+2 && contains("raw_discard=0 raw_present=1 raw_valid=1 raw_width=256 raw_height=128 raw_components=1"),"raw-to-create progression is recorded numerically without bytes");
    atlas.mCreateQueuedFrame=21;owner.apply(&face,&atlas);check(emits==initial+2,"queue frame is a logged value, not a key");
    atlas.resident=true;atlas.discard=0;atlas.fetching=false;atlas.has_fetcher=false;atlas.mCreatePending=false;owner.apply(&face,&atlas);
    check(emits==initial+3 && contains("gpu_resident=1 current_discard_known=1 current_discard=0"),"existing published output metadata recorded");
    check(atlas.raw==&raw && atlas.raw_valid && atlas.raw_discard==0 && atlas.desired==0 && atlas.discard==0,"observation preserves fetched inputs and demand");
    media.resident=true;media.discard=2;owner.apply(&face,&atlas);
    check(emits==initial+4 && contains("current_discard_known=1 current_discard=2 parcel_media=1 fetch_details_known=0"),"resident media uses safe backing discard and still has unknown fetch state");
    LLViewerFetchedTexture lod;lod.type=LLViewerTexture::LOD_TEXTURE;lod.id.value="40000000-0000-0000-0000-000000000001";face.textures[0]=&lod;owner.apply(&face,&lod);
    check(contains("texture_type=4 type=lod hud_boost_owner=1"),"LOD is a proven fetched subtype");
    LLViewerTexture dynamic;dynamic.type=LLViewerTexture::DYNAMIC_TEXTURE;dynamic.id.value="00000000-0000-0000-0000-000000000000";face.textures[0]=&dynamic;face.textures[1]=nullptr;
    owner.apply(&face,&dynamic);const int before=emits;owner.object.value="20000000-0000-0000-0000-000000000003";owner.apply(&face,&dynamic);
    check(emits==before+1 && contains("type=dynamic"),"null-asset dynamic textures distinguish HUD owners");
    for(S8 type:{S8(LLViewerTexture::LOCAL_TEXTURE),S8(LLViewerTexture::INVALID_TEXTURE_TYPE)}){dynamic.type=type;owner.apply(&face,&dynamic);}
    check(contains("type=local") && contains("type=other"),"non-fetched local/unknown types remain safe");
    LLFace pbr;pbr.textures[4]=&lod;pbr.textures[5]=&media;pbr.textures[6]=&atlas;
    const int pbr_before=emits;owner.apply(&pbr,&lod,3,6);
    check(emits==pbr_before+3 && contains("slot=4") && contains("slot=5") && contains("slot=6"),"PBR existing slots observed when basecolor is absent");
    check(owner.mHUDBoosts.size()==1 && owner.mHUDBoosts.front().get()==&lod && pbr.virtual_size==2000000,"PBR keeps the existing selected boost/demand owner");
    LLVOVolume ordinary;ordinary.hud=false;LLFace ordinary_face;const int reads=metadata_reads;const int observed=emits;
    ordinary.apply(&ordinary_face,&atlas);check(metadata_reads==reads && emits==observed && ordinary.mHUDBoosts.empty() && ordinary.observed_vsize==33,"ordinary face path remains unobserved and unchanged");
}
void exceptions() {
    LLVOVolume owner;LLFace face;LLViewerFetchedTexture atlas;face.textures[0]=&atlas;
    atlas.fail_collect=true;owner.apply(&face,&atlas);check(emits==0,"collection exception does not escape");policy(owner,face,atlas);
    atlas.fail_collect=false;fail_emit=true;owner.apply(&face,&atlas);fail_emit=false;policy(owner,face,atlas);
    atlas.has_fetcher=true;owner.apply(&face,&atlas);check(emits==1 && contains("has_fetcher=1"),"later metadata state survives emitter failure");
}
void cap() {
    for(unsigned n=0;n<64;++n) check(ALWarningDiagnostics::take(ALWarningDiagnostics::Kind::texture,"map-tile-"+std::to_string(n)),"unrelated texture category fills independently");
    LLVOVolume owner;LLFace face;LLViewerFetchedTexture atlas;face.textures[0]=&atlas;
    for(unsigned n=0;n<80;++n){atlas.raw_discard=S32(n);owner.apply(&face,&atlas);}
    check(emits==64,"HUD category independently admits exactly64 distinct states");
    check(owner.mHUDBoosts.size()==1 && atlas.raw_discard==79 && atlas.desired==0,"cap does not block or alter HUD demand");
}
int main(int argc,char** argv) {
    (void)&observeHUDTextureInputs; // Baseline branch deliberately has no observer call.
    const std::string mode=argc==2?argv[1]:"";
    if(mode=="disabled")disabled();else if(mode=="enabled")enabled();else if(mode=="exceptions")exceptions();else if(mode=="cap")cap();else return 2;
    std::cout<<mode<<": "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
