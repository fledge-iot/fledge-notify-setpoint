/*
 * FogLAMP notification delivery plugin that executes
 * setpoint control on a south service.
 *
 * Copyright (c) 2021 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <plugin_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string>
#include <logger.h>
#include <config_category.h>
#include <unistd.h>
#include "version.h"
#include <setpoint.h>

using namespace std;
using namespace rapidjson;

#define TRIGGERED	QUOTE({				\
			"values" : {			\
				"name" : "value"	\
			}				\
		})

#define CLEARED	 QUOTE({				\
			"values" : {			\
				"name" : "value"	\
				}			\
		})

/**
 * Plugin specific default configuration
 *
 * lists.
 */
static const char * default_config = QUOTE({
	"plugin": {
		"description": "Setpoint control of a south service",
		"type": "string",
		"default": PLUGIN_NAME,
		"readonly": "true" },
	"service": {
		"description": "The south service to control",
		"type": "string",
		"default": "",
		"order": "1",
		"displayName" : "Service" },
	"triggerValue": {
		"description": "The value to set in item when the notification triggers",
		"type": "JSON",
		"default": TRIGGERED,
		"order": "2",
		"displayName" : "Trigger Value" },
	"clearValue": {
		"displayName": "Cleared Value",
		"type": "JSON",
		"default": CLEARED,
		"order": "3",
		"description" : "The value to set in the item when the notification clears" },
	 "enable": {
		"description": "A switch that can be used to enable or disable execution of the notification plugin.",
		"type": "boolean",
		"default": "false",
		"displayName" : "Enabled" }
	});

/**
 * The SetPointDelivery plugin interface
 */
extern "C" {

/**
 * The C API plugin information structure
 */
static PLUGIN_INFORMATION info = {
	PLUGIN_NAME,				// Name
	VERSION,				// Version
	SP_GET_MANAGEMENT,			// Flags
	PLUGIN_TYPE_NOTIFICATION_DELIVERY,	// Type
	"1.0.0",				// Interface version
	default_config				// Configuration
};

/**
 * Return the information about this plugin
 */
PLUGIN_INFORMATION *plugin_info()
{
	return &info;
}

/**
 * Initialise the plugin with configuration.
 *
 * This funcion is called to get the plugin handle.
 */
PLUGIN_HANDLE plugin_init(ConfigCategory* configData)
{
	SetPointDelivery* handler = new SetPointDelivery(configData);

	return (PLUGIN_HANDLE)handler;
}

/**
 * Enable plugin features
 *
 * @param handle	The plugin handle returned from plugin_init
 */
void plugin_start(PLUGIN_HANDLE handle)
{
	SetPointDelivery* delivery = (SetPointDelivery *)handle;

	// Set the management service clients
	delivery->setManagementClient();

	return;
}

/**
 * Shutdown the plugin
 *
 * @param handle	The plugin handle returned from plugin_init
 */
void plugin_shutdown(PLUGIN_HANDLE handle)
{
	SetPointDelivery* delivery = (SetPointDelivery *)handle;

	// Delete plugin handle
	delete delivery;
}

/**
 * Deliver received notification data
 *
 * @param handle		The plugin handle returned from plugin_init
 * @param deliveryName		The delivery category name
 * @param notificationName	The notification name
 * @param triggerReason		The trigger reason for notification
 * @param customMessage		The message from notification
 * @return			True on success, false otherwise
 */
bool plugin_deliver(PLUGIN_HANDLE handle,
		    const std::string& deliveryName,
		    const std::string& notificationName,
		    const std::string& triggerReason,
		    const std::string& customMessage)
{

	Logger::getLogger()->debug("SetPointDelivery notification plugin_deliver(): "
				   "deliveryName=%s, notificationName=%s, "
				   "triggerReason=%s, message=%s",
				   deliveryName.c_str(),
				   notificationName.c_str(),
				   triggerReason.c_str(),
				   customMessage.c_str());

	SetPointDelivery *delivery = (SetPointDelivery *)handle;

	bool ret = delivery->notify(notificationName,
				    triggerReason,
				    customMessage);

	// Return error indicator
	return ret;
}

/**
 * Reconfigure the plugin
 *
 * @param handle	The plugin handle returned from plugin_init
 * @param newConfig	The new configuration to apply, as JSON string
 */
void plugin_reconfigure(PLUGIN_HANDLE *handle,
			string& newConfig)
{
	SetPointDelivery *delivery = (SetPointDelivery *)handle;
	delivery->reconfigure(newConfig);

	return;
}

/**
 * Register a callback function used to get the NotificationService ServiceHandler object
 *
 * @param handle	The plugin handle returned from plugin_init
 * @param func		The callback funcion to call
 * @service		Fist parameter to pass to the callback funcion
 */
void plugin_registerService(PLUGIN_HANDLE *handle, void *func, void *service)
{
	SetPointDelivery* delivery = (SetPointDelivery *)handle;

	// Set Notification server service handler by calling
	// the callback routine
	delivery->setService((FuncPtr)func, service);

	return;
}

// End of extern "C"
};
