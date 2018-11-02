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

static const char* LOGTAG = "FroniusSolar";

static const int LED_COUNT = 15;

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


void FroniusSolarData::Init(Ufo* pUfo, DisplayCharter* pDisplayLowerRing, DisplayCharter* pDisplayUpperRing) {
	ESP_LOGI(LOGTAG, "Init");
    DynatraceAction* dtIntegration = pUfo->dt.enterAction("Init FroniusSolarData");	

    mpUfo = pUfo;  
    mpDisplayLowerRing = pDisplayLowerRing;
    mpDisplayUpperRing = pDisplayUpperRing;

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
            ESP_LOGE(LOGTAG, "Communication with Dynatrace failed - error %u", responseCode);
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
    mpDisplayUpperRing->SetLeds(0, 3, 0x0000ff);
    mpDisplayLowerRing->SetLeds(0, 3, 0x0000ff);
    mpDisplayUpperRing->SetWhirl(220, true);
    mpDisplayLowerRing->SetWhirl(220, false);
    miSOC = -1;
    mdPV = -1;
}


void FroniusSolarData::DisplayDefault() {
	//ESP_LOGW(LOGTAG, "Unimplemented: DisplayDefault: %d/%f", miSOC, mdPV);

	ESP_LOGD(LOGTAG, "DisplayDefault: %d/%f", miSOC, mdPV);
    mpDisplayLowerRing->Init();
    mpDisplayUpperRing->Init();

    // map values to 15 LEDs each
    int topLedCount = (int)(((double)LED_COUNT)/100*miSOC);
    int bottomLedCount = (int)(((double)LED_COUNT)/ mpConfig->miSolarMax * mdPV);

    ESP_LOGI(LOGTAG, "current SOC: %i: %i, current PV: %f (max: %i): %i", miSOC, topLedCount, mdPV, mpConfig->miSolarMax, bottomLedCount);
    mpDisplayUpperRing->SetLeds(0, LED_COUNT, 0x000000);
    if(topLedCount > 0) {
        mpDisplayUpperRing->SetLeds(0, topLedCount, 002200);
    }

    mpDisplayLowerRing->SetLeds(0, LED_COUNT, 0x000000);
    if(bottomLedCount > 0) {
        mpDisplayLowerRing->SetLeds(LED_COUNT - bottomLedCount, bottomLedCount, 000044);
    }
}


void FroniusSolarData::Process(String& jsonString) {
	//ESP_LOGW(LOGTAG, "Unimplemented: Handle JSON: %s", jsonString.c_str());

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
   "Body" : {
      "Data" : {
         "Inverters" : {
            "1" : {
               ...
               "SOC" : 35
            }
         },
         "Site" : {
            ...
            "P_PV" : -0.95600000000000007,
         },
      }
   }
    */

    cJSON* jsonSOC = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(json, "Data"), "Inverters"), "1"), "SOC");
    if(!cJSON_IsNull(jsonSOC)) {
        miSOC = jsonSOC->valueint;
    }

    cJSON* jsonPV = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(json, "Data"), "Site"), "P_PV");
    if(!cJSON_IsNull(jsonPV)) {
        mdPV = jsonPV->valuedouble;
    }

    cJSON_Delete(parentJson);

    DisplayDefault();
}
