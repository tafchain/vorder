#ifndef I_ORDER_CHANNEL_PROCESS_AGENT_SERVICE_H_9872134832176132742312136213
#define I_ORDER_CHANNEL_PROCESS_AGENT_SERVICE_H_9872134832176132742312136213

#include "vbh_comm/comm_msg_def/vbh_comm_msg_cps_cpas_def.h"
class IOrderChannelprocessAgentService
{

public:
	virtual void ReceiveRaftHeartBeat(VBH::CRaftHeartBeatCpsCpas& rRaftHeartBeatCpsCpas) = 0;
	virtual ACE_INT32 GetRaftVoteRsp(VBH::CRaftVoteCpsCpasReq& rRaftVoteCpsCpasReq, ACE_UINT16 &nResult) = 0;
};

#endif