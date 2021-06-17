#ifndef ORDER_CHANNEL_PROCESS_SERVICE_FACTORY_H_789984454657431354354468753
#define ORDER_CHANNEL_PROCESS_SERVICE_FACTORY_H_789984454657431354354468753

#include "dsc/service/dsc_service_container.h"


class CChannelProcessServiceFactory : public IDscServiceFactory
{
public:
	CChannelProcessServiceFactory(CChannelProcessService* pChannelProcessService);

public:
	virtual CDscService* CreateDscService(void);

private:
	CChannelProcessService* m_pChannelProcessService;
};
#endif