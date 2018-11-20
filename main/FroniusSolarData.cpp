#include "FroniusSolarData.h"
#include "DynatraceAction.h"
#include "WebClient.h"
#include "Url.h"
#include "Ufo.h"
#include "DisplayCharter.h"
#include "Config.h"
#include "String.h"
#include "esp_system.h"
#include <esp_log.h>
#include <cJSON.h>
#include <sys/time.h>

static const char* LOGTAG = "FroniusSolar";

typedef struct{
    FroniusSolarData* pIntegration;
    __uint8_t uTaskId;
} TSolarTaskParam;

void task_function_fronius_solar_data(void *pvParameter)
{
    ESP_LOGI(LOGTAG, "task_function_fronius_solar_data");
    TSolarTaskParam* p = (TSolarTaskParam*)pvParameter;
    ESP_LOGI(LOGTAG, "run pIntegration");
	p->pIntegration->Run(p->uTaskId);
    ESP_LOGI(LOGTAG, "Ending Task %d", p->uTaskId);
    delete p;

	vTaskDelete(NULL);
}


FroniusSolarData::FroniusSolarData() {
    miSOC = -1;
    mdPV = -1;

	ESP_LOGI(LOGTAG, "Start");
}


FroniusSolarData::~FroniusSolarData() {

}


void FroniusSolarData::Init(Ufo* pUfo, DisplayCharter* pDisplayLowerRing, DisplayCharter* pDisplayUpperRing,
            DisplayCharterLogo* pDisplayCharterLogo) {
	ESP_LOGI(LOGTAG, "Init");
    DynatraceAction* dtIntegration = pUfo->dt.enterAction("Init FroniusSolarData");	

    mpUfo = pUfo;  
    mpDisplayLowerRing = pDisplayLowerRing;
    mpDisplayUpperRing = pDisplayUpperRing;
    mpDisplayCharterLogo = pDisplayCharterLogo;

    mpConfig = &(mpUfo->GetConfig());
    mEnabled = false;
    mInitialized = true;
    mActTaskId = 1;
    mActConfigRevision = 0;
    ProcessConfigChange();
    mpUfo->dt.leaveAction(dtIntegration);
}

// care about starting or ending the task
void FroniusSolarData::ProcessConfigChange(){
    ESP_LOGI(LOGTAG, "Config change, %d", mInitialized);

    if (!mInitialized)
        return; 

    //its a little tricky to handle an enable/disable race condition without ending up with 0 or 2 tasks running
    //so whenever there is a (short) disabled situation detected we let the old task go and do not need to wait on its termination
    if (mpConfig->mbSolarEnabled){
        if (!mEnabled){
            TSolarTaskParam* pParam = new TSolarTaskParam;
            pParam->pIntegration = this;
            pParam->uTaskId = mActTaskId;
            ESP_LOGI(LOGTAG, "Create Task %d", mActTaskId);
            xTaskCreate(&task_function_fronius_solar_data, "Task_FroniusSolarData", 8192, pParam, 5, NULL);
            ESP_LOGI(LOGTAG, "task created");
        }
    }
    else{
        if (mEnabled)
            mActTaskId++;
    }
    mEnabled = mpConfig->mbSolarEnabled;
    mActConfigRevision++;
}

void FroniusParseIntegrationUrl(Url& rUrl, String& sSolarUrl){
    String sHelp;
 
    ESP_LOGI(LOGTAG, "%s", sSolarUrl.c_str());

    if (sSolarUrl.length()){
        if (sSolarUrl.charAt(sSolarUrl.length()-1) == '/')
            sHelp.printf("%ssolar_api/v1/GetPowerFlowRealtimeData.fcgi", sSolarUrl.c_str());
        else
            sHelp.printf("%s/solar_api/v1/GetPowerFlowRealtimeData.fcgi", sSolarUrl.c_str());
    }
    
    ESP_LOGD(LOGTAG, "URL: %s", sHelp.c_str());
    rUrl.Clear();
    rUrl.Parse(sHelp);
}


void ParseDynatraceDeviceUrl(Url& rUrlDevice, String& sEnvIdOrUrl, String& sApiToken){
    String sHelpDevice;

    ESP_LOGI(LOGTAG, "%s", sEnvIdOrUrl.c_str());
    ESP_LOGD(LOGTAG, "%s", sApiToken.c_str());

    if (sEnvIdOrUrl.length()){
        if (sEnvIdOrUrl.indexOf(".") < 0){ //an environment id
            sHelpDevice.printf("https://%s.live.dynatrace.com/api/v1/entity/infrastructure/custom/dost_solar?Api-Token=%s", sEnvIdOrUrl.c_str(), sApiToken.c_str());
        }
        else{
            if (sEnvIdOrUrl.startsWith("http")){
                if (sEnvIdOrUrl.charAt(sEnvIdOrUrl.length()-1) == '/') {
                    sHelpDevice.printf("%sapi/v1/entity/infrastructure/custom/dost_solar?Api-Token=%s", sEnvIdOrUrl.c_str(), sApiToken.c_str());
                } else {
                    sHelpDevice.printf("%s/api/v1/entity/infrastructure/custom/dost_solar?Api-Token=%s", sEnvIdOrUrl.c_str(), sApiToken.c_str());
                }
            }
            else{
                if (sEnvIdOrUrl.charAt(sEnvIdOrUrl.length()-1) == '/') {
                    sHelpDevice.printf("https://%sapi/v1/entity/infrastructure/custom/dost_solar?Api-Token=%s", sEnvIdOrUrl.c_str(), sApiToken.c_str());
                } else {
                    sHelpDevice.printf("https://%s/api/v1/entity/infrastructure/custom/dost_solar?Api-Token=%s", sEnvIdOrUrl.c_str(), sApiToken.c_str());
                }
            }

        }
    }

    ESP_LOGD(LOGTAG, "URL-Device: %s", sHelpDevice.c_str());
    rUrlDevice.Clear();
    rUrlDevice.Parse(sHelpDevice);
}


void ParseDynatraceMetricUrl(Url& rUrlMetric, String& sEnvIdOrUrl, const char* cpMetric, String& sApiToken){
    String sHelpMetric;

    ESP_LOGI(LOGTAG, "EnvId: %s", sEnvIdOrUrl.c_str());
    ESP_LOGD(LOGTAG, "Token: %s", sApiToken.c_str());
    ESP_LOGD(LOGTAG, "Metric: %s", cpMetric);

    if (sEnvIdOrUrl.length()){
        if (sEnvIdOrUrl.indexOf(".") < 0){ //an environment id
            sHelpMetric.printf("https://%s.live.dynatrace.com/api/v1/timeseries/%s?Api-Token=%s", sEnvIdOrUrl.c_str(), cpMetric, sApiToken.c_str());
        }
        else{
            if (sEnvIdOrUrl.startsWith("http")){
                if (sEnvIdOrUrl.charAt(sEnvIdOrUrl.length()-1) == '/') {
                    sHelpMetric.printf("%sapi/v1/timeseries/%s?Api-Token=%s", sEnvIdOrUrl.c_str(), cpMetric, sApiToken.c_str());
                } else {
                    sHelpMetric.printf("%s/api/v1/timeseries/%s?Api-Token=%s", sEnvIdOrUrl.c_str(), cpMetric, sApiToken.c_str());
                }
            }
            else{
                if (sEnvIdOrUrl.charAt(sEnvIdOrUrl.length()-1) == '/') {
                    sHelpMetric.printf("https://%sapi/v1/timeseries/%s?Api-Token=%s", sEnvIdOrUrl.c_str(), cpMetric, sApiToken.c_str());
                } else {
                    sHelpMetric.printf("https://%s/api/v1/timeseries/%s?Api-Token=%s", sEnvIdOrUrl.c_str(), cpMetric, sApiToken.c_str());
                }
            }

        }
    }

    ESP_LOGD(LOGTAG, "URL-Metric: %s", sHelpMetric.c_str());
    rUrlMetric.Clear();
    rUrlMetric.Parse(sHelpMetric);
}


void FroniusSolarData::Run(__uint8_t uTaskId) {
    __uint8_t uConfigRevision = mActConfigRevision - 1;
    vTaskDelay(5000 / portTICK_PERIOD_MS);
	ESP_LOGD(LOGTAG, "Run");
    while (1) {
        if (mpUfo->GetWifi().IsConnected()) {
            //Configuration is not atomic - so in case of a change there is the possibility that we use inconsistent credentials - but who cares (the next time it would be fine again)
            if (uConfigRevision != mActConfigRevision){
                uConfigRevision = mActConfigRevision; //memory barrier would be needed here
                FroniusParseIntegrationUrl(mSolarUrl, mpConfig->msSolarUrl);

                ESP_LOGI(LOGTAG, "DTURL: %s", mpConfig->msSolarDTEnvIdOrUrl.c_str());
                if(mpConfig->msSolarDTEnvIdOrUrl.length()) {
                    ParseDynatraceDeviceUrl(mDtUrlDevice, mpConfig->msSolarDTEnvIdOrUrl, mpConfig->msSolarDTApiToken);
                    CreateDynatraceDevice();

                    ParseDynatraceMetricUrl(mDtUrlMetric, mpConfig->msSolarDTEnvIdOrUrl, "custom:dost.solar.dt", mpConfig->msSolarDTApiToken);
                    CreateDynatraceMetric("DT");
                    ParseDynatraceMetricUrl(mDtUrlMetric, mpConfig->msSolarDTEnvIdOrUrl, "custom:dost.solar.e_day", mpConfig->msSolarDTApiToken);
                    CreateDynatraceMetric("E_Day");
                    ParseDynatraceMetricUrl(mDtUrlMetric, mpConfig->msSolarDTEnvIdOrUrl, "custom:dost.solar.e_total", mpConfig->msSolarDTApiToken);
                    CreateDynatraceMetric("E_Total");
                    ParseDynatraceMetricUrl(mDtUrlMetric, mpConfig->msSolarDTEnvIdOrUrl, "custom:dost.solar.e_year", mpConfig->msSolarDTApiToken);
                    CreateDynatraceMetric("E_Year");
                    ParseDynatraceMetricUrl(mDtUrlMetric, mpConfig->msSolarDTEnvIdOrUrl, "custom:dost.solar.p", mpConfig->msSolarDTApiToken);
                    CreateDynatraceMetric("P");
                    ParseDynatraceMetricUrl(mDtUrlMetric, mpConfig->msSolarDTEnvIdOrUrl, "custom:dost.solar.soc", mpConfig->msSolarDTApiToken);
                    CreateDynatraceMetric("SOC");
                    ParseDynatraceMetricUrl(mDtUrlMetric, mpConfig->msSolarDTEnvIdOrUrl, "custom:dost.solar.p_akku", mpConfig->msSolarDTApiToken);
                    CreateDynatraceMetric("P_Akku");
                    ParseDynatraceMetricUrl(mDtUrlMetric, mpConfig->msSolarDTEnvIdOrUrl, "custom:dost.solar.p_grid", mpConfig->msSolarDTApiToken);
                    CreateDynatraceMetric("P_Grid");
                    ParseDynatraceMetricUrl(mDtUrlMetric, mpConfig->msSolarDTEnvIdOrUrl, "custom:dost.solar.p_load", mpConfig->msSolarDTApiToken);
                    CreateDynatraceMetric("P_Load");
                    ParseDynatraceMetricUrl(mDtUrlMetric, mpConfig->msSolarDTEnvIdOrUrl, "custom:dost.solar.p_pv", mpConfig->msSolarDTApiToken);
                    CreateDynatraceMetric("P_PV");
                    ParseDynatraceMetricUrl(mDtUrlMetric, mpConfig->msSolarDTEnvIdOrUrl, "custom:dost.solar.rel_autononmy", mpConfig->msSolarDTApiToken);
                    CreateDynatraceMetric("rel_Autonomy");
                    ParseDynatraceMetricUrl(mDtUrlMetric, mpConfig->msSolarDTEnvIdOrUrl, "custom:dost.solar.rel_selfconsumption", mpConfig->msSolarDTApiToken);
                    CreateDynatraceMetric("rel_SelfConsumption");
                }
            }

            GetData();

            ESP_LOGD(LOGTAG, "free heap after processing Fronius: %i", esp_get_free_heap_size());

            for (int i=0 ; i < mpConfig->miSolarInterval ; i++){
                vTaskDelay(1000 / portTICK_PERIOD_MS);

                if (uTaskId != mActTaskId)
                    return;
            }
        }
        else
		    vTaskDelay(1000 / portTICK_PERIOD_MS);

        if (uTaskId != mActTaskId)
            return;
    }
}

void FroniusSolarData::GetData() {
	ESP_LOGD(LOGTAG, "polling");
    DynatraceAction* dtPollApi = mpUfo->dt.enterAction("Poll Fronius Solar API");
    if (solarClient.Prepare(&mSolarUrl)) {
        DynatraceAction* solarHttpGet = mpUfo->dt.enterAction("HTTP Get Request", WEBREQUEST, dtPollApi);
        unsigned short responseCode = solarClient.HttpGet();
        String response = solarClient.GetResponseData();
        mpUfo->dt.leaveAction(solarHttpGet, &mSolarUrlString, responseCode, response.length());
        if (responseCode == 200) {
            DynatraceAction* solarProcess = mpUfo->dt.enterAction("Process Fronius Solar Metrics", dtPollApi);
            Process(response);
            mpUfo->dt.leaveAction(solarProcess);
        } else {
            ESP_LOGE(LOGTAG, "Communication with Fronius API failed - error %u", responseCode);
            DynatraceAction* solarFailure = mpUfo->dt.enterAction("Handle Fronius Solar API failure", dtPollApi);
            HandleFailure();
            mpUfo->dt.leaveAction(solarFailure);
        }        
    }
    solarClient.Clear();
    mpUfo->dt.leaveAction(dtPollApi);
}

void FroniusSolarData::HandleFailure() {
    mpDisplayUpperRing->Init();
    mpDisplayLowerRing->Init();
    mpDisplayCharterLogo->Init();
    mpDisplayUpperRing->SetLeds(0, 3, 0x0000ff);
    mpDisplayLowerRing->SetLeds(0, 3, 0x0000ff);
    mpDisplayUpperRing->SetWhirl(220, true);
    mpDisplayLowerRing->SetWhirl(220, false);
    mpDisplayCharterLogo->SetLed(0, 0, 0, 0);
    mpDisplayCharterLogo->SetLed(1, 0, 0, 0);
    mpDisplayCharterLogo->SetLed(2, 0, 0, 0);
    mpDisplayCharterLogo->SetLed(3, 0, 0, 0);
    miSOC = -1;
    mdPV = -1;
    msBatteryMode = "";
}


void FroniusSolarData::CreateDynatraceDevice() {
	ESP_LOGD(LOGTAG, "Creating Dynatrace Custom Device");
    DynatraceAction* dtPollApi = mpUfo->dt.enterAction("Prepare Dynatrace Custom Device");
    if (solarClient.Prepare(&mDtUrlDevice)) {
        DynatraceAction* dtHttpPost = mpUfo->dt.enterAction("HTTP POST Request", WEBREQUEST, dtPollApi);
        String data = String("{\"displayName\":\"Fronius Device\",\"type\":\"Fronius\"}");
        unsigned short responseCode = solarClient.HttpPost(data);
        String response = solarClient.GetResponseData();
        mpUfo->dt.leaveAction(dtHttpPost, &mDtUrlDeviceString, responseCode, response.length());
        if (responseCode == 200) {
            DynatraceAction* dtProcess = mpUfo->dt.enterAction("Process response from Dynatrace Custom Device creation", dtPollApi);
            //Process(response);
            mpUfo->dt.leaveAction(dtProcess);
        } else {
            ESP_LOGE(LOGTAG, "Communication with Dynatrace failed - error %u", responseCode);
            DynatraceAction* dtFailure = mpUfo->dt.enterAction("Handle Dynatrace API failure", dtPollApi);
            HandleFailure();
            mpUfo->dt.leaveAction(dtFailure);
        }
    }
    solarClient.Clear();

    mpUfo->dt.leaveAction(dtPollApi);
}


void FroniusSolarData::CreateDynatraceMetric(const char* cpName) {
	ESP_LOGD(LOGTAG, "Creating Dynatrace Custom Metric");
    DynatraceAction* dtPollApi = mpUfo->dt.enterAction("Prepare Dynatrace Custom Metric");
    if (solarClient.Prepare(&mDtUrlMetric)) {
        DynatraceAction* dtHttpPost = mpUfo->dt.enterAction("HTTP POST Request", WEBREQUEST, dtPollApi);
        String data = String("{\"displayName\":\"Fronius Solar-");
        data += cpName;
        data += "\",\"unit\":\"Count\",\"dimensions\":[\"value\"],\"types\":[\"Fronius\"]}";
        unsigned short responseCode = solarClient.HttpPost(data);
        String response = solarClient.GetResponseData();
        mpUfo->dt.leaveAction(dtHttpPost, &mDtUrlMetricString, responseCode, response.length());
        if (responseCode == 200) {
            DynatraceAction* dtProcess = mpUfo->dt.enterAction("Process response from Dynatrace Custom Metric creation", dtPollApi);
            //Process(response);
            mpUfo->dt.leaveAction(dtProcess);
        } else {
            ESP_LOGE(LOGTAG, "Communication with Dynatrace failed - error %u", responseCode);
            DynatraceAction* dtFailure = mpUfo->dt.enterAction("Handle Dynatrace API failure", dtPollApi);
            HandleFailure();
            mpUfo->dt.leaveAction(dtFailure);
        }
    }
    solarClient.Clear();

    mpUfo->dt.leaveAction(dtPollApi);
}


void FroniusSolarData::SendDataToDynatrace(String &series) {
	ESP_LOGD(LOGTAG, "Sending to dynatrace: %s", series.c_str());
    DynatraceAction* dtPollApi = mpUfo->dt.enterAction("Send to Dynatrace API");
    if (solarClient.Prepare(&mDtUrlDevice)) {
        DynatraceAction* dtHttpPost = mpUfo->dt.enterAction("HTTP POST Request", WEBREQUEST, dtPollApi);

        String data = String("{ \"series\" : [");
        data += series;
        data += "] }";
        unsigned short responseCode = solarClient.HttpPost(data);
        String response = solarClient.GetResponseData();
        mpUfo->dt.leaveAction(dtHttpPost, &mDtUrlDeviceString, responseCode, response.length());
        if (responseCode == 200) {
            DynatraceAction* dtProcess = mpUfo->dt.enterAction("Process response from Dynatrace Metrics data", dtPollApi);
            //Process(response);
            mpUfo->dt.leaveAction(dtProcess);
        } else {
            ESP_LOGE(LOGTAG, "Communication with Dynatrace failed - error %u", responseCode);
            DynatraceAction* dtFailure = mpUfo->dt.enterAction("Handle Dynatrace API failure", dtPollApi);
            HandleFailure();
            mpUfo->dt.leaveAction(dtFailure);
        }
    }
    solarClient.Clear();
    mpUfo->dt.leaveAction(dtPollApi);
}


void FroniusSolarData::DisplayDefault() {
	ESP_LOGD(LOGTAG, "DisplayDefault: %d, %f, %s", miSOC, mdPV, msBatteryMode.c_str());
    mpDisplayLowerRing->Init();
    mpDisplayUpperRing->Init();
    mpDisplayCharterLogo->Init();

    // map values to 15 LEDs each
    int topLedCount = (int)(((double)RING_LEDCOUNT)/100*miSOC);
    int bottomLedCount = (int)(((double)RING_LEDCOUNT)/ mpConfig->miSolarMax * mdPV);

    ESP_LOGI(LOGTAG, "current SOC: %i: %i, current PV: %f (max: %i): %i, battery mode: %s",
        miSOC, topLedCount, mdPV, mpConfig->miSolarMax, bottomLedCount, msBatteryMode.c_str());

    mpDisplayUpperRing->SetLeds(0, RING_LEDCOUNT, 0x000000);
    if(topLedCount > 0) {
        mpDisplayUpperRing->SetLeds(0, topLedCount, 0x002200);
    }

    mpDisplayLowerRing->SetLeds(0, RING_LEDCOUNT, 0x000000);
    if(bottomLedCount > 0) {
        mpDisplayLowerRing->SetLeds(RING_LEDCOUNT - bottomLedCount, bottomLedCount, 0x000044);
    }

    // Visualize Battery_Mode in the colors of the Logo
/*
# optional field
# " disabled ", " normal ", " service ", " charge boost ",
# " nearly depleted ", " suspended ", " calibrate ",
# "grid support ", " deplete recovery ", "non operable ( voltage )",
# "non operable ( temperature )", " preheating " or " startup "
string Battery_Mode ;
*/
    if(msBatteryMode.startsWith("normal") || msBatteryMode.startsWith("nearly depleted")) {
        // do not light the LEDs on "normal" or "nearly depleted" as these are the expected normal states
        mpDisplayCharterLogo->SetLed(0, 0, 0, 0);
        mpDisplayCharterLogo->SetLed(1, 0, 0, 0);
        mpDisplayCharterLogo->SetLed(2, 0, 0, 0);
        mpDisplayCharterLogo->SetLed(3, 0, 0, 0);
    } else if(msBatteryMode.startsWith("disabled") || msBatteryMode.startsWith("service") ||
            msBatteryMode.startsWith("suspended") || msBatteryMode.startsWith("non operable")) {
        // display error states in red
        ESP_LOGW(LOGTAG, "Found warning state: %s", msBatteryMode.c_str());
        mpDisplayCharterLogo->SetLed(0, 255, 0, 0);
        mpDisplayCharterLogo->SetLed(1, 255, 0, 0);
        mpDisplayCharterLogo->SetLed(2, 255, 0, 0);
        mpDisplayCharterLogo->SetLed(3, 255, 0, 0);
    } else if(msBatteryMode.startsWith("calibrate") || msBatteryMode.startsWith("grid support") ||
            msBatteryMode.startsWith("deplete recovery") || msBatteryMode.startsWith("suspended") ||
            msBatteryMode.startsWith("preheating") || msBatteryMode.startsWith("startup") ||
            msBatteryMode.startsWith("charge boost")) {
        // display temporary or unexpected states in yellow
        ESP_LOGI(LOGTAG, "Found temporary state: %s", msBatteryMode.c_str());
        mpDisplayCharterLogo->SetLed(0, 255, 255, 0);
        mpDisplayCharterLogo->SetLed(1, 255, 255, 0);
        mpDisplayCharterLogo->SetLed(2, 255, 255, 0);
        mpDisplayCharterLogo->SetLed(3, 255, 255, 0);
    } else {
        // display temporary or unexpected states in yellow
        ESP_LOGW(LOGTAG, "Found unexpected state: %s", msBatteryMode.c_str());
        mpDisplayCharterLogo->SetLed(0, 0, 0, 255);
        mpDisplayCharterLogo->SetLed(1, 0, 0, 255);
        mpDisplayCharterLogo->SetLed(2, 0, 0, 255);
        mpDisplayCharterLogo->SetLed(3, 0, 0, 255);
    }
}

long timestamp() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

void addSeries(String &series, const char* cpMetric, cJSON *jsonMetric) {
    if(cJSON_IsNull(jsonMetric)) {
        return;
    }

    if(series.length() > 0) {
        series += ",";
    }

    series.printf("{ \"timeseriesId\" : \"%s\", \"dimensions\" : { \"value\" : \"value\" }, \"dataPoints\" : [ [ %d, %f ] ] }",
        cpMetric, timestamp, jsonMetric->valuedouble);
}

void FroniusSolarData::Process(String& jsonString) {
    cJSON* parentJson = cJSON_Parse(jsonString.c_str());
    if (!parentJson) {
        ESP_LOGW(LOGTAG, "Could not parse: %s", jsonString.c_str());
        return;
    }
    cJSON* json = cJSON_GetObjectItem(parentJson, "Body");
    if (!json) {
        ESP_LOGW(LOGTAG, "Could not get object 'Body': %s", jsonString.c_str());
        return;
    }
    
    if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG){
        char* sJsonPrint = cJSON_Print(json);
	    ESP_LOGD(LOGTAG, "processing %s", sJsonPrint);
        free(sJsonPrint);
    }

/*
{
   "Body" : {
      "Data" : {
         "Inverters" : {
            "1" : {
               "Battery_Mode" : "nearly depleted",
               "DT" : 99,
               "E_Day" : 1285.300048828125,
               "E_Total" : 17817346,
               "E_Year" : 5229070.5,
               "P" : 645,
               "SOC" : 6
            }
         },
         "Site" : {
            "BackupMode" : false,
            "BatteryStandby" : false,
            "E_Day" : 1285.3000000000002,
            "E_Total" : 17817346.400000002,
            "E_Year" : 5229070.5,
            "Meter_Location" : "grid",
            "Mode" : "bidirectional",
            "P_Akku" : -79.519999999999996,
            "P_Grid" : 16.23,
            "P_Load" : -661.23000000000002,
            "P_PV" : 777.5,
            "rel_Autonomy" : 97.545483417267818,
            "rel_SelfConsumption" : 100
         },
         "Version" : "10"
      }
   },
   "Head" : {
      "RequestArguments" : {},
      "Status" : {
         "Code" : 0,
         "Reason" : "",
         "UserMessage" : ""
      },
      "Timestamp" : "2018-11-02T10:29:10+01:00"
   }
}
*/

    cJSON* jsonData = cJSON_GetObjectItem(json, "Data");

    cJSON* jsonSOC = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Inverters"), "1"), "SOC");
    if(!cJSON_IsNull(jsonSOC)) {
        miSOC = jsonSOC->valueint;

        // aprox. 5% are always kept in the battery and are not available for consumption
        if(miSOC > 5) {
            miSOC -= 5;
        }
    }

    cJSON* jsonBatteryMode = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Inverters"), "1"), "Battery_Mode");
    if(!cJSON_IsNull(jsonBatteryMode)) {
        msBatteryMode = jsonBatteryMode->valuestring;
    }

    cJSON* jsonPV = cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Site"), "P_PV");
    if(!cJSON_IsNull(jsonPV)) {
        mdPV = jsonPV->valuedouble;
    }

    if(mpConfig->msSolarDTEnvIdOrUrl.length()) {
        DynatraceAction* dtSendApi = mpUfo->dt.enterAction("Send Fronius Solar data to Dynatrace");
        DynatraceAction* solarDataSend = mpUfo->dt.enterAction("Send Fronius Solar Metrics to Dynatrace", dtSendApi);

        String series = String();
        cJSON* jsonMetric = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Inverters"), "1"), "DT");
        addSeries(series, "custom:dost.solar.dt", jsonMetric);

        jsonMetric = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Inverters"), "1"), "E_Day");
        addSeries(series, "custom:dost.solar.e_day", jsonMetric);

        jsonMetric = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Inverters"), "1"), "E_Total");
        addSeries(series, "custom:dost.solar.e_total", jsonMetric);

        jsonMetric = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Inverters"), "1"), "E_Year");
        addSeries(series, "custom:dost.solar.e_year", jsonMetric);

        jsonMetric = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Inverters"), "1"), "P");
        addSeries(series, "custom:dost.solar.p", jsonMetric);

        jsonMetric = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Inverters"), "1"), "SOC");
        addSeries(series, "custom:dost.solar.soc", jsonMetric);

        jsonMetric = cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Site"), "P_Akku");
        addSeries(series, "custom:dost.solar.p_akku", jsonMetric);

        jsonMetric = cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Site"), "P_Grid");
        addSeries(series, "custom:dost.solar.p_grid", jsonMetric);

        jsonMetric = cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Site"), "P_Load");
        addSeries(series, "custom:dost.solar.p_load", jsonMetric);

        jsonMetric = cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Site"), "P_PV");
        addSeries(series, "custom:dost.solar.p_pv", jsonMetric);

        jsonMetric = cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Site"), "rel_Autonomy");
        addSeries(series, "custom:dost.solar.rel_autonomy", jsonMetric);

        jsonMetric = cJSON_GetObjectItem(cJSON_GetObjectItem(jsonData, "Site"), "rel_SelfConsumption");
        addSeries(series, "custom:dost.solar.rel_selfconsumption", jsonMetric);

        SendDataToDynatrace(series);
        mpUfo->dt.leaveAction(solarDataSend);
    }

    cJSON_Delete(parentJson);

    DisplayDefault();
}
