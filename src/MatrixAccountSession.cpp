#include "MatrixAccountSession.h"
#include "include/ITeam.h"
#include "PolyTrix.h"
#include "include/Message.h"
#include <chrono>
#include <memory>
#include <variant>
#include <functional>

#include "include/ICore.h"
#include "include/IAccount.h"
#include "include/IAccountManager.h"

#include <mtx.hpp>
#include <mtxclient/http/errors.hpp>
#include <mtx/events/collections.hpp>
#include <nlohmann/json.hpp>


using namespace PolyTrixPlugin;
using RoomNameStateEvent = mtx::events::StateEvent<mtx::events::state::Name>;
using RoomMessageEvent = mtx::events::RoomEvent<mtx::events::msg::Text>;

MatrixAccountSession::MatrixAccountSession(PolyTrix& plugin, Polychat::IAccount& coreAccount,
	Polychat::ICore& core, std::string serverAddr, std::string name, std::string password)
	: plugin(plugin), coreAccount(coreAccount), core(core)
{
	olmAccountFilePath = std::filesystem::path(plugin.getFolderPath()).append(name + "_account.json").u8string();
	client = std::make_shared<mtx::http::Client>(serverAddr);
	loadOlm();

	client->login(name, password, std::bind(&MatrixAccountSession::onLogin,
		this, std::placeholders::_1, std::placeholders::_2)	);
}

void MatrixAccountSession::loadOlm() {
	olmClient = std::make_shared<mtx::crypto::OlmClient>();

	std::ifstream db(olmAccountFilePath);
	std::string db_data((std::istreambuf_iterator<char>(db)), std::istreambuf_iterator<char>());

	if (db_data.empty())
		olmClient->create_new_account();
	else
		olmClient->load(json::parse(db_data).at("account").get<std::string>(),
			STORAGE_KEY);

	storage.logger = &core.getLogger();
	storage.load();

}


void MatrixAccountSession::shutdownSave()
{
	storage.logger->info("Saving OLM account data");
	storage.save();

	std::ofstream db(olmAccountFilePath);
	if (!db.is_open()) {
		storage.logger->critical(std::string("couldn't open file ").append(olmAccountFilePath).append(" to save account keys"));
		return;
	}

	json data;
	data["account"] = olmClient->save(STORAGE_KEY);

	db << data.dump(2);
	db.close();

	// The sync calls will stop.
	client->shutdown();
}

// Utility functions

std::string findRoomName(const mtx::responses::JoinedRoom& room) {
	const auto& events = room.state.events;
	for (auto i = events.rbegin(); i != events.rend(); ++i) {
		if (auto nameEvent = std::get_if<RoomNameStateEvent>(&(*i))) {
			return nameEvent->content.name;
		}
	}

	return "Unknown Room Name";
}

template<class T>
bool
is_member_event(const T& event)
{
	using namespace mtx::events;
	using namespace mtx::events::state;
	return std::holds_alternative<StateEvent<Member>>(event);
}

template<class T>
bool
is_room_encryption(const T& event)
{
	using namespace mtx::events;
	using namespace mtx::events::state;
	return std::holds_alternative<StateEvent<Encryption>>(event);
}

bool
is_encrypted(const TimelineEvent& event)
{
	using namespace mtx::events;
	return std::holds_alternative<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(event);
}

template<class T>
std::string
get_json(const T& event)
{
	return std::visit([](auto e) { return json(e).dump(2); }, event);
}

// End of utility functions


void MatrixAccountSession::send_encrypted_reply(const std::string& room_id,
	const std::string& reply_msg, mtx::http::Callback<mtx::responses::EventId> cb)
{
	storage.logger->info("sending reply");

	// Create a megolm session if it doesn't already exist.
	if (storage.outbound_group_exists(room_id)) {
		auto session_obj = storage.get_outbound_group_session(room_id);

		send_group_message(
			session_obj.session, session_obj.data.session_id, room_id, reply_msg, cb);

	}
	else {
		storage.logger->info("creating new megolm outbound session");
		create_outbound_megolm_session(room_id, reply_msg, cb);
	}
}

void MatrixAccountSession::send_group_message(OlmOutboundGroupSession* session,
	const std::string& session_id,
	const std::string& room_id,
	const std::string& msg,
	mtx::http::Callback<mtx::responses::EventId> cb)
{
	// Create event payload
	json doc{ {"type", "m.room.message"},
			 {"content", {{"type", "m.text"}, {"body", msg}}},
			 {"room_id", room_id} };

	auto payload = olmClient->encrypt_group_message(session, doc.dump());

	using namespace mtx::events;
	using namespace mtx::identifiers;

	msg::Encrypted data;
	data.algorithm = "m.megolm.v1.aes-sha2";
	data.ciphertext = std::string((char*)payload.data(), payload.size());
	data.sender_key = olmClient->identity_keys().curve25519;
	data.session_id = session_id;
	data.device_id = client->device_id();

	client->send_room_message<msg::Encrypted>(
		room_id, data, [this](const mtx::responses::EventId& res, mtx::http::RequestErr err) {
			if (err) {
				storage.logger->critical("Error in send_room_message callback");
				print_errors(err);
				return;
			}

			storage.logger->info(std::string("message sent with event_id: ").append(res.event_id.to_string()));
		});
}


void MatrixAccountSession::create_outbound_megolm_session(
	const std::string& room_id, const std::string& reply_msg,
	mtx::http::Callback<mtx::responses::EventId> cb)
{
	// Create an outbound session
	auto outbound_session = olmClient->init_outbound_group_session();

	const auto session_id = mtx::crypto::session_id(outbound_session.get());
	const auto session_key = mtx::crypto::session_key(outbound_session.get());

	mtx::events::DeviceEvent<mtx::events::msg::RoomKey> megolm_payload;
	megolm_payload.content.algorithm = "m.megolm.v1.aes-sha2";
	megolm_payload.content.room_id = room_id;
	megolm_payload.content.session_id = session_id;
	megolm_payload.content.session_key = session_key;
	megolm_payload.type = mtx::events::EventType::RoomKey;

	if (storage.members.find(room_id) == storage.members.end()) {
		storage.logger->critical(std::string("no members found for room ").append(room_id));
		return;
	}

	const auto members = storage.members[room_id];

	for (const auto& member : members) {
		const auto devices = storage.devices[member.first];

		if (devices.empty()) {
			storage.logger->warning(std::string("Device list empty for user ").append(member.first)
				.append(". They will not receive the keys to decrypt the sessions's messages."));
		}

		// TODO: Figure out for which devices we don't have olm sessions.
		for (const auto& dev : devices) {
			// TODO: check if we have downloaded the keys
			const auto device_keys = storage.device_keys[dev];

			auto to_device_cb = [this](mtx::http::RequestErr err) {
				if (err) {
					storage.logger->critical("Error in to_device callback in create_outbound_megolm_session");
					print_errors(err);
				}
			};

			if (storage.olm_outbound_sessions.find(device_keys.curve25519) !=
				storage.olm_outbound_sessions.end()) {
				storage.logger->info(
					std::string("found existing olm outbound session with device ").append(dev));
				auto olm_session =
					storage.olm_outbound_sessions[device_keys.curve25519].get();

				auto device_msg =
					olmClient->create_olm_encrypted_content(olm_session,
						megolm_payload,
						UserId(member.first),
						device_keys.ed25519,
						device_keys.curve25519);

				json body{ {"messages", {{member, {{dev, device_msg}}}}} };

				client->send_to_device("m.room.encrypted", body, to_device_cb);
				// TODO: send message to device
			}
			else {
				storage.logger->info(std::string("claiming one time keys for device ").append(dev));
				auto cb = [member = member.first,
					dev,
					megolm_payload,
					to_device_cb,
					this] (const mtx::responses::ClaimKeys& res,
						mtx::http::RequestErr err) {
					if (err) {
						storage.logger->critical("Error in callback for one time keys in create_outbound_megolm_session");
						print_errors(err);
						return;
					}

					storage.logger->info(std::string("claimed keys for ").append(member).append(" - ").append(dev));
					storage.logger->info(std::string("room_key ").append(json(megolm_payload).dump(4)));

					storage.logger->warning("signed one time keys");
					auto retrieved_devices = res.one_time_keys.at(member);
					for (const auto& rd : retrieved_devices) {
						storage.logger->info(
							std::string().append(rd.first).append(" : \n").append(rd.second.dump(2)));

						// TODO: Verify signatures
						auto otk = rd.second.begin()->at("key");
						auto id_key = storage.device_keys[dev].curve25519;

						auto session =
							olmClient->create_outbound_session(id_key, otk);

						auto device_msg =
							olmClient->create_olm_encrypted_content(
								session.get(),
								megolm_payload,
								UserId(member),
								storage.device_keys[dev].ed25519,
								storage.device_keys[dev].curve25519);

						// TODO: saving should happen when the message is
						// sent.
						storage.olm_outbound_sessions[id_key] =
							std::move(session);

						json body{
						  {"messages", {{member, {{dev, device_msg}}}}} };

						client->send_to_device(
							"m.room.encrypted", body, to_device_cb);
					}
				};

				mtx::requests::ClaimKeys claim_keys;
				claim_keys.one_time_keys[member.first][dev] = mtx::crypto::SIGNED_CURVE25519;

				// TODO: we should bulk request device keys here
				client->claim_keys(claim_keys, cb);
			}
		}
	}

	storage.logger->info("waiting to send sendToDevice messages");
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	storage.logger->info("sending encrypted group message");

	// TODO: This should be done after all sendToDevice messages have been sent.
	send_group_message(outbound_session.get(), session_id, room_id, reply_msg, cb);

	// TODO: save message index also.
	storage.set_outbound_group_session(
		room_id, std::move(outbound_session), { session_id, session_key });
}

void MatrixAccountSession::onKeysUploaded(const mtx::responses::UploadKeys&, mtx::http::RequestErr err)
{
	if (err) {
		storage.logger->critical("Error in onKeysUploaded");
		print_errors(err);
		return;
	}

	olmClient->mark_keys_as_published();
	storage.logger->info("keys uploaded");
}

void MatrixAccountSession::onLogin(const mtx::responses::Login& res, mtx::http::RequestErr err) {
	if (err) {
		storage.logger->critical("Error logging in");
		print_errors(err);
		return;
	}

	// Upload one time keys.
	olmClient->set_user_id(client->user_id().to_string());
	olmClient->set_device_id(client->device_id());
	olmClient->generate_one_time_keys(50);

	client->upload_keys(olmClient->create_upload_keys_request(),
		[this](const mtx::responses::UploadKeys&, mtx::http::RequestErr err) {
			if (err) {
				storage.logger->critical("Error in upload_keys callback in onLogin");
				print_errors(err);
				return;
			}

			olmClient->mark_keys_as_published();
			storage.logger->info("keys uploaded");
			storage.logger->debug("starting initial sync");

			mtx::http::SyncOpts opts;
			opts.timeout = 0;

			// Initial sync after keys are uploaded
			client->sync(opts, std::bind(&MatrixAccountSession::onSync,
				this, std::placeholders::_1, std::placeholders::_2));
		});

	client->set_access_token(res.access_token);
	mtx::http::SyncOpts opts;
	
	return;
}

void MatrixAccountSession::onSync(const mtx::responses::Sync sync, mtx::http::RequestErr err) {
	mtx::http::SyncOpts opts;

	if (err) {
		storage.logger->critical("Error syncing state ");
		print_errors(err);
		// Run again with the previous batch token
		opts.since = client->next_batch_token();
		client->sync(opts, std::bind(&MatrixAccountSession::onSync,
			this, std::placeholders::_1, std::placeholders::_2));
		return;
	}

	parseSyncMessages(sync);
	
	opts.since = sync.next_batch;
	client->set_next_batch_token(sync.next_batch);
	if (plugin.connectionsActive()) {
		client->sync(opts, std::bind(&MatrixAccountSession::onSync,
			this, std::placeholders::_1, std::placeholders::_2));
	}
}
void MatrixAccountSession::onInitialSync(const mtx::responses::Sync sync, mtx::http::RequestErr err) {
	mtx::http::SyncOpts opts;

	if (err) {
		storage.logger->critical("Error during initial sync");
		print_errors(err);
		
		if (err->status_code != 200) {
			storage.logger->warning("retrying initial sync ..");
			opts.timeout = 0;
			client->sync(opts, std::bind(&MatrixAccountSession::onInitialSync,
				this, std::placeholders::_1, std::placeholders::_2));
		}
		return;
	}

	parseSyncMessages(sync);

	opts.since = sync.next_batch;
	client->set_next_batch_token(sync.next_batch);
	if (plugin.connectionsActive()) {
		client->sync(opts, std::bind(&MatrixAccountSession::onSync,
			this, std::placeholders::_1, std::placeholders::_2));
	}
}

void MatrixAccountSession::parseSyncMessages(const mtx::responses::Sync sync) {
	// TODO: Handle room invitations

	// Check if we have any new m.room_key messages (i.e starting a new megolm session)
	handle_to_device_msgs(sync.to_device);

	// Check if the uploaded one time keys are enough
	for (const auto& device : sync.device_one_time_keys_count) {
		if (device.second < 50) {
			storage.logger->info(std::string("number of one time keys: ")
				.append(std::to_string(device.second)));
			olmClient->generate_one_time_keys(50 - device.second);
			// TODO: Mark keys as sent
			client->upload_keys(olmClient->create_upload_keys_request(),
				std::bind(&MatrixAccountSession::onKeysUploaded, this, std::placeholders::_1, std::placeholders::_2));
		}
	}

	// Process for each room
	const std::map<std::string, std::shared_ptr<IConversation>>& conversations
		= coreAccount.getConversations();
	for (auto joinedRoom : sync.rooms.join) {
		std::string roomID = joinedRoom.first;
		// Determine if the conversation even exists in PolyChat
		// Add it if it doesn't.
		auto itr = conversations.find(roomID);

		std::shared_ptr<IConversation> polychatConv;
		if (itr == conversations.end()) {

			// TODO: Differentiate channel types
			polychatConv = coreAccount.loadConversation(roomID,
				Polychat::CONVERSATION_TYPE::PUBLIC_CHANNEL, findRoomName(joinedRoom.second));
		}
		else {
			polychatConv = itr->second;
		}

		// State events
		for (const auto& event : joinedRoom.second.state.events) {
			if (is_room_encryption(event)) {
				mark_encrypted_room(RoomId(roomID));
				storage.logger->debug(get_json(event));
			} else if (is_member_event(event)) {
				auto m =
					std::get<mtx::events::StateEvent<mtx::events::state::Member>>(event);

				get_device_keys(UserId(m.state_key));
				storage.add_member(roomID, m.state_key);
			}
		}

		// Timeline events
		for (auto event : joinedRoom.second.timeline.events) {
			if (is_encrypted(event)) {
				storage.logger->info(std::string("received an encrypted event: ").append(roomID));
				storage.logger->info(get_json(event));
				event = parseEncryptedEvent(roomID,
					std::get<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(event));
			}

			if (is_room_encryption(event)) {
				mark_encrypted_room(RoomId(roomID));
				storage.logger->debug(get_json(event));
			}
			else if (is_member_event(event)) {
				auto m =
					std::get<mtx::events::StateEvent<mtx::events::state::Member>>(event);

				get_device_keys(UserId(m.state_key));
				storage.add_member(roomID, m.state_key);
				
			} else if (auto msgEvent = std::get_if<RoomMessageEvent>(&(event))) {
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
}

TimelineEvent MatrixAccountSession::parseEncryptedEvent(std::string roomID, EncryptedEvent encryptedEvent) {

	if (storage.inbound_group_exists(
		roomID, encryptedEvent.content.session_id, encryptedEvent.content.sender_key)) {
		auto res = olmClient->decrypt_group_message(
			storage.get_inbound_group_session(roomID,
				encryptedEvent.content.session_id,
				encryptedEvent.content.sender_key),
			encryptedEvent.content.ciphertext);

		auto msg_str =
			std::string((char*)res.data.data(), res.data.size());
		const auto decryptedBody = json::parse(msg_str)
			.at("content")
			.at("body")
			.get<std::string>();

		json body = json::parse(msg_str);
		body["event_id"] = encryptedEvent.event_id;
		body["sender"] = encryptedEvent.sender;
		body["origin_server_ts"] = encryptedEvent.origin_server_ts;
		body["unsigned"] = encryptedEvent.unsigned_data;

		// relations are unencrypted in content...
		mtx::common::add_relations(body["content"], encryptedEvent.content.relations);

		mtx::events::collections::TimelineEvent te;
		mtx::events::collections::from_json(body, te);

		return te.data;
	}
	else {
		storage.logger->warning(
			"no megolm session found to decrypt the event");
		return encryptedEvent;
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
		if (storage.encrypted_rooms[message->channelId]) {
			send_encrypted_reply(message->channelId, message->msgContent, [message, this](const mtx::responses::EventId& res, mtx::http::RequestErr err) {
				if (err) {
					storage.logger->critical("Failed to send message");
					print_errors(err);
					message->sendStatus = SendStatus::FAILED;
				}
				else {
					message->sendStatus = SendStatus::SENT;
				}
			});
		} else {
			mtx::events::msg::Text payload;
			payload.body = message->msgContent;
			client->send_room_message(message->channelId, payload, [message, this](const mtx::responses::EventId& res, mtx::http::RequestErr err) {
				if (err) {
					storage.logger->critical("Failed to send message");
					print_errors(err);
					message->sendStatus = SendStatus::FAILED;
				}
				else {
					message->sendStatus = SendStatus::SENT;
				}
			});
		}
		break;
	}
}

void
MatrixAccountSession::save_device_keys(const mtx::responses::QueryKeys& res)
{
	for (const auto& entry : res.device_keys) {
		const auto user_id = entry.first;

		if (!exists(storage.devices, user_id))
			storage.logger->info(std::string("keys for user ").append(user_id));

		std::vector<std::string> device_list;
		for (const auto& device : entry.second) {
			const auto key_struct = device.second;

			const std::string device_id = key_struct.device_id;
			const std::string index = "curve25519:" + device_id;

			if (key_struct.keys.find(index) == key_struct.keys.end())
				continue;

			const auto key = key_struct.keys.at(index);

			if (!exists(storage.device_keys, device_id)) {
				storage.logger->info(std::string(device_id).append(" -> ").append(key));
				storage.device_keys[device_id] = {
					key_struct.keys.at("ed25519:" + device_id),
					key_struct.keys.at("curve25519:" + device_id) };
			}

			device_list.push_back(device_id);
		}

		if (!exists(storage.devices, user_id)) {
			storage.devices[user_id] = device_list;
		}
	}
}

void
MatrixAccountSession::get_device_keys(const UserId& user)
{
	// Retrieve all devices keys.
	mtx::requests::QueryKeys query;
	query.device_keys[user.get()] = {};

	client->query_keys(query, [this, user](const mtx::responses::QueryKeys& res, mtx::http::RequestErr err) {
		if (err) {
			storage.logger->critical(std::string("Error in get_device_keys for user ").append(user));
			print_errors(err);
			return;
		}

		for (const auto& key : res.device_keys) {
			const auto user_id = key.first;
			const auto devices = key.second;

			for (const auto& device : devices) {
				const auto id = device.first;
				const auto data = device.second;

				try {
					auto ok = verify_identity_signature(
						json(data), DeviceId(id), UserId(user_id));

					if (!ok) {
						storage.logger->warning("signature could not be verified");
						storage.logger->warning(json(data).dump(2));
					}
				}
				catch (const mtx::crypto::olm_exception& e) {
					storage.logger->warning(e.what());
				}
				catch (...) {
					storage.logger->critical("Exception while verifying identity signature");
				}
			}
		}

		save_device_keys(res);
	});
}

void
MatrixAccountSession::handle_to_device_msgs(const mtx::responses::ToDevice& msgs)
{
	if (!msgs.events.empty())
		storage.logger->info(std::string("inspecting")\
			.append(std::to_string(msgs.events.size())).append("to_device messages"));

	for (const auto& msg : msgs.events) {
		storage.logger->info(std::visit([](const auto& e) { return json(e); }, msg).dump(2));

		try {
			OlmMessage olm_msg = std::visit([](const auto& e) { return json(e); }, msg);
			decrypt_olm_message(std::move(olm_msg));
		}
		catch (const nlohmann::json::exception& e) {
			storage.logger->warning(std::string("parsing error for olm message: ").append(e.what()));
		}
		catch (const std::invalid_argument& e) {
			storage.logger->warning(std::string("validation error for olm message: ").append(e.what()));
		}
	}
}

void
MatrixAccountSession::mark_encrypted_room(const RoomId& id)
{
	storage.logger->info(std::string("encryption is enabled for room: ").append(id.get()));
	storage.encrypted_rooms[id.get()] = true;
}

void
MatrixAccountSession::print_errors(mtx::http::RequestErr err)
{
	if (err->status_code)
		storage.logger->critical(std::to_string(err->status_code));
	if (!err->matrix_error.error.empty()) {
		storage.logger->critical(err->matrix_error.error);
		storage.logger->critical(std::to_string(static_cast<int>(err->matrix_error.errcode)));
	}
	if (err->error_code)
		storage.logger->critical(std::to_string(err->error_code));
}


void MatrixAccountSession::decrypt_olm_message(const OlmMessage& olm_msg)
{
	storage.logger->info("OLM message");
	storage.logger->info(std::string("sender: ").append(olm_msg.sender));
	storage.logger->info(std::string("sender_key: ").append(olm_msg.sender_key));

	const auto my_id_key = olmClient->identity_keys().curve25519;
	for (const auto& cipher : olm_msg.ciphertext) {
		if (cipher.first == my_id_key) {
			const auto msg_body = cipher.second.body;
			const auto msg_type = cipher.second.type;

			storage.logger->info("the message is meant for us");
			storage.logger->info(std::string("body: ").append(msg_body));
			storage.logger->info(std::string("type: ").append(std::to_string(msg_type)));

			if (msg_type == 0) {
				storage.logger->info(std::string("opening session with ").append(olm_msg.sender));
				auto inbound_session = olmClient->create_inbound_session(msg_body);

				auto ok = mtx::crypto::matches_inbound_session_from(
					inbound_session.get(), olm_msg.sender_key, msg_body);

				if (!ok) {
					storage.logger->critical("session could not be established");

				}
				else {
					auto output = olmClient->decrypt_message(
						inbound_session.get(), msg_type, msg_body);

					auto plaintext = json::parse(
						std::string((char*)output.data(), output.size()));
					storage.logger->info(std::string("decrypted message: \n ").append(plaintext.dump(2)));

					storage.olm_inbound_sessions.emplace(
						olm_msg.sender_key, std::move(inbound_session));

					std::string room_id = plaintext.at("content").at("room_id");
					std::string session_id =
						plaintext.at("content").at("session_id");
					std::string session_key =
						plaintext.at("content").at("session_key");

					if (storage.inbound_group_exists(
						room_id, session_id, olm_msg.sender_key)) {
						storage.logger->warning("megolm session already exists");
					}
					else {
						auto megolm_session =
							olmClient->init_inbound_group_session(
								session_key);

						storage.set_inbound_group_session(
							room_id,
							session_id,
							olm_msg.sender_key,
							std::move(megolm_session));

						storage.logger->info("megolm_session saved");
					}
				}
			}
		}
	}
}
