#ifndef I_ORDER_CHANNEL_PROCESS_SERVICE_H_7887978967654254343134674656
#define I_ORDER_CHANNEL_PROCESS_SERVICE_H_7887978967654254343134674656



class IOrderChannelProcessService
{

public:
	virtual void ChangeCpsToMasterState(void) = 0;
	virtual void ChangeCpsToFollowerState(void) = 0;
};

#endif