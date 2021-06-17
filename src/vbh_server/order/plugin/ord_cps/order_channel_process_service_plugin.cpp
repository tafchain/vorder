#include "dsc/service/dsc_service_container.h"
#include "dsc/db/per/persistence.h"
#include "dsc/dsc_database_factory.h"

#include "vbh_comm/vbh_comm_func.h"

#include "dsc/dispatcher/dsc_dispatcher_center.h"


#include "ord_cps/order_channel_process_service.h"
#include "ord_cps/order_channel_process_service_factory.h"
#include "ord_cps/order_channel_process_service_plugin.h"
#include "ord_cpas/order_channel_process_agent_service.h"
#include "ord_cps/order_channel_process_service.h"
#include "ord_cpas/order_channel_process_agent_service_factory.h"


class CCpsCfg
{
public:
	CCpsCfg()
	: m_cpsID("CPS_ID")
	, m_ipAddr("CPS_IP_ADDR")
	, m_port("CPS_PORT")
	, m_channelID("CH_ID")
	{
	}

public:
	PER_BIND_ATTR(m_cpsID, m_ipAddr, m_port, m_channelID);

public:
	CColumnWrapper< ACE_INT32 > m_cpsID;
	CColumnWrapper< CDscString > m_ipAddr;
	CColumnWrapper< ACE_INT32 > m_port;
	CColumnWrapper< ACE_UINT32 > m_channelID;
};

class CCpsCfgCriterion : public CSelectCriterion
{
public:
	virtual void SetCriterion(CPerSelect& rPerSelect)
	{
		rPerSelect.Where(rPerSelect["NODE_ID"] == CDscAppManager::Instance()->GetNodeID());
	}
};

CChannelProcessServicePlugin::CChannelProcessServicePlugin()
{
}

ACE_INT32 CChannelProcessServicePlugin::OnInit(void)
{
	CDscDatabase database;
	CDBConnection dbConnection;

	if (CDscDatabaseFactoryDemon::instance()->CreateDatabase(database, dbConnection))
	{
		DSC_RUN_LOG_ERROR("connect database failed.");

		return -1;
	}

	CTableWrapper< CCollectWrapper<CCpsCfg> > lstCpsCfg("CPS_CFG");
	CCpsCfgCriterion criterion;

	if (::PerSelect(lstCpsCfg, database, dbConnection, &criterion))
	{
		DSC_RUN_LOG_ERROR("select from CPS_CFG failed");

		return -1;
	}

	RegistDscReactorServiceContainer(NULL, VBH::EN_ORDER_CHANNEL_PROCESS_SERVICE_CONTAINER_TYPE, lstCpsCfg->size());


	CChannelProcessService* pChannelProcessService;
	CDscReactorServiceContainerFactory dscReactorServiceContainerFactory;
	ACE_UINT16 nContainerID = 1;
	CChannelProcessAgentService* pChannelProcessAgentService;

	for (auto it = lstCpsCfg->begin(); it != lstCpsCfg->end(); ++it)
	{

		//2.×¢²ácontainer
		CDscDispatcherCenterDemon::instance()->AcquireWrite();
		IDscTask* pDscServiceContainer = CDscDispatcherCenterDemon::instance()->GetDscTask_i(VBH::EN_ORDER_CHANNEL_PROCESS_SERVICE_CONTAINER_TYPE, nContainerID);
		if (!pDscServiceContainer)
		{
			pDscServiceContainer = dscReactorServiceContainerFactory.CreateDscServiceContainer();
			if (CDscDispatcherCenterDemon::instance()->RegistDscTask_i(pDscServiceContainer, VBH::EN_ORDER_CHANNEL_PROCESS_SERVICE_CONTAINER_TYPE, nContainerID))
			{
				DSC_RUN_LOG_ERROR("regist cps container error, type:%d, id:%d.", VBH::EN_ORDER_CHANNEL_PROCESS_SERVICE_CONTAINER_TYPE, nContainerID);
				CDscDispatcherCenterDemon::instance()->Release();

				return -1;
			}
		}
		if (!pDscServiceContainer)
		{
			DSC_RUN_LOG_ERROR("cann't create container.");
			CDscDispatcherCenterDemon::instance()->Release();

			return -1;
		}
		CDscDispatcherCenterDemon::instance()->Release();
		++nContainerID;

		const ACE_UINT16 nCpsID = (ACE_UINT16)*it->m_cpsID;

		DSC_NEW(pChannelProcessService, CChannelProcessService(*it->m_ipAddr, *it->m_port, *it->m_channelID));
		pChannelProcessService->SetType(CChannelProcessService::EN_SERVICE_TYPE);
		pChannelProcessService->SetID(nCpsID);

		DSC_NEW(pChannelProcessAgentService, CChannelProcessAgentService(*it->m_ipAddr, *it->m_port, *it->m_channelID, nCpsID));
		pChannelProcessAgentService->SetType(CChannelProcessAgentService::EN_SERVICE_TYPE);
		pChannelProcessAgentService->SetID(nCpsID);

		pChannelProcessService->SetChannelprocessAgentService(pChannelProcessAgentService);
		pChannelProcessAgentService->SetChannelProcessService(pChannelProcessService);


		CChannelProcessServiceFactory cpsFactory(pChannelProcessService);
		CDscSynchCtrlMsg ctrlCcFactory(DSC::EN_REGIST_SERVICE_CONTAINER_MSG, &cpsFactory);

		if (pDscServiceContainer->PostDscMessage(&ctrlCcFactory))
		{
			DSC_RUN_LOG_ERROR("Failed to push queue at container:%d.", VBH::EN_ORDER_CHANNEL_PROCESS_SERVICE_CONTAINER_TYPE);

			return -1;
		}


		//2.×¢²ácontainer
		CDscDispatcherCenterDemon::instance()->AcquireWrite();
		IDscTask* pDscServiceContainer1 = CDscDispatcherCenterDemon::instance()->GetDscTask_i(VBH::EN_ORDER_CHANNEL_PROCESS_SERVICE_CONTAINER_TYPE, nContainerID);
		if (!pDscServiceContainer1)
		{
			pDscServiceContainer1 = dscReactorServiceContainerFactory.CreateDscServiceContainer();
			if (CDscDispatcherCenterDemon::instance()->RegistDscTask_i(pDscServiceContainer1, VBH::EN_ORDER_CHANNEL_PROCESS_SERVICE_CONTAINER_TYPE, nContainerID))
			{
				DSC_RUN_LOG_ERROR("regist cps container error, type:%d, id:%d.", VBH::EN_ORDER_CHANNEL_PROCESS_SERVICE_CONTAINER_TYPE, nContainerID);
				CDscDispatcherCenterDemon::instance()->Release();

				return -1;
			}
		}
		if (!pDscServiceContainer1)
		{
			DSC_RUN_LOG_ERROR("cann't create container.");
			CDscDispatcherCenterDemon::instance()->Release();

			return -1;
		}
		CDscDispatcherCenterDemon::instance()->Release();
		++nContainerID;

		CChannelProcessAgentServiceFactory agentServiceFactory(pChannelProcessAgentService);
		CDscSynchCtrlMsg ctrlCcAgentFactory(DSC::EN_REGIST_SERVICE_CONTAINER_MSG, &agentServiceFactory);

		if (pDscServiceContainer1->PostDscMessage(&ctrlCcAgentFactory))
		{
			DSC_RUN_LOG_ERROR("Failed to push queue at container:%d.", VBH::EN_ORDER_CHANNEL_PROCESS_SERVICE_CONTAINER_TYPE);

			return -1;
		}

	}

	return 0;
}


#ifndef DSC_TEST
extern "C" PLUGIN_EXPORT void* CreateDscPlugin(void)
{
	CChannelProcessServicePlugin* pPlugIn = NULL;

	DSC_NEW(pPlugIn, CChannelProcessServicePlugin);

	return pPlugIn;
}
#endif
