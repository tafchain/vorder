#ifndef ORDER_CHANNEL_PROCESS_AGENT_SERVICE_FACTORY_H_12549797654674961316546498
#define ORDER_CHANNEL_PROCESS_AGENT_SERVICE_FACTORY_H_12549797654674961316546498

#include "dsc/service/dsc_service_container.h"


class CChannelProcessAgentServiceFactory : public IDscServiceFactory
{
public:
	CChannelProcessAgentServiceFactory(CChannelProcessAgentService* pChannelProcessAgentService);

public:
	virtual CDscService* CreateDscService(void);

private:
	CChannelProcessAgentService* m_pChannelProcessAgentService;
};
#endif