#ifndef MAIN_FRONIUSSOLARDATA_H_
#define MAIN_FRONIUSSOLARDATA_H_

#include "WebClient.h"
#include "Url.h"
#include "DisplayCharter.h"
#include "DisplayCharterLogo.h"
#include "Config.h"
#include "String.h"


class Ufo;

class FroniusSolarData {

public:

    FroniusSolarData();
	virtual ~FroniusSolarData();
    
    void Init(Ufo* pUfo, DisplayCharter* pDisplayLowerRing, DisplayCharter* pDisplayUpperRing,
            DisplayCharterLogo* pDisplayCharterLogo);
    void ProcessConfigChange();
    void Run(__uint8_t uTaskId);
    bool IsActive() { return mEnabled; };

private:

    void GetData();
    void SendDataToDynatrace();
    void CreateDynatraceDeviceAndMetric();
    void Process(String& jsonString);
    void DisplayDefault();
    void HandleFailure();

    WebClient  solarClient;

    Ufo* mpUfo;  
    DisplayCharter* mpDisplayLowerRing;
    DisplayCharter* mpDisplayUpperRing;
    DisplayCharterLogo* mpDisplayCharterLogo;
    Config* mpConfig;
//	Wifi* mpWifi;

    // for fetching Data from the Fronius appliance
    Url mSolarUrl;
    String mSolarUrlString;

    // for sending metrics to Dynatrace API
    Url mDtUrlMetric;
    String mDtUrlMetricString;
    Url mDtUrlDevice;
    String mDtUrlDeviceString;

    bool mInitialized = false;
    bool mEnabled;
    __uint8_t mActTaskId; 
    __uint8_t mActConfigRevision;

    int miSOC;
    double mdPV;
    String msBatteryMode;
};

#endif