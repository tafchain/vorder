#ifndef ORDER_CHANNEL_PROCESS_AGENT_SERVICE_H_13456465496431268975616164964
#define ORDER_CHANNEL_PROCESS_AGENT_SERVICE_H_13456465496431268975616164964

#include "dsc/protocol/hts/dsc_hts_service.h"
#include "dsc/protocol/mcp/mcp_asynch_acceptor.h"
#include "vbh_comm/vbh_comm_error_code.h"
#include "vbh_comm/vbh_encrypt_lib.h"

#include "ord_cps/i_order_channel_process_service.h"
#include "ord_cpas/i_order_channel_process_agent_service.h"
#include "vbh_comm/comm_msg_def/vbh_comm_msg_cps_cpas_def.h"

class PLUGIN_EXPORT CChannelProcessAgentService  : public CDscHtsClientService, public IOrderChannelprocessAgentService, public CDscServiceTimerHandler
{
public:
	enum
	{
		EN_SERVICE_TYPE = VBH::EN_ORDER_CHANNEL_PROCESS_AGENT_SERVICE_TYPE,
		EN_SESSION_TIMEOUT_VALUE = 60,
		EN_SESSION_MAP_SIZE_BITS = 16,
	};

	enum VoteState  //集群的状态
	{
		EN_VOTE_FOLLOWER_STATE = 1, //follower
		EN_VOTE_CANDIDATE_STATE, //candidate
		EN_VOTE_LEADER_STATE // leader
	};

	enum TimerType  //集群的状态
	{
		EN_VOTE_TIMER = 1, //选举定时器
		EN_HEART_BEAT_TIMER, //心跳定时器
	};
	class CCpsHandleInfo
	{
	public:
		DSC_BIND_ATTR(m_nHandleID, m_nHandleState);

	public:
		ACE_UINT32 m_nHandleID; //nHandleID
		ACE_UINT32 m_nHandleState = 0; //默认为零
	};

	class CRaftVoteInfo
	{
	public:
		DSC_BIND_ATTR( m_nTerm, m_nLeaderId, m_nVoteNum, m_nTimeLong);

	public:
		ACE_UINT64 m_nTerm; //当前领导任期
		ACE_UINT16 m_nLeaderId; //当前领导ID
		ACE_UINT16 m_nVoteNum;
		ACE_INT64  m_nTimeLong;
	};


public:
	CChannelProcessAgentService(CDscString strIpAddr, ACE_INT32 nPort, ACE_UINT32 nChannelID, ACE_UINT16 nCpsId);

public:
	virtual ACE_INT32 OnInit(void) override;
	virtual ACE_INT32 OnExit(void) override;

protected:
	BEGIN_HTS_MESSAGE_BIND
		BIND_HTS_MESSAGE(VBH::CRaftVoteCpsCpasRsp)
		END_HTS_MESSAGE_BIND

public:
	ACE_INT32 OnHtsMsg(VBH::CRaftVoteCpsCpasRsp& rRaftVoteCpsCpasRsp, CMcpHandler* pMcpHandler);
	inline void SetChannelProcessService(IOrderChannelProcessService* pOrderChannelprocessService);

protected:
	virtual ACE_INT32 OnConnectedNodify(CMcpClientHandler* pMcpClientHandler) override;
	virtual void ReceiveRaftHeartBeat(VBH::CRaftHeartBeatCpsCpas& rRaftHeartBeatCpsCpas) override;
	virtual ACE_INT32 GetRaftVoteRsp(VBH::CRaftVoteCpsCpasReq& rRaftVoteCpsCpasReq, ACE_UINT16& nResult) override;
	virtual void OnNetworkError(CMcpHandler* pMcpHandler) override;

private:
	void RaftHeartBeatTimeOut(void);
	void RaftVoteTimeOut(void);
	void SendHeartBeatToAllServers(void);

	void OnTimer(void) final;


private:
	const ACE_UINT32 m_nChannelID;
	const ACE_UINT16 m_nCpsId;
	const CDscString m_strIpAddr;
	const ACE_INT32 m_nPort;
	CRaftVoteInfo m_raftVoteInfo;
	VoteState m_voteState;
	ACE_UINT16 m_nKafkaValue;//需要收到的投票数

private:
	dsc_vector_type(CCpsHandleInfo) m_vecCpsHandleInfo;//和cps的handle-id
	IOrderChannelProcessService* m_pCps = nullptr;
	TimerType m_eTimerType = EN_VOTE_TIMER;
	ACE_UINT16 m_startTimerFlag = 0;

};
#include "ord_cpas/order_channel_process_agent_service.inl"

#endif
