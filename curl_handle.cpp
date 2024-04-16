#ifndef curl_handle
#define curl_handle

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <curl/curl.h>
#include "cJSON.h"

#include "curl_handle.h"
#include "telnet_server.h"

#define URL "http://127.0.0.1/_dev0"

enum {
	HELP			= 0,
	SHOW_VERSION	= 0,
	TAKE_SNAPSHOT	= 2,
	SET_RECORD		= 1,
	SET_STREAM		= 1,
	GET_FORMAT		= 0
};

cJSON* ptr_jsonReturn = NULL;

struct mapping {
    char  func_name[32];
    void  (*func_ptr)(char** pars, int* res, void* pUserData);
};

struct mapping table[_NUMBER_OF_API] = {
// System Function
    { "help",                       &help_direction },
    { "SHOW_VERSION",               &show_version },
    { "SNAPSHOT",					&shapshot},
    { "RECORD",						&set_record_status },
    { "STREAM",						&set_stream_status },
    { "INPUT",						&get_input_format },
    { "REBOOT",						&reboot },
    { "RESTORE_DEFAULT",			&restore_to_default }
    /*{ "SYSTEM_GET_DEBUG_MODE",       &uke_get_debug_mode },
    { "SYSTEM_SET_DEBUG_MODE",       &uke_set_debug_mode },
    { "SYSTEM_SET_TELNET_MODE",      &uke_set_telnet_mode },
    { "SYSTEM_FPGA_UPDATE",          &uke_set_fpga_update },
    { "SYSTEM_GET_FPGA_PROGRESS",    &uke_get_fpga_progress },
    { "SYSTEM_SEND_USER_CMDLINE",    &uke_send_user_cmdline },*/
};

void handle_client(char data[], void* pUserData) {

	TelnetServer* Server = (TelnetServer*) pUserData;
    int* result = new int;
    *result = -1;
    char pszFailedMsg[BUFFER_SIZE];
	memset(pszFailedMsg, 0x00, BUFFER_SIZE);

    // LOGI("Client Request: %s", data);
    remove_new_line(data);
    printf("Client command: %s\n", data);

    if (strlen(data) == 0) {
        printf("Empty command. Ignoring...\n");
        delete result;
        return;
    }

    //separate data into command and parameters
    char* command;
    char* parameters;

    command = strtok(data, " ");
    parameters = strtok(NULL, "");

    printf("Command:%s\n", command);
    printf("Parameters:%s\n", parameters);

    for (int i = 0; i < _NUMBER_OF_API; i++) {
        if (strcmp(command, table[i].func_name) == 0) {
            table[i].func_ptr(&parameters, result, (void*)Server);
            break;
        }
        else {
            *result = RES_NOT_SUPPORT;
        }
    }

	switch (*result) {
    	case RES_NOT_SUPPORT:
    		snprintf(pszFailedMsg, BUFFER_SIZE, "Sorry, no such command.\n");

			if (write(Server->m_nClientSocket, pszFailedMsg, strlen(pszFailedMsg)) < 0) {
			        perror("ERROR writing to socket");
			        *result = RES_SEND_ERROR;
			}
			break;
    	case RES_PARAMETER_ERROR:
    	case RES_NOT_EXIST:
    	case RES_BUSY:
    	case RES_NO_SPACE:
    	case RES_ERROR_PLATFORM:
    	case RES_SEND_ERROR:
    		memset(pszFailedMsg, 0x00, BUFFER_SIZE);

    		snprintf(pszFailedMsg, BUFFER_SIZE, "Error Return: %d\n", *result);

	        if (write(Server->m_nClientSocket, pszFailedMsg, strlen(pszFailedMsg)) < 0) {
	                perror("ERROR writing to socket");
	                *result = RES_SEND_ERROR;
	        }
	        printf("[handle_client] error: %d\n", *result);
    		break;
    	case RES_SUCCESS:
    		break;
    }

    delete result;

    if (ptr_jsonReturn) { printf("[handle_client]try to delete ptr_jsonReturn\n"); cJSON_Delete(ptr_jsonReturn); ptr_jsonReturn = NULL; printf("[handle_client]Delete ptr_jsonReturn\n");}
    return;
}

char* remove_new_line(char* str) {
    char* ptr = str;
    while (*ptr != '\0') {
        if (*ptr == '\r' || *ptr == '\n') {
            *ptr = '\0';
            break;
        }
        ptr++;
    }
    return str;
}

size_t curl_callback(void *contents, size_t size, size_t nmemb, char **response) {
    size_t realsize = size * nmemb;
    *response = (char*)realloc(*response, realsize + 1);
    if (*response == NULL) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }

    memcpy(*response, contents, realsize);
    (*response)[realsize] = 0;
    return realsize;
}

char* send_curl_command(const char* url, const char* data) {
    CURL *curl;
    CURLcode res;
    char *response = NULL;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();

    return response;
}

int send_command(const char* pszCommand) {//char** pszReturn
    printf("[send_command] command:%s\n", pszCommand);
	printf("[send_command] URL:%s\n", URL);

    char* Response = send_curl_command(URL, pszCommand);

    if (ptr_jsonReturn) { printf("try to delete ptr_jsonReturn\n"); cJSON_Delete(ptr_jsonReturn); ptr_jsonReturn = NULL; printf("Delete ptr_jsonReturn\n");}

    if (Response) {
        printf("JSON Response:%s\n", Response);

        cJSON *root = cJSON_Parse(Response);

        if (root) {
        	cJSON *ReturnField = cJSON_GetObjectItem(root, "Return");
        	cJSON *ErrorField = cJSON_GetObjectItem(root, "_error_");
        	if ( (ReturnField && cJSON_IsNumber(ReturnField) ) &&
        		 (ErrorField  && cJSON_IsString(ErrorField)   ) ) {
        		if (ReturnField->valueint != 0 ||
        		    strcmp(ErrorField->valuestring, "ok") != 0 ) {
        			printf("Http command Return: %d, Error: %s \n", ReturnField->valueint, ErrorField->valuestring);
        			free(Response);
	            	return RES_PARAMETER_ERROR;
        		}

        		ptr_jsonReturn = root;
        	}
        	else {
        		printf("Http command Return or Error is not exist\n");
        		free(Response);
	            return RES_PARAMETER_ERROR;
        	}
        }
        else {
            fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        }
        free(Response);
    } else {
        fprintf(stderr, "Error sending CURL command\n");
        return RES_SEND_ERROR;
    }

    return RES_SUCCESS;

}

void help_direction(char** pars, int* res, void* pUserData) {
	TelnetServer* Server = (TelnetServer*) pUserData;

    printf("here is help\n");

    char pszHelpMsg[BUFFER_SIZE];
	memset(pszHelpMsg, 0x00, BUFFER_SIZE);

	const char* pszShowVersionMsg	= "SHOW_VERSION\t\t\t\t -- Show the device version.\n";

	const char* pszGetVideoFormatMsg= "INPUT\t\t\t\t\t -- Get the input format of the device.\n";

	const char* pszSnapshotMsg		= "SNAPSHOT [IMG_NAME] [0:bmp/1:jpg]\t -- Snapshot an image.\n";

	const char* pszRecordMsg		= "RECORD [0:stop/1:start]\t\t\t -- Open or close Main Record.\n";

	const char* pszStreamMsg		= "STREAM [0:stop/1:start]\t\t\t -- Open or close Main Stream.\n";

	const char* pszRebootMsg		= "REBOOT\t\t\t\t\t -- Reboot the device.\n";

	const char* pszReStoreDefaultMsg= "RESTORE_DEFAULT\t\t\t\t -- Restore to default setting.\n";

    snprintf(pszHelpMsg, BUFFER_SIZE, "%s%s%s%s%s%s%s\n", pszShowVersionMsg, pszGetVideoFormatMsg, pszSnapshotMsg, pszRecordMsg, pszStreamMsg, pszRebootMsg, pszReStoreDefaultMsg);

    if (write(Server->m_nClientSocket, pszHelpMsg, strlen(pszHelpMsg)) < 0) {
        perror("ERROR writing to socket");
        *res = RES_SEND_ERROR;
    }

    *res = RES_SUCCESS;

    return;
}

void show_version(char** pars, int* res, void* pUserData) {
	TelnetServer* Server = (TelnetServer*) pUserData;

	printf("HIHIHIHIHI here is show_version\n");

	const char* pszCommand = "{\"qcapid\":\"53434352514B4352\", \"Function\":\"SYSTEM_GET_VERSION_INFO\"}";
	char pszOutput[BUFFER_SIZE];

	*res = send_command(pszCommand);//, &pszOutput
	switch (*res) {
		case RES_SUCCESS:
			if (ptr_jsonReturn) {
	        	cJSON *FirmwareVersionField = cJSON_GetObjectItem(ptr_jsonReturn, "FirmwareVersion");
	        	cJSON *SoftwareVersionField = cJSON_GetObjectItem(ptr_jsonReturn, "SoftwareVersion");
	        	cJSON *FPGAVersionField = cJSON_GetObjectItem(ptr_jsonReturn, "FpgaVersion");

		        if (FirmwareVersionField && cJSON_IsString(FirmwareVersionField) &&
		            SoftwareVersionField && cJSON_IsString(SoftwareVersionField) &&
		            FPGAVersionField && cJSON_IsString(FPGAVersionField)) {
		        	memset(&pszOutput, 0x00, BUFFER_SIZE);
		        	snprintf(pszOutput, BUFFER_SIZE, "%-7s: %s\n%-7s: %s\n%-7s: %s\n\n","Version", SoftwareVersionField->valuestring, "MCU", FirmwareVersionField->valuestring, "FPGA", FPGAVersionField->valuestring);

		        	//printf("Command output:%s\n", pszOutput);

		        	if (write(Server->m_nClientSocket, pszOutput, strlen(pszOutput)) < 0) {
		                perror("ERROR writing to socket");
		                *res = RES_SEND_ERROR;
	        		}
	        		else {
	        			*res = RES_SUCCESS;
	        		}
		        }
		        else {
		        	printf("json may somthing wrong\n");

		        	*res = RES_NOT_EXIST;
		        }
        	}
	        else {
	        	printf("ptr_jsonReturn is NULL\n");

		       	*res = RES_NOT_EXIST;
	        }
	        break;

    	case RES_NOT_SUPPORT:
    	case RES_PARAMETER_ERROR:
    	case RES_NOT_EXIST:
    	case RES_BUSY:
    	case RES_NO_SPACE:
    	case RES_ERROR_PLATFORM:
    	case RES_SEND_ERROR:
    		break;
	}
    return;
}

void shapshot(char** pars, int* res, void* pUserData) {
	TelnetServer* Server = (TelnetServer*) pUserData;

	printf("here is snapshot\n");
	printf("Parameters:%s---\n", *pars);

	char pszPICName[64];
	int nPICType;

	if (*pars == NULL) {
		*res = RES_PARAMETER_ERROR;
		return;
	}

	if (sscanf(*pars, "%s %d", pszPICName, &nPICType) != TAKE_SNAPSHOT) {
		*res = RES_PARAMETER_ERROR;
		return;
	}

	char pszCommand[BUFFER_SIZE];
	memset(&pszCommand, 0x00, sizeof(pszCommand));

	char pszOutput[BUFFER_SIZE];
	memset(&pszOutput, 0x00, sizeof(pszOutput));

	snprintf(pszCommand, BUFFER_SIZE, \
	"{\"qcapid\":\"53434352514B4352\", \"Function\":\"APPS_SET_SNAPSHOT\", \"Enabled\":\"1\", \"FileName\":\"%s\", \"FileType\":\"%d\", \"Quality\":\"100\", \"Snapshot\":\"1\"}", pszPICName, nPICType);

	printf("%s----\n", pszCommand);

	*res = send_command(pszCommand);//, &pszOutput
	switch (*res) {
		case RES_SUCCESS:
			if (ptr_jsonReturn) {
				snprintf(pszOutput, BUFFER_SIZE, "screenshoted %s.%s\n\n", pszPICName, (nPICType)? "jpg":"bmp");
	        	if (write(Server->m_nClientSocket, pszOutput, strlen(pszOutput)) < 0) {
	                perror("ERROR writing to socket");
	                *res = RES_SEND_ERROR;
        		}
        		else {
        			*res = RES_SUCCESS;
        		}
	        }
	        else {
	        	printf("ptr_jsonReturn is NULL\n");

		       	*res = RES_NOT_EXIST;
	        }
        	break;

    	case RES_NOT_SUPPORT:
    	case RES_PARAMETER_ERROR:
    	case RES_NOT_EXIST:
    	case RES_BUSY:
    	case RES_NO_SPACE:
    	case RES_ERROR_PLATFORM:
    	case RES_SEND_ERROR:
    		break;
	}
    return;
}

void set_record_status(char** pars, int* res, void* pUserData) {
	TelnetServer* Server = (TelnetServer*) pUserData;

	printf("here is set_record_status -------- start\n");
	printf("Parameters:%s---\n", *pars);

	int nRecordStatus;

	if (*pars == NULL) {
		*res = RES_PARAMETER_ERROR;
		return;
	}

	if (sscanf(*pars, "%d",&nRecordStatus) != SET_RECORD) {
		*res = RES_PARAMETER_ERROR;
		return;
	}

	if (nRecordStatus != 0 && nRecordStatus != 1) {
		*res = RES_PARAMETER_ERROR;
		printf("nRecordStatus: %d, it should be 0 or 1.\n", nRecordStatus);
		return;
	}

	char pszCommand[BUFFER_SIZE];
	memset(&pszCommand, 0x00, sizeof(pszCommand));

	char pszOutput[BUFFER_SIZE];
	memset(&pszOutput, 0x00, sizeof(pszOutput));

	snprintf(pszCommand, BUFFER_SIZE, \
	"{\"qcapid\":\"53434352514B4352\", \"Function\":\"APPS_SET_RECORD_STATUS\", \"EncoderNum\":\"0\", \"RecordNum\":\"1\", \"Status\":\"%d\"}", nRecordStatus);

	printf("%s----\n", pszCommand);

	*res = send_command(pszCommand);//, &pszOutput
	switch (*res) {
		case RES_SUCCESS:
			if (ptr_jsonReturn) {
				snprintf(pszOutput, BUFFER_SIZE, "record %s\n\n", (nRecordStatus)? "started":"stopped");
	        	if (write(Server->m_nClientSocket, pszOutput, strlen(pszOutput)) < 0) {
	                perror("ERROR writing to socket");
	                *res = RES_SEND_ERROR;
        		}
        		else {
        			*res = RES_SUCCESS;
        		}
	        }
	        else {
	        	printf("ptr_jsonReturn is NULL\n");

		       	*res = RES_NOT_EXIST;
	        }
        	break;

    	case RES_NOT_SUPPORT:
    	case RES_PARAMETER_ERROR:
    	case RES_NOT_EXIST:
    	case RES_BUSY:
    	case RES_NO_SPACE:
    	case RES_ERROR_PLATFORM:
    	case RES_SEND_ERROR:
    		break;
	}

	return;
}

void set_stream_status(char** pars, int* res, void* pUserData){
	TelnetServer* Server = (TelnetServer*) pUserData;

	printf("here is set_stream_status -------- start\n");
	printf("Parameters:%s---\n", *pars);

	int nStreamStatus;

	if (*pars == NULL) {
		*res = RES_PARAMETER_ERROR;
		return;
	}

	if (sscanf(*pars, "%d",&nStreamStatus) != SET_STREAM) {
		*res = RES_PARAMETER_ERROR;
		return;
	}

	if (nStreamStatus != 0 && nStreamStatus != 1) {
		*res = RES_PARAMETER_ERROR;
		printf("nStreamStatus: %d, it should be 0 or 1.\n", nStreamStatus);
		return;
	}

	char pszCommand[BUFFER_SIZE];
	memset(&pszCommand, 0x00, sizeof(pszCommand));

	char pszOutput[BUFFER_SIZE];
	memset(&pszOutput, 0x00, sizeof(pszOutput));

	snprintf(pszCommand, BUFFER_SIZE, \
	"{\"qcapid\":\"53434352514B4352\", \"Function\":\"APPS_SET_STREAMING_STATUS\", \"EncoderNum\":\"0\", \"StreamNum\":\"1\", \"StreamType\":\"0\", \"Status\":\"%d\"}", nStreamStatus);

	printf("%s----\n", pszCommand);

	*res = send_command(pszCommand);//, &pszOutput
	switch (*res) {
		case RES_SUCCESS:
			if (ptr_jsonReturn) {
				snprintf(pszOutput, BUFFER_SIZE, "stream %s\n\n", (nStreamStatus)? "started":"stopped");
	        	if (write(Server->m_nClientSocket, pszOutput, strlen(pszOutput)) < 0) {
	                perror("ERROR writing to socket");
	                *res = RES_SEND_ERROR;
        		}
        		else {
        			*res = RES_SUCCESS;
        		}
	        }
	        else {
	        	printf("ptr_jsonReturn is NULL\n");

		       	*res = RES_NOT_EXIST;
	        }
        	break;

    	case RES_NOT_SUPPORT:
    	case RES_PARAMETER_ERROR:
    	case RES_NOT_EXIST:
    	case RES_BUSY:
    	case RES_NO_SPACE:
    	case RES_ERROR_PLATFORM:
    	case RES_SEND_ERROR:
    		break;
	}

	return;
}

void get_input_format(char** pars, int* res, void* pUserData) {
	TelnetServer* Server = (TelnetServer*) pUserData;

	printf("here is get_input_format -------- start\n");

	const char* pszCommand1 = "{\"qcapid\":\"53434352514B4352\", \"Function\":\"APPS_GET_VIDEO_SOURCE\"}";
	char pszVideoSource[BUFFER_SIZE];
	memset(&pszVideoSource, 0x00, BUFFER_SIZE);

	*res = send_command(pszCommand1);

	if (ptr_jsonReturn) {
    	cJSON *VideoSourceField = cJSON_GetObjectItem(ptr_jsonReturn, "VideoSource");
    	switch (VideoSourceField->valueint) {
	    	case 0 :
	    		snprintf(pszVideoSource, BUFFER_SIZE, "%s","Composite");
	    		break;
	    	case 1 :
	    		snprintf(pszVideoSource, BUFFER_SIZE, "%s","S-Video");
	    		break;
    		case 2 :
	    		snprintf(pszVideoSource, BUFFER_SIZE, "%s","HDMI");
	    		break;
    		case 3 :
	    		snprintf(pszVideoSource, BUFFER_SIZE, "%s","DVI");
	    		break;
    		case 4 :
	    		snprintf(pszVideoSource, BUFFER_SIZE, "%s","YPbPr");
	    		break;
    		case 5 :
	    		snprintf(pszVideoSource, BUFFER_SIZE, "%s","VGA");
	    		break;
    		case 6 :
	    		snprintf(pszVideoSource, BUFFER_SIZE, "%s","SDI");
	    		break;
    		case 12 :
	    		snprintf(pszVideoSource, BUFFER_SIZE, "%s","DP");
	    		break;
    		case 7 :
	    		snprintf(pszVideoSource, BUFFER_SIZE, "%s","Picture");
	    		break;
    		case 8 :
	    		snprintf(pszVideoSource, BUFFER_SIZE, "%s","Stream");
	    		break;
    		case 9 :
	    		snprintf(pszVideoSource, BUFFER_SIZE, "%s","File");
	    		break;
    		case 10 :
	    		snprintf(pszVideoSource, BUFFER_SIZE, "%s","NDI");
	    		break;
    		case 11 :
	    		snprintf(pszVideoSource, BUFFER_SIZE, "%s","Camera");
	    		break;
    	}
    }

	const char* pszCommand = "{\"qcapid\":\"53434352514B4352\", \"Function\":\"APPS_GET_INPUT_FORMAT\"}";

	char pszOutput[BUFFER_SIZE];

	printf("%s----\n", pszCommand);

	*res = send_command(pszCommand);//, &pszOutput
	switch (*res) {
		case RES_SUCCESS:
			if (ptr_jsonReturn) {
	        	cJSON *SignalField = cJSON_GetObjectItem(ptr_jsonReturn, "Signal");


		        if (SignalField && cJSON_IsNumber(SignalField) ) {
		        	if (SignalField->valueint == 0) {
		        		memset(&pszOutput, 0x00, BUFFER_SIZE);
		        		snprintf(pszOutput, BUFFER_SIZE, "%s\n\n","No input.");
		        	}
		        	else {
		        		cJSON *WidthField = cJSON_GetObjectItem(ptr_jsonReturn, "Width");
	        			cJSON *HeightField = cJSON_GetObjectItem(ptr_jsonReturn, "Height");
	        			cJSON *InterleavedField = cJSON_GetObjectItem(ptr_jsonReturn, "Interleaved");
	        			cJSON *FramerateField = cJSON_GetObjectItem(ptr_jsonReturn, "Framerate");
	        			cJSON *AudioFormatField = cJSON_GetObjectItem(ptr_jsonReturn, "AudioFormat");
	        			cJSON *ChannelsField = cJSON_GetObjectItem(ptr_jsonReturn, "Channels");
	        			cJSON *BitsPerSampleField = cJSON_GetObjectItem(ptr_jsonReturn, "BitsPerSample");
	        			cJSON *SampleFrequencyField = cJSON_GetObjectItem(ptr_jsonReturn, "SampleFrequency");/**/

	        			/**/char psztmp1[BUFFER_SIZE];
	        			memset(&psztmp1, 0x00, BUFFER_SIZE);
	        			snprintf(psztmp1, BUFFER_SIZE, "%dx%d", WidthField->valueint, HeightField->valueint);

	        			char pszOutput1[BUFFER_SIZE]; char pszOutput2[BUFFER_SIZE]; char pszOutput3[BUFFER_SIZE]; char pszOutput4[BUFFER_SIZE]; char pszOutput5[BUFFER_SIZE];
	        			memset(&pszOutput1, 0x00, BUFFER_SIZE);
	        			memset(&pszOutput2, 0x00, BUFFER_SIZE);
	        			memset(&pszOutput3, 0x00, BUFFER_SIZE);
	        			memset(&pszOutput4, 0x00, BUFFER_SIZE);
	        			memset(&pszOutput5, 0x00, BUFFER_SIZE);

	        			snprintf(pszOutput1, BUFFER_SIZE, "┌-------------------------------------------------------------------------------------------------------┐\n");
	        			snprintf(pszOutput2, BUFFER_SIZE, "┝ Video Source │ Audio Source │ Resolution | Frame Rate | Channels | Bits Per Sample | Sample Frequency ┤\n");
	        			snprintf(pszOutput3, BUFFER_SIZE, "┝-------------------------------------------------------------------------------------------------------┤\n");
	        			snprintf(pszOutput4, BUFFER_SIZE, "┝ %-12s │ %-12s | %-10s | %-10.1f | %-8d | %-15d | %-16d ┤\n", pszVideoSource, (AudioFormatField->valueint)?"Line in":"Embedd.", psztmp1, FramerateField->valuedouble, ChannelsField->valueint, BitsPerSampleField->valueint, SampleFrequencyField->valueint);
	        			snprintf(pszOutput5, BUFFER_SIZE, "┕-------------------------------------------------------------------------------------------------------┘\n\n");

	        			memset(&pszOutput, 0x00, BUFFER_SIZE);
	        			snprintf(pszOutput, BUFFER_SIZE, "%s%s%s%s%s", pszOutput1, pszOutput2, pszOutput3, pszOutput4, pszOutput5);

	        			printf("Command output:\n%s\n", pszOutput);/**/
		        	}

		        	*res = RES_SUCCESS;

		        	/**/if (write(Server->m_nClientSocket, pszOutput, strlen(pszOutput)) < 0) {
		                perror("ERROR writing to socket");
		                *res = RES_SEND_ERROR;
	        		}
	        		else {
	        			*res = RES_SUCCESS;
	        		}
		        }
		        else {
		        	printf("json may somthing wrong\n");

		        	*res = RES_NOT_EXIST;
		        }
        	}
	        else {
	        	printf("ptr_jsonReturn is NULL\n");

		       	*res = RES_NOT_EXIST;
	        }
	        break;

    	case RES_NOT_SUPPORT:
    	case RES_PARAMETER_ERROR:
    	case RES_NOT_EXIST:
    	case RES_BUSY:
    	case RES_NO_SPACE:
    	case RES_ERROR_PLATFORM:
    	case RES_SEND_ERROR:
    		break;
    }

	return;
}

void reboot(char** pars, int* res, void* pUserData) {
	printf("HIHIHIHIHI here is reboot\n");

	const char* pszCommand = "{\"qcapid\":\"53434352514B4352\", \"Function\":\"SYSTEM_REBOOT\"}";

	send_command(pszCommand);

    return;
}

void restore_to_default(char** pars, int* res, void* pUserData) {
	printf("HIHIHIHIHI here is restore_to_default\n");

	const char* pszCommand = "{\"qcapid\":\"53434352514B4352\", \"Function\":\"SYSTEM_RESTORE_DEFAULT\"}";

	send_command(pszCommand);

    return;
}

#endif