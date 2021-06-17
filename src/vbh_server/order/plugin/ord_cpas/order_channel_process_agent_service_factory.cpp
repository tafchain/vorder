#include "ord_cpas/order_channel_process_agent_service.h"
#include "ord_cpas/order_channel_process_agent_service_factory.h"



CChannelProcessAgentServiceFactory::CChannelProcessAgentServiceFactory(CChannelProcessAgentService* pChannelProcessAgentService)
	: m_pChannelProcessAgentService(pChannelProcessAgentService)
{
}

CDscService* CChannelProcessAgentServiceFactory::CreateDscService(void)
{
	return m_pChannelProcessAgentService;
}
