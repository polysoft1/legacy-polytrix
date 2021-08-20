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

#include <mtx.hpp>
#include <mtxclient/http/errors.hpp>
#include <mtx/events/collections.hpp>

using namespace PolyTrixPlugin;
using RoomNameStateEvent = mtx::events::StateEvent<mtx::events::state::Name>;
using RoomMessageEvent = mtx::events::RoomEvent<mtx::events::msg::Text>;

MatrixAccountSession::MatrixAccountSession(PolyTrix& plugin, Polychat::IAccount& coreAccount,
	Polychat::ICore& core, std::string serverAddr, std::string name, std::string password)
	: plugin(plugin), coreAccount(coreAccount), core(core)
{
	client = std::make_shared<mtx::http::Client>(serverAddr);
	client->login(name, password, [this](const mtx::responses::Login& res, mtx::http::RequestErr err)
		{
			if (err) {
				std::cerr << "Error logging in!\n";
				return;
			}

			client->set_access_token(res.access_token);
			mtx::http::SyncOpts opts;
			client->sync(opts, [this](const mtx::responses::Sync sync, mtx::http::RequestErr err) {
				onSync(sync, err);
			});
			return;
		}
	);
}

std::string findRoomName(const mtx::responses::JoinedRoom& room) {
	const auto& events = room.state.events;
	for (auto i = events.rbegin(); i != events.rend(); ++i) {
		if (auto nameEvent = std::get_if<RoomNameStateEvent>(&(*i))) {
			return nameEvent->content.name;
		}
	}

	return "Unknown Room Name";
}

void MatrixAccountSession::onSync(const mtx::responses::Sync sync, mtx::http::RequestErr err) {
	if (err) {
		std::cout << "Error syncing state\n";
		return;
	}


	const std::map<std::string, std::shared_ptr<IConversation>>& conversations
		= coreAccount.getConversations();
	for (auto joinedRoom : sync.rooms.join) {
		// Determine if the conversation even exists in PolyChat
		auto itr = conversations.find(joinedRoom.first);

		std::shared_ptr<IConversation> polychatConv;
		if (itr == conversations.end()) {

			// TODO: Differentiate channel types
			polychatConv = coreAccount.loadConversation(joinedRoom.first,
				Polychat::CONVERSATION_TYPE::PUBLIC_CHANNEL, findRoomName(joinedRoom.second));
		} else {
			polychatConv = itr->second;
		}

		for (auto event : joinedRoom.second.timeline.events) {
			if (auto msgEvent = std::get_if<RoomMessageEvent>(&(event))) {
				std::shared_ptr<Polychat::Message> polychatMessage = std::make_shared<Polychat::Message>();
				polychatMessage->id = msgEvent->event_id;
				polychatMessage->sendStatus = Polychat::SendStatus::SENT;
				polychatMessage->channelId = msgEvent->room_id;
				polychatMessage->msgContent = msgEvent->content.body; // Note, there is more content than the body
				polychatMessage->uid = msgEvent->sender;
				polychatMessage->createdAt = msgEvent->origin_server_ts;
				polychatConv->processMessage(polychatMessage);
			}
		}
	}
	mtx::http::SyncOpts opts;
	opts.since = sync.next_batch;
	client->set_next_batch_token(sync.next_batch);
	if (plugin.connectionsActive()) {
		client->sync(opts, [this](const mtx::responses::Sync sync, mtx::http::RequestErr err) {
			onSync(sync, err);
		});
	}
}

void MatrixAccountSession::doSync() {
	
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
		mtx::events::msg::Text payload;
		payload.body = message->msgContent;
		client->send_room_message(message->channelId, payload, [message, this](const mtx::responses::EventId& res, mtx::http::RequestErr err) {
			if (err) {
				message->sendStatus = SendStatus::FAILED;
			} else {
				message->sendStatus = SendStatus::SENT;
			}
		});
		break;
	}
}
