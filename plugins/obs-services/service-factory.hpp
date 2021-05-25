#pragma once

#include <string>
#include <vector>
#include <map>

extern "C" {
#include <obs-module.h>
#include <jansson.h>
}

struct server_t {
	const char *protocol;
	const char *url;
	const char *name;
};

class service_factory {
	obs_service_info _info = {};

	std::string _id;
	std::string _name;
	std::vector<std::string> protocols;
	std::vector<server_t> servers;

	void create_server_lists(obs_properties_t *props);

	static const char *_get_name(void *type_data) noexcept;

	static void *_create(obs_data_t *settings,
			     obs_service_t *service) noexcept;
	static void _destroy(void *data) noexcept;

	static void _update(void *data, obs_data_t *settings) noexcept;

	static void _get_defaults2(obs_data_t *settings,
				   void *type_data) noexcept;

	static obs_properties_t *_get_properties2(void *data,
						  void *type_data) noexcept;

	static const char *_get_protocol(void *data) noexcept;
	static const char *_get_url(void *data) noexcept;

public:
	service_factory(json_t *service);
	~service_factory();

	const char *get_name();

	virtual void *create(obs_data_t *settings, obs_service_t *service);

	virtual void get_defaults2(obs_data_t *settings);

	virtual obs_properties_t *get_properties2(void *data);
};
