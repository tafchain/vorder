#include "ord_cps/order_channel_process_service.h"
#include "ord_cps/order_channel_process_service_factory.h"

CChannelProcessServiceFactory::CChannelProcessServiceFactory(CChannelProcessService* pChannelProcessService)
	: m_pChannelProcessService(pChannelProcessService)
{
}

CDscService* CChannelProcessServiceFactory::CreateDscService(void)
{
	return m_pChannelProcessService;
}
