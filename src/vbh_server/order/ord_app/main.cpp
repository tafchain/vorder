#include "ace/OS_main.h"
#include "ace/OS_NS_stdio.h"

#include "order_service_appmanager.h"
#include "vbh_comm/vbh_comm_id_def.h"

int ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{ 
	COrderServiceAppManager* pOrderServiceAppManager = ::new(std::nothrow) COrderServiceAppManager;
	if(!pOrderServiceAppManager) 
	{ 
		ACE_OS::printf("failed to new order service appmanager!"); 
		
		return -1; 
	} 

	pOrderServiceAppManager->SetNodeType(VBH::EN_ORDER_APP_TYPE);
	if( pOrderServiceAppManager->Init(argc, argv) ) 
	{ 
		ACE_OS::printf("order service appmanager init failed, now exit!\n"); 
		pOrderServiceAppManager->Exit();
		delete pOrderServiceAppManager;

		return -1; 
	} 
	
	ACE_OS::printf("order service appmanager init succeed, running...\n"); 
	pOrderServiceAppManager->Run_Loop(); 
	delete pOrderServiceAppManager;
	ACE_OS::printf("order service appmanager terminated!\n"); 
	
	return 0; 
}