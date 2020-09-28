#include "MatrixAccountSession.h"
#include "include/ITeam.h"
#include "PolyTrix.h"
#include "include/Message.h"
#include <chrono>
#include <memory>
#include <functional>

#include "include/ICore.h"
#include "include/IAccount.h"
#include "include/IAccountManager.h"

using namespace PolyTrixPlugin;

MatrixAccountSession::MatrixAccountSession(Polychat::IAccount& coreAccount, Polychat::ICore& core,
		std::string serverAddr, std::string name, std::string password)
	: coreAccount(coreAccount), core(core), backendSession(serverAddr)
{
	std::function<void(std::future<void>&)> callback = [&core, &coreAccount, this](std::future<void>& result) {
		try {
			result.get();
			core.getAccountManager().alertOfSessionChange(coreAccount, AuthStatus::AUTHENTICATED);
		} catch (...) {
			core.getAccountManager().alertOfSessionChange(coreAccount, AuthStatus::FAIL_HTTP_ERROR);
		}
	};
	core.getScheduler().runOnReady(std::make_unique<FutureTask<void>>(
		backendSession.login(name, password), callback));
}

void MatrixAccountSession::doSync() {
	isSyncing = true;
	std::function<void(std::future<LibMatrix::RoomMap>&)> callback
		= [this](std::future<LibMatrix::RoomMap>& result)
	{
		try {
			LibMatrix::RoomMap roomUpdateData = result.get();

			const std::map<std::string, std::shared_ptr<IConversation>>& conversations
				= coreAccount.getConversations();
			for (auto roomPair : roomUpdateData) {
				// Determine if the conversation even exists in PolyChat
				auto itr = conversations.find(roomPair.first);
				std::shared_ptr<IConversation> polychatConv;
				if (itr == conversations.end()) {
					// TODO: Differentiate channel types
					polychatConv = coreAccount.loadConversation(roomPair.first,
						Polychat::CONVERSATION_TYPE::PUBLIC_CHANNEL, roomPair.second->name);
				} else {
					polychatConv = itr->second;
				}
				for (LibMatrix::Message msg : roomPair.second->messages) {
					std::shared_ptr<Polychat::Message> polychatMessage = std::make_shared<Polychat::Message>();
					polychatMessage->id = msg.id;
					polychatMessage->sendStatus = Polychat::SendStatus::SENT;
					polychatMessage->channelId = roomPair.first;
					polychatMessage->msgContent = msg.content;
					polychatMessage->uid = msg.sender;
					polychatMessage->createdAt = msg.timestamp;
					polychatConv->processMessage(polychatMessage);
				}

			}
		} catch (...) {

		}
		doSync();
	};
	core.getScheduler().runOnReady(std::make_unique<FutureTask<LibMatrix::RoomMap>>(backendSession.syncState(), callback));
}

void MatrixAccountSession::refresh(std::shared_ptr<IConversation> currentlyViewedConversation) {
	// TODO: Now is when the rooms are loaded
	if (!isSyncing) {
		doSync();
	}
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
		message->sendStatus = SendStatus::SENDING;
		std::function<void(std::future<void>&)> callback = [message, this](std::future<void>& result) {
			try {
				result.get();
				message->sendStatus = SendStatus::SENT;
			} catch (...) {
				message->sendStatus = SendStatus::FAILED;
			}
		};
		core.getScheduler().runOnReady(std::make_unique<FutureTask<void>>(
			backendSession.sendMessage(message->channelId, message->msgContent), callback));
	}
}
