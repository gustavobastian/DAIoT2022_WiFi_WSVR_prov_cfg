/* Various global declarations 

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PARAM_NAMESPACE "esp32_WS"

#define PROTO_TCP 6
#define PROTO_UDP 17

extern char* ssid;
extern char* passwd;

extern uint16_t connect_count;
extern bool ap_connect;

extern uint32_t my_ip;
extern uint32_t my_ap_ip;

void preprocess_string(char* str);
int set_sta(int argc, char **argv);

esp_err_t get_config_param_int(char* name, int* param);
esp_err_t get_config_param_str(char* name, char** param);

#ifdef __cplusplus
}
#endif
