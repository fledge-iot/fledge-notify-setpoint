/*
 * FogLAMP Configuration Delivery plugin
 *
 * Copyright (c) 2020 Dianomic Systems
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
#include <plugin_exception.h>
#include <iostream>
#include <config_category.h>
#include "rapidjson/document.h"
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/error/en.h>
#include <rapidjson/writer.h>
#include <rapidjson/pointer.h>
#include <sstream>
#include <unistd.h>
#include <query.h>
#include <where.h>
#include <config.h>
#include <JSONPath.h>
#include <string_utils.h>

using namespace std;
using namespace rapidjson;

/**
 * Construct for ConfigJSONDelivery class
 *
 * @param category	The configuration of the plugin
 */
ConfigJSONDelivery::ConfigJSONDelivery(ConfigCategory *category)
{
	// Configuration set is protected by a lock
	lock_guard<mutex> guard(m_configMutex);

	// Create default values
	m_enable = false;

	// Set configuration
	this->configure(category);
}

/**
 * The destructor for the ConfigJSONDelivery class
 */
ConfigJSONDelivery::~ConfigJSONDelivery()
{
}

/**
 * Send a notification This simply sets a configuration option
 *
 * @param notificationName 	The name of this notification
 * @param triggerReason		Why the notification is being sent
 * @param message		The message to send
 */
bool ConfigJSONDelivery::notify(const string& notificationName,
			   const string& triggerReason,
			   const string& customMessage)
{
	Logger::getLogger()->info("Delivery plugin %s: "
				   "JSON trigger reason '%s'",
				   PLUGIN_NAME,
				   triggerReason.c_str());

	// Configuration fetch is protected by a mutex
	m_configMutex.lock();

	// Check for enable and for required clients
	if (!m_enable || !m_mngmtClient)
	{
		// Release lock	
		m_configMutex.unlock();
		return false;
	}

	/*
	 * Parse the triggerReason docuemnt and determine of this is a
	 * trigger event or a clear event. Then set the value accordingly
	 */
	string value;
	Document doc;
	doc.Parse(triggerReason.c_str());
	if (!doc.HasParseError())
	{
		if (doc.HasMember("reason"))
		{
			if (doc["reason"].IsString())
			{
				string reasonStr = doc["reason"].GetString();
				if (reasonStr.compare("triggered") == 0)
				{
					value = m_triggerValue;
				}
				else
				{
					value = m_clearValue;
				}
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
	string category = m_category;
	string item = m_item;

	// Release lock 
	m_configMutex.unlock();

	// Update the configuration item with the new value
	try {
		ConfigCategory config = m_mngmtClient->getCategory(category);
		if (config.isJSON(item))
		{
			string json = config.getValue(item);
			json = setValue(json, value);
			StringEscapeQuotes(json);
			string res = m_mngmtClient->setCategoryItemValue(category, item, json);
		}
		else
		{
			Logger::getLogger()->error("Configuration item %s is not a JSON item", item.c_str());
		}
		return true;
	}
	catch (exception &e) {
		Logger::getLogger()->error("Failed to set value %s for item %s in category %s, %s",
				value.c_str(), item.c_str(), category.c_str(), e.what());
		return false;
	}
}

/**
 * Reconfigure the delivery plugin
 *
 * @param newConfig	The new configuration
 */
void ConfigJSONDelivery::reconfigure(const string& newConfig)
{
	ConfigCategory category("new", newConfig);

	// Configuration change is protected by a lock
	lock_guard<mutex> guard(m_configMutex);

	// Set the new configuration
	this->configure(&category);
}

/**
 * Configure the delivery plugin
 *
 * @param category	The plugin configuration
 */
void ConfigJSONDelivery::configure(ConfigCategory *category)
{
	// Get the configuration category we are changing
	if (category->itemExists("category"))
	{
		m_category = category->getValue("category");
	}

	// Get the item in the configuration category
	if (category->itemExists("item"))
	{
		m_item = category->getValue("item");
	}

	// Get the JSON Path in the configuration item to set
	if (category->itemExists("path"))
	{
		m_path = category->getValue("path");
	}

	// Get the JSON property in the configuration item to set
	if (category->itemExists("property"))
	{
		m_property = category->getValue("property");
	}

	// Get value to set on triggering
	if (category->itemExists("triggerValue"))
	{
		m_triggerValue = category->getValue("triggerValue");
	}

	// Get value to set on clearing
	if (category->itemExists("clearValue"))
	{
		m_clearValue = category->getValue("clearValue");
	}

	if (category->itemExists("enable"))
	{
       		m_enable = category->getValue("enable").compare("true") == 0 ||
			   category->getValue("enable").compare("True") == 0;
	}
}

/**
 * Update the JSON configuration item and return the new item.
 *
 * Use the m_path to determine the item to update and then set it to value
 *
 * @param json	The JSON to modify
 * @param value	THe new value to set
 * @return	The updated JSON or throws a runtime error if the path was not matched
 */
string  ConfigJSONDelivery::setValue(string& json, const string& value)
{
	Document d;
	if (d.Parse(json.c_str()).HasParseError())
	{
		Logger::getLogger()->error("Failed to parse configuration item, %s at %u",
				GetParseError_En(d.GetParseError()), (unsigned)d.GetErrorOffset());
		Logger::getLogger()->error("Item is: '%s'", json.c_str());
		throw runtime_error("Invalid JSON in configuration item");
	}
	const char *val = value.c_str();

	JSONPath path(m_path);
	Value *jval = path.findNode(d);
	if (!jval->IsObject())
	{
		Logger::getLogger()->error("The path %s does not specify a JSON object", m_path.c_str());
		throw runtime_error("Path does not reference an object");
	}
	if (jval->HasMember(m_property.c_str()))
	{
		Value& item = (*jval)[m_property.c_str()];
		if (item.IsString())
		{
			item.SetString(value.c_str(), value.size(), d.GetAllocator());
		}
		else if (item.IsInt())
		{
			long ival = strtol(value.c_str(), NULL, 10);
			item.SetInt64(ival);
		}
		else if (item.IsDouble())
		{
			double dval = strtod(value.c_str(), NULL);
			item.SetDouble(dval);
		}
	}
	else
	{
		Logger::getLogger()->error("The path %s does not contain a property called %s",
			       	m_path.c_str(), m_property.c_str());
		throw runtime_error("Path does not contain configured item");
	}


	StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        d.Accept(writer);

        const char *config = buffer.GetString();
	return string(config);
}
