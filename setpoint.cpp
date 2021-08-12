/*
 * FogLAMP SetPoint Delivery plugin
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
#include <setpoint.h>
#include <string_utils.h>
#include <service_record.h>

using namespace std;
using namespace rapidjson;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

/**
 * Construct for SetPointDelivery class
 *
 * @param category	The configuration of the plugin
 */
SetPointDelivery::SetPointDelivery(ConfigCategory *category)
{
	// Configuration set is protected by a lock
	lock_guard<mutex> guard(m_configMutex);

	// Create default values
	m_enable = false;

	// Set configuration
	this->configure(category);
}

/**
 * The destructor for the SetPointDelivery class
 */
SetPointDelivery::~SetPointDelivery()
{
}

/**
 * Send a notification This simply sets a configuration option
 *
 * @param notificationName 	The name of this notification
 * @param triggerReason		Why the notification is being sent
 * @param message		The message to send
 */
bool SetPointDelivery::notify(const string& notificationName,
			   const string& triggerReason,
			   const string& customMessage)
{
	Logger::getLogger()->info("Delivery plugin %s: "
				   "trigger reason '%s'",
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
		if (doc.HasMember("data") && doc["data"].IsObject())
		{
			dataSubstitution(value, doc["data"].GetObject());
		}
	}
	else
	{
		return false;
	}

	// Release lock 
	m_configMutex.unlock();

	// Send the control message to the south service
	try {
		// TODO the real work
		ServiceRecord service(m_southService);
		if (!m_mngmtClient->getService(service))
		{
			Logger::getLogger()->error("Unable to find service '%s'", m_southService.c_str());
			return false;
		}
		string address = service.getAddress();
		unsigned short port = service.getPort();
		char addressAndPort[80];
		snprintf(addressAndPort, sizeof(addressAndPort), "%s:%d", address.c_str(), port);
		SimpleWeb::Client<SimpleWeb::HTTP> http(addressAndPort);

		string url = "/fledge/south/setpoint";
		try {
			SimpleWeb::CaseInsensitiveMultimap headers = {{"Content-Type", "application/json"}};
			auto res = http.request("PUT", url, value, headers);
			if (res->status_code.compare("200 OK"))
			{
				Logger::getLogger()->error("Failed to send set point operation to service %s, %s",
							m_southService.c_str(), res->status_code.c_str());
				return false;
			}
		} catch (exception& e) {
			Logger::getLogger()->error("Failed to send set point operation to service %s, %s",
						m_southService.c_str(), e.what());
			return false;
		}

		return true;
	}
	catch (exception &e) {
		Logger::getLogger()->error("Failed to send set point operation to service %s, %s",
				m_southService.c_str(), e.what());
		return false;
	}
}

/**
 * Reconfigure the delivery plugin
 *
 * @param newConfig	The new configuration
 */
void SetPointDelivery::reconfigure(const string& newConfig)
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
void SetPointDelivery::configure(ConfigCategory *category)
{
	// Get the configuration category we are changing
	if (category->itemExists("service"))
	{
		m_southService = category->getValue("service");
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
 * Substitute varaibles with reading data
 */
void SetPointDelivery::dataSubstitution(string& message, const Value& obj)
{
	string rval("");
	size_t p1 = 0;

	size_t dstart;
	while ((dstart = message.find_first_of("$", p1)) != string::npos)
	{
		rval.append(message.substr(p1, dstart));
		dstart++;
		size_t dend = message.find_first_of ("$", dstart);
		if (dend != string::npos)
		{
			string var = message.substr(dstart, dend - dstart);
			size_t p2 = var.find_first_of(".");
			string asset = var.substr(0, p2);
			string datapoint = var.substr(p2 + 1);
			if (obj.HasMember(asset.c_str()) && obj[asset.c_str()].IsObject())
			{
				const Value& dp = obj[asset.c_str()];
				if (dp.HasMember(datapoint.c_str()))
				{
					const Value& dpv = dp[datapoint.c_str()];
					if (dpv.IsString())
					{
						rval.append(dpv.GetString());
					}
					else if (dpv.IsDouble())
					{
						char buf[40];
						snprintf(buf, sizeof(buf), "%f", dpv.GetDouble());
						rval.append(buf);
					}
					else if (dpv.IsInt64())
					{
						char buf[40];
						snprintf(buf, sizeof(buf), "%ld", dpv.GetInt64());
						rval.append(buf);
					}
				}
				else
				{
					Logger::getLogger()->error("There is no datapoint '%s' in the '%s asset received",
						       datapoint.c_str(), asset.c_str());
				}
			}
			else
			{
				Logger::getLogger()->error("There is no asset '%s' in the data received", asset.c_str());
			}
		}
		else
		{
			Logger::getLogger()->error("Unterminated macro substitution in '%s':%ld", message.c_str(), p1);
		}
		p1 = dend + 1;
	}
	rval.append(message.substr(p1));

	Logger::getLogger()->debug("'%s'", message.c_str());
	Logger::getLogger()->debug("became '%s'", rval.c_str());
	message = rval;
}
