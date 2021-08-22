#include "PolyTrix.h"
#include "MatrixAccountSession.h"

#include "include/IAccountManager.h"

#include <functional>
#include <string>
#include <memory>

using namespace Polychat;

using namespace PolyTrixPlugin;

std::string PolyTrix::getPluginName() const {
	return "PolyTrix";
}
std::string PolyTrix::getProtocolName() const {
	return "Matrix";
}

PolyTrix::PolyTrix() {
	loginFieldsList.push_back(LoginField("address", true, true, false));
//	loginFieldsList.push_back(LoginField("email", true, true, false));
	loginFieldsList.push_back(LoginField("username", true, true, false));
	loginFieldsList.push_back(LoginField("password", true, false, true));
}

PolyTrix::~PolyTrix() {

}

bool PolyTrix::initialize(ICore& core) {
	this->core = &core;

	return true;
}

void PolyTrix::shutdown(ICore& core) {
	std::filesystem::create_directory(getFolderPath());
	for (auto session : sessions) {
		session.second->shutdownSave();
	}
}

std::string PolyTrix::getDatabaseName() const {
	return "PolyTrix";
}

AuthStatus PolyTrix::login(std::map<std::string, std::string> fields, IAccount& account) {
	std::string& name = fields["username"];
	auto nameFindResult = sessions.find(name);
	if (nameFindResult != sessions.end()) {
		core->alert("That session already exists.");
		return AuthStatus::FAIL_OTHER;
	} else {
		// TODO: Allow passwordless login from cached account
		std::shared_ptr<MatrixAccountSession> newSession = std::make_shared<MatrixAccountSession>(*this, account, *core, fields["address"], name, fields["password"]);
		account.setSession(newSession);
		account.setUsername(name);
		sessions[name] = newSession;
		return AuthStatus::CONNECTING;
	}
}

std::unordered_map<std::string, std::shared_ptr<MatrixAccountSession>>& PolyTrix::getSessions() {
	return sessions;
}

std::filesystem::path PolyTrix::getFolderPath() {
	std::filesystem::path path = core->getFileUtil().getPluginsFolderPath();
	path.append("polytrix");
	return path;
}

#ifdef POLYTRIX_SHARED
extern "C" {
#ifdef _WIN32
	__declspec(dllexport) PolyTrix* create()
	{
		return new PolyTrix;
	}

	__declspec(dllexport) void destroy(PolyTrix * in)
	{
		delete in;
	}
#else
	PolyTrix* create()
	{
		return new PolyTrix;
	}

	void destroy(PolyTrix* in)
	{
		delete in;
	}
#endif
}
#endif
