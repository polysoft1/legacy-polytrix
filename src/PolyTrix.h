#include "include/IProtocolPlugin.h"
#include "include/IPlugin.h"
#include "include/ICore.h"
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <map>


using namespace Polychat;

namespace PolyTrixPlugin {

class MatrixAccountSession;

class PolyTrix : public IProtocolPlugin {
public:
	PolyTrix();

	~PolyTrix();

	virtual std::string getPluginName() const;
	virtual std::string getProtocolName() const;

	virtual bool initialize(ICore* core);

	virtual std::string getDatabaseName() const;

	virtual AuthStatus login(std::map<std::string, std::string> fields, IAccount& login);

	virtual const std::vector<LoginField>& loginFields() const { return loginFieldsList; };

	virtual bool connectionsActive() {
		return makingConnections;
	}

	virtual bool startConnections() {
		makingConnections = true;
		return true;
	};

	virtual bool stopConnections() {
		makingConnections = false;
		return true;
	};

	virtual bool usesTeams() { return false; };

	std::unordered_map<std::string, std::shared_ptr<MatrixAccountSession>>& getSessions();

private:
	ICore* core = nullptr;
	std::vector<LoginField> loginFieldsList;
	std::unordered_map<std::string, std::shared_ptr<MatrixAccountSession>> sessions;
	bool makingConnections = true;
};

}
