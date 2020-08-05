#include "MatrixAccountSession.h"
#include "include/ITeam.h"
#include "PolyTrix.h"
#include "include/Message.h"
#include <chrono>
#include <memory>

using namespace PolyTrixPlugin;

MatrixAccountSession::MatrixAccountSession(Polychat::IAccount& coreAccount, Polychat::ICore& core,
		std::string serverAddr, std::string name, std::string password)
	: coreAccount(coreAccount), core(core), backendSession(serverAddr)
{
	//std::future<void> loginResult = backendSession.login(name, password);
	bool isSuccess = backendSession.login(name, password);
}

void MatrixAccountSession::refresh(std::shared_ptr<IConversation> currentlyViewedConversation) {
	// TODO: Now is when the rooms are loaded
	//backendSession.getRooms();
}

void MatrixAccountSession::updatePosts(IConversation& conversation, int limit) {
	// TODO
}

bool MatrixAccountSession::isValid() {
	return true;
}

void MatrixAccountSession::sendMessageAction(std::shared_ptr<Message> message, MessageAction action) {
	// TODO: Send message
	switch (action) {
	case MessageAction::EDIT_MESSAGE:
		// TODO
		break;
	case MessageAction::PIN_MESSAGE:
	case MessageAction::UNPIN_MESSAGE:
		// TODO: Unsupported
		break;
	case MessageAction::REMOVE_MESSAGE:
		// TODO
		break;
	case MessageAction::SEND_NEW_MESSAGE:
		backendSession.sendMessage(message->channelId, message->msgContent);
	}
}
