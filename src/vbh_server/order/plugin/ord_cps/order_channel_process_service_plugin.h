#ifndef ORDER_CHANNEL_PROCESS_SERVICE_PLUGIN_H_4CE9CBBA98DD11E98C1A60F18A3A20D1
#define ORDER_CHANNEL_PROCESS_SERVICE_PLUGIN_H_4CE9CBBA98DD11E98C1A60F18A3A20D1

#include "dsc/plugin/i_dsc_plugin.h"

class CChannelProcessServicePlugin : public IDscPlugin
{

public:
	CChannelProcessServicePlugin();

public:
	ACE_INT32 OnInit(void);


};
#endif