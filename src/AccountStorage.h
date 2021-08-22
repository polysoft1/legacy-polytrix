/*
MIT License

Copyright(c) 2018 Konstantinos Sideris

Permission is hereby granted, free of charge, to any person obtaining a copy
of this softwareand associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright noticeand this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <string>
#include <map>
#include <vector>

#include <mtx/responses/sync.hpp>
#include "include/ILogger.h"
#include <nlohmann/json.hpp>

namespace PolyTrixPlugin {

constexpr auto OLM_ALGO = "m.olm.v1.curve25519-aes-sha2";
constexpr auto STORAGE_KEY = "secret";

struct OlmCipherContent
{
    std::string body;
    uint8_t type;
};

inline void
    from_json(const nlohmann::json& obj, OlmCipherContent& msg)
{
    msg.body = obj.at("body");
    msg.type = obj.at("type");
}

struct OlmMessage
{
    std::string sender_key;
    std::string sender;

    using RecipientKey = std::string;
    std::map<RecipientKey, OlmCipherContent> ciphertext;
};

inline void
    from_json(const nlohmann::json& obj, OlmMessage& msg)
{
    if (obj.at("type") != "m.room.encrypted") {
        throw std::invalid_argument("invalid type for olm message");
    }

    if (obj.at("content").at("algorithm") != OLM_ALGO)
        throw std::invalid_argument("invalid algorithm for olm message");

    msg.sender = obj.at("sender");
    msg.sender_key = obj.at("content").at("sender_key");
    msg.ciphertext =
        obj.at("content").at("ciphertext").get<std::map<std::string, OlmCipherContent>>();
}

template<class Container, class Item>
bool
    exists(const Container& container, const Item& item)
{
    return container.find(item) != container.end();
}

struct OutboundSessionData
{
    std::string session_id;
    std::string session_key;
    uint64_t message_index = 0;
};

inline void
    to_json(nlohmann::json& obj, const OutboundSessionData& msg)
{
    obj["session_id"] = msg.session_id;
    obj["session_key"] = msg.session_key;
    obj["message_index"] = msg.message_index;
}

inline void
    from_json(const nlohmann::json& obj, OutboundSessionData& msg)
{
    msg.session_id = obj.at("session_id");
    msg.session_key = obj.at("session_key");
    msg.message_index = obj.at("message_index");
}

struct OutboundSessionDataRef
{
    OlmOutboundGroupSession* session;
    OutboundSessionData data;
};

struct DevKeys
{
    std::string ed25519;
    std::string curve25519;
};

inline void
    to_json(nlohmann::json& obj, const DevKeys& msg)
{
    obj["ed25519"] = msg.ed25519;
    obj["curve25519"] = msg.curve25519;
}

inline void
    from_json(const nlohmann::json& obj, DevKeys& msg)
{
    msg.ed25519 = obj.at("ed25519");
    msg.curve25519 = obj.at("curve25519");
}

struct Storage
{
    Polychat::ILogger* logger;

    //! Storage for the user_id -> list of devices std::mapping.
    std::map<std::string, std::vector<std::string>> devices;
    //! Storage for the identity key for a device.
    std::map<std::string, DevKeys> device_keys;
    //! Flag that indicate if a specific room has encryption enabled.
    std::map<std::string, bool> encrypted_rooms;

    //! Keep track of members per room.
    std::map<std::string, std::map<std::string, bool>> members;

    void add_member(const std::string& room_id, const std::string& user_id)
    {
        members[room_id][user_id] = true;
    }

    //! std::mapping from curve25519 to session.
    std::map<std::string, mtx::crypto::OlmSessionPtr> olm_inbound_sessions;
    std::map<std::string, mtx::crypto::OlmSessionPtr> olm_outbound_sessions;

    // TODO: store message_index / event_id
    std::map<std::string, mtx::crypto::InboundGroupSessionPtr> inbound_group_sessions;
    // TODO: store rotation period
    std::map<std::string, OutboundSessionData> outbound_group_session_data;
    std::map<std::string, mtx::crypto::OutboundGroupSessionPtr> outbound_group_sessions;

    bool outbound_group_exists(const std::string& room_id)
    {
        return (outbound_group_sessions.find(room_id) != outbound_group_sessions.end()) &&
            (outbound_group_session_data.find(room_id) !=
                outbound_group_session_data.end());
    }

    void set_outbound_group_session(const std::string& room_id,
        mtx::crypto::OutboundGroupSessionPtr session,
        OutboundSessionData data)
    {
        outbound_group_session_data[room_id] = data;
        outbound_group_sessions[room_id] = std::move(session);
    }

    OutboundSessionDataRef get_outbound_group_session(const std::string& room_id)
    {
        return OutboundSessionDataRef{ outbound_group_sessions[room_id].get(),
                                        outbound_group_session_data[room_id] };
    }

    bool inbound_group_exists(const std::string& room_id,
        const std::string& session_id,
        const std::string& sender_key)
    {
        const auto key = room_id + session_id + sender_key;
        return inbound_group_sessions.find(key) != inbound_group_sessions.end();
    }

    void set_inbound_group_session(const std::string& room_id,
        const std::string& session_id,
        const std::string& sender_key,
        mtx::crypto::InboundGroupSessionPtr session)
    {
        const auto key = room_id + session_id + sender_key;
        inbound_group_sessions[key] = std::move(session);
    }

    OlmInboundGroupSession* get_inbound_group_session(const std::string& room_id,
        const std::string& session_id,
        const std::string& sender_key)
    {
        const auto key = room_id + session_id + sender_key;
        return inbound_group_sessions[key].get();
    }

    void load(std::string dbFile)
    {
        logger->info("restoring storage");

        std::ifstream db(dbFile);
        std::string db_data((std::istreambuf_iterator<char>(db)), std::istreambuf_iterator<char>());

        if (db_data.empty())
            return;

        json obj = json::parse(db_data);

        devices = obj.at("devices").get<std::map<std::string, std::vector<std::string>>>();
        device_keys = obj.at("device_keys").get<std::map<std::string, DevKeys>>();
        encrypted_rooms = obj.at("encrypted_rooms").get<std::map<std::string, bool>>();
        members = obj.at("members").get<std::map<std::string, std::map<std::string, bool>>>();

        if (obj.count("olm_inbound_sessions") != 0) {
            auto sessions = obj.at("olm_inbound_sessions").get<std::map<std::string, std::string>>();
            for (const auto& s : sessions)
                olm_inbound_sessions[s.first] =
                mtx::crypto::unpickle<mtx::crypto::SessionObject>(s.second, STORAGE_KEY);
        }

        if (obj.count("olm_outbound_sessions") != 0) {
            auto sessions = obj.at("olm_outbound_sessions").get<std::map<std::string, std::string>>();
            for (const auto& s : sessions)
                olm_outbound_sessions[s.first] =
                mtx::crypto::unpickle<mtx::crypto::SessionObject>(s.second, STORAGE_KEY);
        }

        if (obj.count("inbound_group_sessions") != 0) {
            auto sessions = obj.at("inbound_group_sessions").get<std::map<std::string, std::string>>();
            for (const auto& s : sessions)
                inbound_group_sessions[s.first] =
                mtx::crypto::unpickle<mtx::crypto::InboundSessionObject>(s.second, STORAGE_KEY);
        }

        if (obj.count("outbound_group_sessions") != 0) {
            auto sessions =
                obj.at("outbound_group_sessions").get<std::map<std::string, std::string>>();
            for (const auto& s : sessions)
                outbound_group_sessions[s.first] =
                mtx::crypto::unpickle<mtx::crypto::OutboundSessionObject>(s.second, STORAGE_KEY);
        }

        if (obj.count("outbound_group_session_data") != 0) {
            auto sessions = obj.at("outbound_group_session_data")
                .get<std::map<std::string, OutboundSessionData>>();
            for (const auto& s : sessions)
                outbound_group_session_data[s.first] = s.second;
        }
    }

    void save(std::string dbFile)
    {
        logger->info("saving storage");

        std::ofstream db(dbFile);
        if (!db.is_open()) {
            logger->critical("couldn't open file to save keys");
            return;
        }

        json data;
        data["devices"] = devices;
        data["device_keys"] = device_keys;
        data["encrypted_rooms"] = encrypted_rooms;
        data["members"] = members;

        // Save inbound sessions
        for (const auto& s : olm_inbound_sessions)
            data["olm_inbound_sessions"][s.first] =
            mtx::crypto::pickle<mtx::crypto::SessionObject>(s.second.get(), STORAGE_KEY);

        for (const auto& s : olm_outbound_sessions)
            data["olm_outbound_sessions"][s.first] =
            mtx::crypto::pickle<mtx::crypto::SessionObject>(s.second.get(), STORAGE_KEY);

        for (const auto& s : inbound_group_sessions)
            data["inbound_group_sessions"][s.first] =
            mtx::crypto::pickle<mtx::crypto::InboundSessionObject>(s.second.get(), STORAGE_KEY);

        for (const auto& s : outbound_group_sessions)
            data["outbound_group_sessions"][s.first] =
            mtx::crypto::pickle<mtx::crypto::OutboundSessionObject>(s.second.get(), STORAGE_KEY);

        for (const auto& s : outbound_group_session_data)
            data["outbound_group_session_data"][s.first] = s.second;

        // Save to file
        db << data.dump(2);
        db.close();
    }
};
}