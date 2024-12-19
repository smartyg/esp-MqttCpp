#ifndef MQTTCPP_TYPEDEF_H
#define MQTTCPP_TYPEDEF_H

#include <functional>

namespace MqttCpp {
	inline namespace types {
		typedef std::function<void(const char* const topic, const int& topic_len, const int& msg_id, const char* const data, const int& total_data_len, const int& qos, void* user_data)> Callback;
	}
}

#endif /* MQTTCPP_TYPEDEF_H */
