#ifndef MATRIX_ACCOUNT
#define MATRIX_ACCOUNT

#include <string>
#include <map>
#include <mtxclient/http/client.hpp>
#include <mtxclient/crypto/client.hpp>
#include "include/IProtocolSession.h"
#include "include/ICore.h"
#include "include/ITeam.h"
#include "AccountStorage.h"

using namespace Polychat;

using TimelineEvent = mtx::events::collections::TimelineEvents;
using EncryptedEvent = mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>;

namespace Polychat {
class IAccount;
}

namespace PolyTrixPlugin {
class PolyTrix;


/**
 * Represents a user on Matrix. Can be the logged in user
 * or just another team member.
 */
class MatrixAccountSession : public Polychat::IProtocolSession {
private:
	IAccount& coreAccount;

	PolyTrix& plugin;

	Polychat::ICore& core;

	std::shared_ptr<mtx::http::Client> client;
	std::shared_ptr<mtx::crypto::OlmClient> olmClient;
	Storage storage;

	void doSync();

	bool isSyncing = false;

	std::string olmAccountFilePath;

	void loadOlm();
	void onLogin(const mtx::responses::Login& res, mtx::http::RequestErr err);

	void get_device_keys(const UserId& user);

	void save_device_keys(const mtx::responses::QueryKeys& res);

	void mark_encrypted_room(const RoomId& id);

	void handle_to_device_msgs(const mtx::responses::ToDevice& to_device);

	void print_errors(mtx::http::RequestErr err);

	void decrypt_olm_message(const OlmMessage& olm_msg);


	void onInitialSync(const mtx::responses::Sync sync, mtx::http::RequestErr err);
	void parseSyncMessages(const mtx::responses::Sync sync);
	void onKeysUploaded(const mtx::responses::UploadKeys&, mtx::http::RequestErr err);
	void send_encrypted_reply(const std::string& room_id, const std::string& reply_msg,
		mtx::http::Callback<mtx::responses::EventId> cb);
	void send_group_message(OlmOutboundGroupSession* session,
		const std::string& session_id,
		const std::string& room_id,
		const std::string& msg,
		mtx::http::Callback<mtx::responses::EventId> cb);
	void create_outbound_megolm_session(
		const std::string& room_id, const
		std::string& reply_msg,
		mtx::http::Callback<mtx::responses::EventId> cb);
	TimelineEvent parseEncryptedEvent(std::string roomID,
		EncryptedEvent encryptedEvent);
public:
	explicit MatrixAccountSession(PolyTrix& plugin, Polychat::IAccount& coreAccount,
		Polychat::ICore& core, std::string serverAddr, std::string name, std::string password);

	virtual IAccount& getAccount() {
		return coreAccount;
	}

	virtual void refresh(std::shared_ptr<IConversation> currentlyViewedConversation);

	virtual void updatePosts(IConversation& conversation, int limit);

	virtual bool isValid();

	virtual void sendMessageAction(std::shared_ptr<Message>, MessageAction);

	void onSync(const mtx::responses::Sync sync, mtx::http::RequestErr err);

	void shutdownSave();
};

}

#endif // !Matrix_ACCOUNT
