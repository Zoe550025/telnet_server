#define _NUMBER_OF_API 25


void handle_client(char data[], void* pUserData);
char* remove_new_line(char* str);
int send_command(const char* pszCommand);//, char** pszReturn
char* send_curl_command(const char* url, const char* pszCommand);
size_t curl_callback(void *contents, size_t size, size_t nmemb, char **response);

/**
 * System Function
 */
void help_direction(char** pars, int* res, void* pUserData);
void show_version(char** pars, int* res, void* pUserData);
void shapshot(char** pars, int* res, void* pUserData);
void set_record_status(char** pars, int* res, void* pUserData);
void set_stream_status(char** pars, int* res, void* pUserData);
void reboot(char** pars, int* res, void* pUserData);
void restore_to_default(char** pars, int* res, void* pUserData);

void get_input_format(char** pars, int* res, void* pUserData);

