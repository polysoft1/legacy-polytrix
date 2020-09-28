#ifndef MATRIX_ACCOUNT
#define MATRIX_ACCOUNT

#include <string>
#include <map>
#include <libmatrix-client/MatrixSession.h>
#include "include/IProtocolSession.h"
#include "include/ICore.h"
#include "include/ITeam.h"

class PolyMost;
using namespace Polychat;

namespace Polychat {
class IAccount;
}

namespace PolyTrixPlugin {

/**
 * Represents a user on Matrix. Can be the logged in user
 * or just another team member.
 */
class MatrixAccountSession : public Polychat::IProtocolSession {
private:
	IAccount& coreAccount;

	Polychat::ICore& core;

	LibMatrix::MatrixSession backendSession;

	void doSync();

	bool isSyncing = false;

public:
	explicit MatrixAccountSession(Polychat::IAccount& coreAccount, Polychat::ICore& core,
		std::string serverAddr, std::string name, std::string password);

	virtual IAccount& getAccount() {
		return coreAccount;
	}

	virtual void refresh(std::shared_ptr<IConversation> currentlyViewedConversation);

	virtual void updatePosts(IConversation& conversation, int limit);

	virtual bool isValid();

	virtual void sendMessageAction(std::shared_ptr<Message>, MessageAction);

};

}

#endif // !Matrix_ACCOUNT
