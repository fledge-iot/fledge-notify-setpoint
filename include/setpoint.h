#ifndef _SETPOINT_H
#define _SETPOINT_H

#include <string>
#include <management_client.h>
#include <notification_service.h>
#include <mutex>

#define PLUGIN_NAME "setpoint"

typedef NotificationService* (*FuncPtr)(void *);

/**
 * setPointDelivery class that changes the configuration of an item within FogLAMP
 */
class SetPointDelivery
{       
	public: 
		SetPointDelivery(ConfigCategory *config);
		~SetPointDelivery();

		bool	notify(const std::string& notificationName,
				const std::string& triggerReason,
				const std::string& message);
		void	configure(ConfigCategory *config);
		void	reconfigure(const std::string& newConfig);
		void	disable() { m_enable = false; };
		// Get and set NotificationServer service handler
		void    setService(FuncPtr getService, void* service)
		{
			// Call the callback routine and set the NotificationServer
			// service handler
			m_service = (NotificationService *)(*getService)(service);;
		};
		// Set management client
		void setManagementClient()
		{
			if (m_service)
				m_mngmtClient = m_service->getMgmtClient();
			else
				m_mngmtClient = NULL;
		}	
		void	configLock() { m_configMutex.lock(); };
		void	configUnlock() { m_configMutex.unlock(); };


	private:
		void 		dataSubstitution(string& message, const Value& obj);
		bool		m_enable;
		std::string	m_southService;
		std::string	m_triggerValue;
		std::string	m_clearValue;

		// Configuration mutex
		std::mutex	m_configMutex;

		// Notification server internals 
		NotificationService*
				m_service;
		ManagementClient*
				m_mngmtClient;
};
#endif
