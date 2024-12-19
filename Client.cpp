#include "Client.h"

#include <stdexcept>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <mqtt_client.h>

#include "Subscription.h"

static constexpr const char* TAG = "MqttCpp";

MqttCpp::Client::Client (void) {
	const esp_mqtt_client_config_t mqtt_cfg = {
		.broker = {
			.address = {
				.uri = "mqtt://127.0.0.1:1883",
			},
			.verification = {},
		},
		.credentials = {},
		.session = {
			.last_will = {/*
				.topic = "",
				.msg = "",
				.qos = 0,
				.retain = 1,*/
			},
			.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
		},
		.network = {},
		.task = {},
		.buffer = {},
		.outbox = {},
	};
	this->_handle = esp_mqtt_client_init (&mqtt_cfg);
	if (this->_handle == nullptr) throw std::runtime_error ("mqtt initialization failed.");
	if (esp_mqtt_client_register_event (this->_handle, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), MqttCpp::Client::eventHandler, this) != ESP_OK)
		ESP_LOGW (TAG, "Can not register event handler.");
}

MqttCpp::Client::~Client (void) {
	if (this->_connected) esp_mqtt_client_disconnect (this->_handle);
	esp_mqtt_client_stop (this->_handle);
	esp_mqtt_client_destroy (this->_handle);

	this->_connected = false;

	for (const Subscription* s : this->_subscriptions) {
		delete s;
	}
	this->_subscriptions.clear ();
}

bool MqttCpp::Client::setUri (const char* uri) {
	ESP_LOGD (TAG, "Set mqtt broker uri to: \"%s\".", uri);
	if (this->_connected) this->disconnect ();
	esp_err_t ret;
	if ((ret = esp_mqtt_client_set_uri (this->_handle, uri)) == ESP_OK) return true;
	ESP_LOGW (TAG, "mqtt can not set broker uri to \"%s\" (%d).", uri, ret);
	return false;
}

bool MqttCpp::Client::connect (void) {
	if (this->_connected) return false;
	ESP_LOGD (TAG, "Try to connect to broker");

	esp_err_t ret;
	if ((ret = esp_mqtt_client_start (this->_handle)) == ESP_OK) {
		//this->_connected = true;
		return true;
	}
	if (ret == ESP_ERR_INVALID_ARG)
		ESP_LOGE (TAG, "mqtt can not start: wrong initialization (%d).", ret);
	else if (ret == ESP_FAIL)
		ESP_LOGE (TAG, "mqtt can not start: unknown error (%d).", ret);
	throw std::runtime_error ("mqtt can not start.");
}

bool MqttCpp::Client::disconnect (void) {
	if (!this->_connected) return false;
	if (esp_mqtt_client_disconnect (this->_handle) == ESP_OK) return true;
	return false;
}

bool MqttCpp::Client::publish (const char* const topic, const char* const msg, const int& msg_len, const int& qos, const bool& retain, const bool& block) {
	ESP_LOGD (TAG, "Publish topic \"%s\".", topic);

	if (!this->_connected) return false;
	if (topic == nullptr || msg == nullptr) return false;

	int ret;
	int tries = 0;
	do {
		if (block)
			ret = esp_mqtt_client_publish (this->_handle, topic, msg, msg_len, qos, retain);
		else
			ret = esp_mqtt_client_enqueue (this->_handle, topic, msg, msg_len, qos, retain, true);
		++tries;
		if (ret == -2) {
			if (tries > 7)
				break;
			else if (tries > 3)
				vTaskDelay (pdMS_TO_TICKS (1000)); // Outbox is still full, wait 1s and try again.
			else
				vTaskDelay (pdMS_TO_TICKS (100)); // Outbox is full, wait 100ms and try again.

			ESP_LOGI (TAG, "Outbox is full, attempt #%d", tries + 1);
		} else if (ret >= 0)
			return true;
		break;
	} while (true);

	return false;
}

bool MqttCpp::Client::subscribe (const char* const topic, const Callback callback, const int& qos, void* user_data) {
	ESP_LOGD (TAG, "Subscribe topic \"%s\".", topic);

	if (!this->_connected) return false;
	if (topic == nullptr) return false;

	const Subscription* s = new Subscription (topic, callback, qos, user_data);
	this->_subscriptions.push_front (s);

	int tries = 0;
	int ret;
	while ((ret = esp_mqtt_client_subscribe_single (this->_handle, s->_topic, s->_qos) == -2) && tries < 6) {
		ESP_LOGI (TAG, "Outbox is full, attempt #%d", tries + 1);
		if (tries > 2)
			vTaskDelay (pdMS_TO_TICKS (1000)); // Outbox is still full, wait 1s and try again.
		else
			vTaskDelay (pdMS_TO_TICKS (100)); // Outbox is full, wait 100ms and try again.
		++tries;
	}

	if (ret >= 0) return true;
	else
		ESP_LOGW (TAG, "Can not subscribe to topic (%d).", ret);
	return false;
}

void MqttCpp::Client::eventHandler (void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
	ESP_LOGD (TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
	MqttCpp::Client* mqtt = static_cast<MqttCpp::Client*>(handler_args);
	esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
	//esp_mqtt_client_handle_t client = event->client;
	//int msg_id;
	switch (event->event_id) {
		case MQTT_EVENT_BEFORE_CONNECT:
			ESP_LOGV (TAG, "Client is initialized and about to start connecting to the broker.");
			break;
		case MQTT_EVENT_CONNECTED:
			ESP_LOGV (TAG, "Client has successfully established a connection to the broker.");
			mqtt->_connected = true;
			break;
		case MQTT_EVENT_DISCONNECTED:
			ESP_LOGI (TAG, "Client has aborted the connection.");
			mqtt->_connected = false;
			break;
		case MQTT_EVENT_SUBSCRIBED:
			ESP_LOGV (TAG, "Broker has acknowledged the client's subscribe request for message id: %d.", event->msg_id);
			break;
		case MQTT_EVENT_UNSUBSCRIBED:
			ESP_LOGV (TAG, "Broker has acknowledged the client's unsubscribe request for message id: %d.", event->msg_id);
			break;
		case MQTT_EVENT_PUBLISHED:
			ESP_LOGV (TAG, "Broker has acknowledged the client's publish message for message id: %d.", event->msg_id);
			break;
		case MQTT_EVENT_DATA:
			ESP_LOGV (TAG, "Client has received a publish message with id: %d from topic: \"%s\".", event->msg_id, event->topic);
			for (const Subscription* subscription : mqtt->_subscriptions) {
				if (Client::matchTopic (subscription->_topic, subscription->_topic_len, event->topic, event->topic_len)) {
					subscription->_callback (event->topic, event->topic_len, event->msg_id, event->data, event->total_data_len, event->qos, subscription->_user_data);
				}
			}
			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGV (TAG, "Client has encountered an error.");
			break;
		default:
			ESP_LOGW (TAG, "Unknown event type (%ld)", event_id);
			break;
	}
}

bool MqttCpp::Client::matchTopic (const char* const str1, const int& str1_len, const char* const str2, const int& str2_len) noexcept {
	if (str1 == nullptr || str2 == nullptr) return false;

	int i1 = 0;
	int i2 = 0;
	bool start = false;

	while ((i1 < str1_len || (str1_len == -1 ? (str1[i1] != '\0') : false)) && (i2 < str2_len || (str2_len == -1 ? (str2[i2] != '\0') : false))) {
		//printf("str1[%d] = %c; str2[%d] = %c\n", i1, str1[i1], i2, str2[i2]);
		if (str1[i1] == str2[i2]) {

			if (str1[i1] == '/') {
				//printf ("new tag\n");
				start = true;
				++i1;
				++i2;
				continue;
			}
			++i1;
			++i2;
		} else if (start && str1[i1] == '*') {
			//printf ("wildcard\n");
			while ((i2 < str2_len || (str2_len == -1 ? (str2[i2] != '\0') : false)) && str2[i2] != '/') {
				++i2;
			}
			++i1;
			if ((i1 == str1_len || (str1_len == -1 ? (str1[i1] == '\0') : false)) || (i2 == str2_len || (str2_len == -1 ? (str2[i1] == '\0') : false))) return true;
		} else
			return false;
		start = false;
	}
	if ((i1 == str1_len || (str1_len == -1 ? (str1[i1] == '\0') : false)) && (i2 == str2_len || (str2_len == -1 ? (str2[i2] == '\0') : false))) return true;
	return false;
}
