#ifndef SERVERLESS_ACCOUNT
#define SERVERLESS_ACCOUNT

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

namespace ServerlessTestPlugin {

/**
 * Represents a user on Serverless. Can be the logged in user
 * or just another team member.
 */
class ServerlessAccountSession : public Polychat::IProtocolSession {
private:
	IAccount& coreAccount;

	Polychat::ICore& core;

	LibMatrix::MatrixSession backendSession;

public:
	explicit ServerlessAccountSession(Polychat::IAccount& coreAccount, Polychat::ICore& core,
		std::string serverAddr, std::string name, std::string password);

	virtual IAccount& getAccount() {
		return coreAccount;
	}

	virtual void refresh(std::shared_ptr<IConversation> currentlyViewedConversation);

	virtual void updatePosts(IConversation& conversation, int limit);

	virtual bool isValid();

	virtual void sendMessageAction(std::shared_ptr<Message>, MessageAction);

	void addTeam(std::string teamName);
	void addConversation(std::string teamName, std::string conversationName);
	void addMsgToConversation(std::string teamName, std::string conversationName, std::string sender, std::string msg);

};

}

#endif // !SERVERLESS_ACCOUNT
