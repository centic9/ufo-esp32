#ifndef MAIN_DYNAMICREQUESTHANDLER_H_
#define MAIN_DYNAMICREQUESTHANDLER_H_

#include "UrlParser.h"
#include "HttpResponse.h"
#include <list>

class Ufo;
class DisplayCharter;

class DynamicRequestHandler {
public:
	DynamicRequestHandler(Ufo* pUfo, DisplayCharter* pDCLevel1, DisplayCharter* pDCLevel2);
	virtual ~DynamicRequestHandler();

	bool HandleApiRequest(std::list<TParam>& params, HttpResponse& rResponse);
	bool HandleApiListRequest(std::list<TParam>& params, HttpResponse& rResponse);
	bool HandleApiEditRequest(std::list<TParam>& params, HttpResponse& rResponse);
	bool HandleInfoRequest(std::list<TParam>& params, HttpResponse& rResponse);
	bool HandleConfigRequest(std::list<TParam>& params, HttpResponse& rResponse);
	bool HandleSrvConfigRequest(std::list<TParam>& params, HttpResponse& rResponse);
	bool HandleFirmwareRequest(std::list<TParam>& params, HttpResponse& response);
	bool HandleCheckFirmwareRequest(std::list<TParam>& params, HttpResponse& response);
	bool HandleDynatraceIntegrationRequest(std::list<TParam>& params, HttpResponse& response);
	bool HandleFroniusSolarDataRequest(std::list<TParam>& params, HttpResponse& response);
	bool HandleDynatraceMonitoringRequest(std::list<TParam>& params, HttpResponse& response);

	bool ShouldRestart() { return mbRestart; }

private:
	Ufo* mpUfo;
	DisplayCharter* mpDisplayCharterLevel1;
	DisplayCharter* mpDisplayCharterLevel2;

	bool mbRestart;
};

#endif /* MAIN_DYNAMICREQUESTHANDLER_H_ */
