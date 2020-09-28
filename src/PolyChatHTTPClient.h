#ifndef POLYCHAT_HTTP_SESSION_H
#define POLYCHAT_HTTP_SESSION_H

#include <libmatrix-client/HTTP.h>
#include "include/IWebHelper.h"

namespace LibMatrix {

class PolyChatHTTPClient : public HTTPClientBase {
private:
	std::shared_ptr<Polychat::IHTTPClient> client;
public:
	PolyChatHTTPClient(std::shared_ptr<Polychat::IHTTPClient> client);
	virtual void request(std::shared_ptr<HTTPRequestData> method);
};

}

#endif