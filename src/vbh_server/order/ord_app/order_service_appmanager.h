#ifndef ORDER_SERVICE_APPMANAGER_H_4EABB5B298A611E99DDD60F18A3A20D1
#define ORDER_SERVICE_APPMANAGER_H_4EABB5B298A611E99DDD60F18A3A20D1

#include "dsc/dsc_app_mng.h"

class COrderServiceAppManager : public CDscAppManager
{

protected:
	virtual ACE_INT32 OnInit(void);
	virtual ACE_INT32 OnExit(void);
};

#endif
