#include "MatrixAccountSession.h"
#include "include/ITeam.h"
#include "PolyTrix.h"
#include "include/Message.h"
#include <chrono>
#include <memory>

using namespace PolyTrixPlugin;

MatrixAccountSession::MatrixAccountSession(Polychat::IAccount& coreAccount,
		Polychat::ICore& core, std::string serverAddr, std::string name, std::string password)
	: coreAccount(coreAccount), core(core), backendServer(serverAddr)
{
	std::future<void> loginResult = backendServer.login(name, password);
}

void MatrixAccountSession::refresh(std::shared_ptr<IConversation> currentlyViewedConversation) {
}

void MatrixAccountSession::updatePosts(IConversation& conversation, int limit) {
	// TODO
}

bool MatrixAccountSession::isValid() {
	return true;
}

void MatrixAccountSession::sendMessageAction(std::shared_ptr<Message> message, MessageAction) {
	// TODO: Send message
}
