#ifndef MQTTCPP_SUBSCRIPTIONT_H
#define MQTTCPP_SUBSCRIPTIONT_H

#include <cstdint>

#include <MqttCpp.h>

namespace MqttCpp {
	struct Subscription {
		const char* _topic = nullptr;
		uint16_t _topic_len = 0;
		const Callback _callback;
		const uint8_t _qos = 2;
		void* _user_data = nullptr;

		Subscription (const char* const topic, const Callback callback, const uint8_t& qos = 2, void* user_data = nullptr) : _callback(callback), _qos(qos), _user_data(user_data) {
			this->_topic = strdup (topic);
			this->_topic_len = strlen (this->_topic);
		}

		~Subscription (void) {
			free (const_cast<char*>(this->_topic));
			this->_topic = nullptr;
			this->_user_data = nullptr;
		}
	};
}

#endif /* MQTTCPP_SUBSCRIPTIONT_H */
