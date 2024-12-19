#ifndef MQTTCPP_CLIENT_H
#define MQTTCPP_CLIENT_H

#include <forward_list>

#include <mqtt_client.h>

#include "typedef.h"

namespace MqttCpp {
	class Subscription;

	class Client {
	private:
		esp_mqtt_client_handle_t _handle;
		bool _connected = false;
		std::forward_list<const Subscription*> _subscriptions;

	public:
		Client (void);
		~Client (void);

		bool setUri (const char* uri);
		bool connect (void);
		bool disconnect (void);
		bool publish (const char* const topic, const char* const msg, const int& msg_len, const int& qos = 1, const bool& retain = false, const bool& block = false);
		bool subscribe (const char* const topic, const Callback callback, const int& qos, void* user_data);

		inline bool isConnected (void) const noexcept {
			return this->_connected;
		}

	private:
		/*
		 * @brief Event handler registered to receive MQTT events
		 *
		 *  This function is called by the MQTT client event loop.
		 *
		 * @param handler_args user data registered to the event.
		 * @param base Event base for the handler(always MQTT Base in this example).
		 * @param event_id The id for the received event.
		 * @param event_data The data for the event, esp_mqtt_event_handle_t.
		 */
		static void eventHandler (void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
		static bool matchTopic (const char* const str1, const int& str1_len, const char* const str2, const int& str2_len) noexcept;
	};
}

#endif /* MQTTCPP_CLIENT_H */
