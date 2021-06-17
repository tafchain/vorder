#include "dsc/dsc_comm_def.h"

#include "order_service_appmanager.h"
#include "vbh_comm/vbh_encrypt_lib.h"

#ifdef DSC_TEST
#include "ord_cps/order_channel_process_service_plugin.h"
#endif

ACE_INT32 COrderServiceAppManager::OnInit()
{
	VBH::InitOpenSsl();
	DSC_FORWARD_CALL(CDscAppManager::OnInit() );

#ifdef DSC_TEST
	//·½±ãvaldring¼ì²â£¬¾²Ì¬¼ÓÔØ²å¼þ
	CChannelProcessServicePlugin cpsPlugin;

	cpsPlugin.OnInit();
#endif

	return 0;
}

ACE_INT32 COrderServiceAppManager::OnExit()
{
	DSC_FORWARD_CALL(CDscAppManager::OnExit());

	return 0;
}
