#ifndef WIFI_CLIENT_H
#define WIFI_CLIENT_H

#include <string>
#include "esp_wifi.h"


class WifiClient {
	WifiClient() {};
	~WifiClient() {};

public:
	static void scan();
	static bool client_connected();
	static void start();
	static inline bool isConnected() {return cl_connected;} ;

private:
	static void tcp_client(void *pvParam);
	static void initialise_wifi(void);
	static void wifi_connect();
	// static esp_err_t event_handler(void *ctx, system_event_t *event);


	static esp_netif_t *sta_netif;
	static std::string SSID;
	static bool cl_connected;
	static EventGroupHandle_t wifi_event_group;


};

#endif
