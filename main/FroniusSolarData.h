#ifndef MAIN_FRONIUSSOLARDATA_H_
#define MAIN_FRONIUSSOLARDATA_H_

#include "WebClient.h"
#include "Url.h"
#include "Wifi.h"
#include "DisplayCharter.h"
#include "Config.h"
#include "String.h"
#include <cJSON.h>


class Ufo;

class FroniusSolarData {

public:

    FroniusSolarData();
	virtual ~FroniusSolarData();
    
    void Init(Ufo* pUfo, DisplayCharter* pDisplayLowerRing, DisplayCharter* pDisplayUpperRing);
    void ProcessConfigChange();
    void Run(__uint8_t uTaskId);
    bool IsActive() { return mEnabled; };

private:

    void GetData();
    void Process(String& jsonString);
    void DisplayDefault();
    void HandleFailure();

    WebClient  solarClient;

    Ufo* mpUfo;  
    DisplayCharter* mpDisplayLowerRing;
    DisplayCharter* mpDisplayUpperRing;
    Config* mpConfig;
//	Wifi* mpWifi;
    
    Url mSolarUrl;
    String mSolarUrlString;
    
    bool mInitialized = false;
    bool mEnabled;
    __uint8_t mActTaskId; 
    __uint8_t mActConfigRevision;

    int miSOC;
    double mdPV;
};

#endif