#include "PolyChatHTTPClient.h"


using LibMatrix::PolyChatHTTPClient;

PolyChatHTTPClient::PolyChatHTTPClient(std::shared_ptr<Polychat::IHTTPClient> client)
	: client(client)
{}

Polychat::HTTPMethod getPolyChatMethodFromMethod(LibMatrix::HTTPMethod method) {
	switch (method) {
	case LibMatrix::HTTPMethod::CONNECT:
		return Polychat::HTTPMethod::CONNECT;
	case LibMatrix::HTTPMethod::DELETE:
		return Polychat::HTTPMethod::DELETE;
	case LibMatrix::HTTPMethod::GET:
		return Polychat::HTTPMethod::GET;
	case LibMatrix::HTTPMethod::HEAD:
		return Polychat::HTTPMethod::HEAD;
	case LibMatrix::HTTPMethod::OPTIONS:
		return Polychat::HTTPMethod::OPTIONS;
	case LibMatrix::HTTPMethod::POST:
		return Polychat::HTTPMethod::POST;
	case LibMatrix::HTTPMethod::PUT:
		return Polychat::HTTPMethod::PUT;
	case LibMatrix::HTTPMethod::TRACE:
		return Polychat::HTTPMethod::TRACE;
	case LibMatrix::HTTPMethod::PATCH:
		return Polychat::HTTPMethod::PATCH;
	}
	throw std::runtime_error("Unknown HTTP Method");
}

LibMatrix::HTTPStatus getStatusFromPolyChatStatus(Polychat::HTTPStatus status) {
	return static_cast<LibMatrix::HTTPStatus>(status);
}

void PolyChatHTTPClient::request(std::shared_ptr<HTTPRequestData> data) {
	Polychat::HTTPMessage newMsg(getPolyChatMethodFromMethod(data->getMethod()), data->getSubPath());
	newMsg.setContent(std::make_shared<Polychat::HTTPStringContent>(data->getBody()));
	if (data->getHeaders()) {
		auto itr = data->getHeaders()->cbegin();
		while (itr != data->getHeaders()->cend()) {
			newMsg[itr->first] = itr->second;
			itr++;
		}
	}
	client->sendRequest(newMsg, [data](std::shared_ptr<Polychat::HTTPMessage> responseMsg) {
		Response responseWrapped(getStatusFromPolyChatStatus(responseMsg->getStatus()),
			responseMsg->getContent()->getAsString());
		auto itr = responseMsg->cbegin();
		while (itr != responseMsg->cend()) {
			responseWrapped.headers[itr->first] = itr->second;
			itr++;
		}

		data->getResponseCallback()(responseWrapped);
	});
}