/*
 * Ota.cpp
 *
 */
#include "Ota.h"

#include "sdkconfig.h"
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_partition.h>

#include <nvs.h>
#include <nvs_flash.h>

#include "WebClient.h"

#define LATEST_FIRMWARE_URL "https://surpro4:9999/getfirmware"
//#define BUFFSIZE 1024
//#define TEXT_BUFFSIZE 1024

static const char* LOGTAG = "ota";


volatile int Ota::miProgress = OTA_PROGRESS_NOTYETSTARTED;
int Ota::GetProgress() { return miProgress; }


Ota::Ota() {
    miProgress = OTA_PROGRESS_NOTYETSTARTED;
}

Ota::~Ota() {
	// TODO Auto-generated destructor stub
}


bool Ota::OnReceiveBegin(unsigned short int httpStatusCode, bool isContentLength, unsigned int contentLength) {

    ESP_LOGI(LOGTAG, "Starting OTA example...");

    if (isContentLength) {
        muContentLength = contentLength;
    } else {
        muContentLength = 1536*1024; // we use Ota partition size when we dont have exact firmware size
    }
    miProgress = 0;

	esp_err_t err;
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    ESP_LOGI(LOGTAG, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);
    ESP_LOGI(LOGTAG, "Configured boot partition type %d subtype %d (offset 0x%08x)",
             configured->type, configured->subtype, configured->address);

    mpUpdatePartition = esp_ota_get_next_update_partition(NULL);
    if (mpUpdatePartition == NULL) {
        ESP_LOGE(LOGTAG, "could not get next update partition");
        miProgress = OTA_PROGRESS_FLASHERROR;
    	return false;
    }

    ESP_LOGI(LOGTAG, "Writing to partition subtype %d at offset 0x%x",
             mpUpdatePartition->subtype, mpUpdatePartition->address);


    err = esp_ota_begin(mpUpdatePartition, OTA_SIZE_UNKNOWN, &mOtaHandle);
    if (err != ESP_OK) {
        ESP_LOGE(LOGTAG, "esp_ota_begin failed, error=%d", err);
        //task_fatal_error();
        miProgress = OTA_PROGRESS_FLASHERROR;
        return false;
    }
    ESP_LOGI(LOGTAG, "esp_ota_begin succeeded");
    return true;
}

bool Ota::OnReceiveData(char* buf, int len) {
    //ESP_LOGI(LOGTAG, "OnReceiveData(%d)", len);

	esp_err_t err;
    //ESP_LOGI(LOGTAG, "Before esp_ota_write");
    err = esp_ota_write( mOtaHandle, (const void *)buf, len);
    if (err == ESP_ERR_INVALID_SIZE) {
    	ESP_LOGE(LOGTAG, "Error partition too small for firmware data: %d", muActualDataLength + len );
        miProgress = OTA_PROGRESS_FLASHERROR;
    	return false;
    } else if (err != ESP_OK) {
    	ESP_LOGE(LOGTAG, "Error writing data: %d", err);
        miProgress = OTA_PROGRESS_FLASHERROR;
    	return false;
    }
    muActualDataLength += len;
    miProgress = 100 * muActualDataLength / muContentLength;
    ESP_LOGI(LOGTAG, "Have written image length %d, total %d", len, muActualDataLength);
    return err == ESP_OK;
}

bool Ota::OnReceiveEnd() {
    ESP_LOGI(LOGTAG, "Total Write binary data length : %u", muActualDataLength);
    //ESP_LOGI(LOGTAG, "DATA: %s", dummy.c_str());

    esp_err_t err;

    if (esp_ota_end(mOtaHandle) != ESP_OK) {
        ESP_LOGE(LOGTAG, "esp_ota_end failed!");
        miProgress = OTA_PROGRESS_FLASHERROR;
        //task_fatal_error();
        return false;
    }
    err = esp_ota_set_boot_partition(mpUpdatePartition);
    if (err != ESP_OK) {
        ESP_LOGE(LOGTAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
        miProgress = OTA_PROGRESS_FLASHERROR;
        //task_fatal_error();
        return false;
    }
    ESP_LOGI(LOGTAG, "Prepare to restart system!");
    miProgress = OTA_PROGRESS_FINISHEDSUCCESS;
    return true;
}



bool Ota::UpdateFirmware(std::string sUrl)
{
	Url url;
	url.Parse(sUrl);

	ESP_LOGI(LOGTAG, "Retrieve firmware from: %s", url.GetUrl().c_str());
	mWebClient.Prepare(&url);
	mWebClient.SetDownloadHandler(this);

    unsigned short statuscode = mWebClient.HttpGet();
    if (statuscode != 200) {
        if (miProgress == OTA_PROGRESS_NOTYETSTARTED || miProgress >= 0) {
            miProgress = OTA_PROGRESS_CONNECTIONERROR;
        }
      	ESP_LOGE(LOGTAG, "Ota update failed - error %u", statuscode)
        // esp_reboot();
      	return false;
    }

	ESP_LOGI(LOGTAG, "UpdateFirmware finished successfully. downloaded %u bytes" , muActualDataLength);

    return true;

}

bool Ota::SwitchBootPartition() {


 	esp_err_t err;
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();
    

    ESP_LOGI(LOGTAG, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);
    ESP_LOGI(LOGTAG, "Configured boot partition type %d subtype %d (offset 0x%08x)",
             configured->type, configured->subtype, configured->address);

    const esp_partition_t *switchto = esp_ota_get_next_update_partition(NULL);
    if (switchto == NULL) {
        ESP_LOGE(LOGTAG, "could not get next update partition");
    	return false;
    }

    err = esp_ota_set_boot_partition(switchto);
    if (err != ESP_OK) {
        ESP_LOGE(LOGTAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
        //task_fatal_error();
        return false;
    }
    ESP_LOGI(LOGTAG, "Partition switched. Prepare to restart system!");
    return true;
}


void task_function_firmwareupdate(void* user_data) {
	ESP_LOGW(LOGTAG, "Starting Firmware Update Task ....");

    Ota ota;
    //if(ota.UpdateFirmware("https://github.com/flyinggorilla/esp32gong/raw/master/firmware/ufo-esp32.bin")) {
    if(ota.UpdateFirmware(LATEST_FIRMWARE_URL)) {
      	ESP_LOGI(LOGTAG, "Firmware updated. Rebooting now......");
    } else {
	  	ESP_LOGE(LOGTAG, "OTA update failed!");
    }
  
    // wait 10 seconds before rebooting to make sure client gets success info
    vTaskDelay(10*1000 / portTICK_PERIOD_MS);
	esp_restart();
}



void Ota::StartUpdateFirmwareTask() {
	xTaskCreate(&task_function_firmwareupdate, "firmwareupdate", 8192, NULL, 5, NULL);
}
