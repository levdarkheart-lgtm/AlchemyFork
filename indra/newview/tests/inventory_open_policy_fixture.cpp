// PPD474 actual tree/gallery entry bodies, with inventory and appearance effects
// stopped at their existing owner boundaries. No viewer/network action occurs.
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
using U32 = unsigned;
constexpr unsigned MASK_CONTROL=1, MASK_SHIFT=2;
struct Settings { U32 attachment=0; bool wearable=true; } gSavedSettings;
template<class T> class LLCachedControl {
    std::string key;
public:
    LLCachedControl(Settings&, const char* k, T): key(k) {}
    T operator()() const { return static_cast<T>(key=="InventoryAddAttachmentBehavior" ? gSavedSettings.attachment : gSavedSettings.wearable); }
};
struct Keyboard { unsigned mask=0; unsigned currentMask(bool) const { return mask; } } keyboard;
Keyboard* gKeyboard=&keyboard;
namespace LLAssetType { enum EType { AT_CLOTHING, AT_BODYPART, AT_OTHER }; }
struct LLViewerInventoryItem {
    LLAssetType::EType type=LLAssetType::AT_CLOTHING;
    bool isWearableType() const { return type!=LLAssetType::AT_OTHER; }
    auto getType() const { return type; }
    int getUUID() const { return 42; }
} item;
bool worn=false, present=true;
bool get_is_item_worn(int) { return worn; }
struct Call { std::string kind; bool replace=false; };
std::vector<Call> calls;
struct LLAppearanceMgr {
    static LLAppearanceMgr& instance() { static LLAppearanceMgr m; return m; }
    void wearItemOnAvatar(int, bool, bool replace) { calls.push_back({"wear",replace}); }
    void removeItemFromAvatar(int) { calls.push_back({"remove",false}); }
};
struct Bridge {
    int mUUID=42;
    LLViewerInventoryItem* getItem() { return present ? &item : nullptr; }
    void* getInventoryModel() { return nullptr; }
    void performAction(void*, const std::string& action) { calls.push_back({action,false}); }
};
struct LLObjectBridge:Bridge { void openItem(); };
struct LLWearableBridge:Bridge { void openItem(); };
struct LLObjectBridgeAction:Bridge { void attachOrDetach(); };
struct LLWearableBridgeAction:Bridge { void wearOnAvatar(); };
// @PRODUCTION_BODIES@
int checks=0, failures=0;
void check(bool value, const std::string& label) {
    ++checks; if (!value) { ++failures; if (failures<=16) std::cerr<<"FAIL "<<label<<'\n'; }
}
int main() {
    LLObjectBridge tree_object; LLObjectBridgeAction gallery_object;
    LLWearableBridge tree_wearable; LLWearableBridgeAction gallery_wearable;
    for (unsigned mask: {0u,MASK_CONTROL,MASK_SHIFT,MASK_CONTROL|MASK_SHIFT}) {
        keyboard.mask=mask;
        for (unsigned preference: {0u,1u,2u}) for (bool is_worn: {false,true}) {
            gSavedSettings.attachment=preference; worn=is_worn;
            // Truth table: preference0 Add,1 Replace; only Ctrl reverses it.
            const bool expected_replace=mask==MASK_CONTROL ? preference!=1 : preference==1;
            calls.clear(); tree_object.openItem();
            check(calls.size()==1,"tree attachment invokes one action");
            if (!calls.empty()) check(calls[0].kind==(worn?"detach":expected_replace?"wear":"wear_add"),"tree attachment Add/Replace/Ctrl/worn");
            calls.clear(); gallery_object.attachOrDetach();
            check(calls.size()==1,"gallery attachment invokes one action");
            if (!calls.empty()) {
                check(calls[0].kind==(worn?"remove":"wear"),"gallery attachment worn toggle");
                if (!worn) check(calls[0].replace==expected_replace,"gallery attachment Add/Replace/Ctrl");
            }
        }
        for (bool preference: {false,true}) for (bool is_worn: {false,true})
            for (int kind=0;kind<4;++kind) {
                gSavedSettings.wearable=preference; worn=is_worn; present=kind!=3;
                item.type=kind==1 ? LLAssetType::AT_BODYPART : kind==2 ? LLAssetType::AT_OTHER : LLAssetType::AT_CLOTHING;
                const bool skip=!present || kind==2 || (worn && kind==1);
                const bool expected_add=kind==0 && (mask==MASK_CONTROL ? !preference : preference);
                calls.clear(); tree_wearable.openItem();
                check(calls.size()==static_cast<unsigned>(!skip),"tree wearable item/bodypart guard");
                if (!skip && !calls.empty()) check(calls[0].kind==(worn?"take_off":expected_add?"wear_add":"wear"),"tree clothing Add/Replace/Ctrl");
                calls.clear(); gallery_wearable.wearOnAvatar();
                check(calls.size()==static_cast<unsigned>(!skip),"gallery wearable item/bodypart guard");
                if (!skip && !calls.empty()) {
                    check(calls[0].kind==(worn?"remove":"wear"),"gallery clothing worn toggle");
                    if (!worn) check(calls[0].replace==!expected_add,"gallery clothing Add/Replace/Ctrl");
                }
            }
    }
    std::cout<<"PPD474 "<<checks<<" checks, "<<failures<<" failures\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
