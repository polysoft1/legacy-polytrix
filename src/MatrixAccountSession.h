#ifndef MATRIX_ACCOUNT
#define MATRIX_ACCOUNT

#include <string>
#include <map>
#include <mtxclient/http/client.hpp>
#include "include/IProtocolSession.h"
#include "include/ICore.h"
#include "include/ITeam.h"

using namespace Polychat;

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

	void doSync();

	bool isSyncing = false;

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

};

}

#endif // !Matrix_ACCOUNT
