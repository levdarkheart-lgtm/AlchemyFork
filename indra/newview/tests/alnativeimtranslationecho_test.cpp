/**
 * @file alnativeimtranslationecho_test.cpp
 * @brief Actual IM send body: translated packets and bilingual local echo.
 *
 * alnativeimtranslationecho_test.py inserts the production method unchanged.
 * The services below capture its network and model boundaries; they do not
 * reproduce its send, local-echo or formatting decisions.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "alnativeimtranslationpolicy.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using U8 = std::uint8_t;
using S32 = std::int32_t;
// @PRODUCTION_LIMITS@

enum EInstantMessage
{
    IM_NOTHING_SPECIAL, IM_GROUP_INVITATION, IM_INVENTORY_OFFERED,
    IM_SESSION_INVITE, IM_SESSION_P2P_INVITE, IM_SESSION_CONFERENCE_START,
    IM_SESSION_SEND, IM_LURE_USER, IM_GODLIKE_LURE_USER, IM_FRIENDSHIP_OFFERED
};
constexpr U8 IM_ONLINE = 0;
constexpr U8 IM_OFFLINE = 1;

struct LLUUID
{
    int value = 0;
    bool notNull() const { return value != 0; }
    friend bool operator==(const LLUUID&, const LLUUID&) = default;
};

struct Packet
{
    LLUUID from, agent_session, recipient, chat;
    std::string text;
    U8 offline;
    EInstantMessage dialog;
};
struct Echo
{
    LLUUID chat, sender;
    std::string text;
    bool direct;
};
std::vector<Packet> packets;
std::vector<Echo> echoes;
std::vector<LLUUID> session_lookups, group_lookups;
struct ArgumentOwner
{
    LLUUID chat{22}, peer{23};
};
std::unique_ptr<ArgumentOwner> argument_owner;
std::vector<std::string> split_pieces;
std::string split_input;
std::size_t split_budget = 0;
char split_token = '\0';
int split_calls = 0;
int sends = 0;
bool remove_session_on_echo = false;
bool remove_arguments_on_echo = false;
int checks = 0;
int failures = 0;

void check(bool condition, const char* description)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}

struct Agent
{
    LLUUID getID() const { return {11}; }
    LLUUID getSessionID() const { return {12}; }
    void sendReliableMessage() { ++sends; }
} gAgent;
const LLUUID gAgentID{11};
void* gMessageSystem = nullptr;

void pack_instant_message(void*, const LLUUID& from, bool,
                          const LLUUID& agent_session, const LLUUID& recipient,
                          const char*, const char* text, U8 offline,
                          EInstantMessage dialog, const LLUUID& chat)
{
    packets.push_back({from, agent_session, recipient, chat, text, offline, dialog});
}

// A controlled splitter boundary, not a second copy of the UTF-8 algorithm.
// Cases supply complete UTF-8 pieces and verify which input the real sender
// submitted, then observe each resulting packet. Splitter correctness itself
// belongs to llcommon's existing string tests.
void utf8str_split(std::list<std::string>& output, std::string_view input,
                   std::size_t budget, char token)
{
    ++split_calls;
    split_input = input;
    split_budget = budget;
    split_token = token;
    output.assign(split_pieces.begin(), split_pieces.end());
}

struct LLAgentUI
{
    static void buildFullname(std::string& name) { name = "Local Resident"; }
};
struct LLRelationship
{
    bool online = true;
    bool isOnline() const { return online; }
};
struct LLAvatarTracker
{
    LLRelationship buddy;
    bool known = true;
    static LLAvatarTracker& instance() { static LLAvatarTracker tracker; return tracker; }
    const LLRelationship* getBuddyInfo(const LLUUID&) const { return known ? &buddy : nullptr; }
};
struct LLMuteList
{
    enum { AR_IM };
    static LLMuteList* getInstance() { static LLMuteList list; return &list; }
    void autoRemove(const LLUUID&, int) {}
};
struct LLIMSpeakerMgr
{
    void speakerChatted(const LLUUID&) {}
    void setSpeakerTyping(const LLUUID&, bool) {}
};
struct LLGroupMgr
{
    static LLGroupMgr* getInstance() { static LLGroupMgr manager; return &manager; }
    const void* getGroupData(const LLUUID& id) const { group_lookups.push_back(id); return nullptr; }
};
struct LLRecentPeople
{
    static LLRecentPeople& instance() { static LLRecentPeople recent; return recent; }
    void add(const LLUUID&) {}
};
struct LLIMModel
{
    struct LLIMSession
    {
        bool group = false;
        std::vector<LLUUID> mInitialTargetIDs{{23}};
        bool isGroupSessionType() const { return group; }
        bool isP2PSessionType() const { return !group; }
    };
    std::unique_ptr<LLIMSession> session = std::make_unique<LLIMSession>();

    static LLIMModel* getInstance() { static LLIMModel model; return &model; }
    static LLIMModel& instance() { return *getInstance(); }
    LLIMSession* findIMSession(const LLUUID& id) { session_lookups.push_back(id); return session.get(); }
    LLIMSpeakerMgr* getSpeakerManager(const LLUUID&) { static LLIMSpeakerMgr manager; return &manager; }
    static void addSpeakersToRecent(const LLUUID&) {}
    void addMessage(const LLUUID& chat, const std::string&, const LLUUID& sender,
                    const std::string& text)
    {
        echoes.push_back({chat, sender, text, false});
    }
    void processAddingMessage(const LLUUID& chat, const std::string&, const LLUUID& sender,
                              const std::string& text, bool = true, bool = false, std::uint32_t = 0)
    {
        echoes.push_back({chat, sender, text, true});
        if (remove_session_on_echo) session.reset();
        if (remove_arguments_on_echo) argument_owner.reset();
    }
    static void sendMessage(const std::string&, const LLUUID&, const LLUUID&,
                            EInstantMessage, const std::string& = {});
};

// @PRODUCTION_SEND_BODY@

void reset()
{
    packets.clear(); echoes.clear(); split_pieces.clear(); split_input.clear();
    session_lookups.clear(); group_lookups.clear(); argument_owner.reset();
    split_budget = 0; split_token = '\0'; split_calls = 0; sends = 0;
    remove_session_on_echo = false;
    remove_arguments_on_echo = false;
    LLAvatarTracker::instance().known = true;
    LLAvatarTracker::instance().buddy.online = true;
    LLIMModel::getInstance()->session = std::make_unique<LLIMModel::LLIMSession>();
}

void shortSend(std::string_view original, std::string_view target,
               std::string_view local, bool direct)
{
    reset();
    LLIMModel::sendMessage(std::string(target), {22}, {23}, IM_NOTHING_SPECIAL, std::string(original));
    check(packets.size() == 1, "Short IM packs exactly one packet");
    check(sends == 1, "Short IM dispatches exactly one packet");
    check(split_calls == 0, "Short IM never invokes the splitter");
    if (!packets.empty())
    {
        check(packets.front().text == target, "Recipient gets exactly the translated bytes");
        check(packets.front().chat == LLUUID{22}, "Recipient packet retains the chat UUID");
        check(packets.front().recipient == LLUUID{23}, "Recipient packet retains the peer UUID");
        check(packets.front().dialog == IM_NOTHING_SPECIAL, "Ordinary P2P wire dialog is unchanged");
    }
    check(echoes.size() == 1, "Sent IM produces exactly one local echo");
    if (!echoes.empty())
    {
        check(echoes.front().text == local, "Local echo shows expected original and bracket translation");
        check(echoes.front().direct == direct, "Bilingual local echo bypasses the global translator");
        check(echoes.front().chat == LLUUID{22} && echoes.front().sender == gAgentID,
              "Local echo retains session and self identity");
    }
}

int main()
{
    shortSend("Hello", "こんにちは", "Hello\n[こんにちは]", true);
    shortSend("", "Ordinary text", "Ordinary text", false);
    shortSend("OK", "OK", "OK", true);
    shortSend("/me waves", "/me 挨拶する", "/me waves\n[挨拶する]", true);
    shortSend("/me's hat", "/me's 帽子", "/me's hat\n[s 帽子]", true);
    shortSend("/me waves", "/me'別の形式", "/me waves\n[/me'別の形式]", true);
    shortSend(std::string(5000, 'q'), "短い", std::string(5000, 'q') + "\n[短い]", true);
    shortSend("First line\nSecond line", "翻訳", "First line\nSecond line\n[翻訳]", true);
    shortSend("こんにちは 👋", "Bonjour 👋", "こんにちは 👋\n[Bonjour 👋]", true);
    shortSend("See https://example.com/a?b=1#c and `x[y]`", "https://example.com/a?b=1#c と `x[y]` を参照",
              "See https://example.com/a?b=1#c and `x[y]`\n[https://example.com/a?b=1#c と `x[y]` を参照]", true);

    reset();
    std::string first, second;
    for (int i = 0; i < 200; ++i) { first += "日"; second += "本"; }
    const std::string translated = first + second;
    const std::string original = "ORIGINAL_ONLY_LOCAL";
    split_pieces = {first, second};
    LLIMModel::sendMessage(translated, {22}, {23}, IM_NOTHING_SPECIAL, original);
    check(translated.size() > MAX_MSG_STR_LEN, "UTF-8 fixture exercises the long-wire path");
    check(split_calls == 1 && split_input == translated, "Only translated text enters the UTF-8 splitter");
    check(split_budget == MAX_MSG_STR_LEN && split_token == ' ', "Existing transport split policy is unchanged");
    check(packets.size() == 2 && sends == 2, "Every controlled UTF-8 piece is packed and dispatched once");
    if (packets.size() == 2)
    {
        check(packets[0].text == first && packets[1].text == second, "UTF-8 pieces retain their exact bytes");
        check(packets[0].text.find(original) == std::string::npos &&
              packets[1].text.find(original) == std::string::npos, "Original never enters a split packet");
    }
    check(echoes.size() == 1, "Split wire text has one complete local echo");
    if (!echoes.empty())
    {
        check(echoes[0].text == original + "\n[" + translated + "]", "Local echo retains complete translation across packet boundaries");
        check(echoes[0].direct, "Split bilingual local echo bypasses global translation");
    }

    reset();
    LLAvatarTracker::instance().buddy.online = false;
    LLIMModel::sendMessage("送信", {22}, {23}, IM_NOTHING_SPECIAL, "Original");
    check(packets.size() == 1 && packets[0].offline == IM_OFFLINE, "Offline-recipient routing is unchanged");

    for (const auto dialog : {IM_SESSION_P2P_INVITE, IM_SESSION_CONFERENCE_START})
    {
        reset();
        LLIMModel::sendMessage("Wire only", {22}, {23}, dialog, "Local original");
        check(packets.size() == 1 && packets[0].text == "Wire only", "Nonordinary dialog still transmits only target text");
        check(packets.size() == 1 && packets[0].dialog == IM_SESSION_SEND, "Existing server-echo dialog mapping is unchanged");
        check(echoes.empty(), "Server-echo dialogs do not gain duplicate local echoes");
    }

    reset();
    LLIMModel::sendMessage("Global target", {22}, {}, IM_NOTHING_SPECIAL, "Local original");
    check(packets.size() == 1 && packets[0].text == "Global target", "Null-recipient wire text is unchanged");
    check(echoes.empty(), "Null recipient does not gain a local P2P echo");

    reset();
    remove_session_on_echo = true;
    LLIMModel::sendMessage("翻訳", {22}, {23}, IM_NOTHING_SPECIAL, "Original");
    check(packets.size() == 1 && sends == 1, "Callback-close retains the already sent packet");
    check(echoes.size() == 1 && echoes[0].text == "Original\n[翻訳]",
          "Callback-close produces one correct bilingual echo");
    check(!LLIMModel::getInstance()->session, "Local echo can synchronously remove the session");

    reset();
    argument_owner = std::make_unique<ArgumentOwner>();
    remove_arguments_on_echo = true;
    LLIMModel::sendMessage("翻訳", argument_owner->chat, argument_owner->peer,
                          IM_NOTHING_SPECIAL, "Original");
    check(!argument_owner, "Local echo can synchronously destroy the owner of argument UUIDs");
    check(packets.size() == 1 && packets[0].chat == LLUUID{22} && packets[0].recipient == LLUUID{23},
          "Callback argument-owner deletion retains the sent routing IDs");
    check(!session_lookups.empty() && session_lookups.back() == LLUUID{22},
          "Post-echo session lookup retains the original owned ID");
    check(!group_lookups.empty() && group_lookups.back() == LLUUID{23},
          "Post-echo peer lookup retains the original owned ID");

    std::cout << "Native IM actual send body: " << checks << " checks, " << failures << " failures\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
