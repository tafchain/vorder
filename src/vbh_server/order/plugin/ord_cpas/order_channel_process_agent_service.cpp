
#include "ace/OS_NS_sys_stat.h"

#include "dsc/configure/dsc_configure.h"
#include "dsc/dsc_log.h"
#include "dsc/codec/dsc_codec/dsc_codec.h"

#include "vbh_comm/vbh_comm_macro_def.h"
#include "vbh_comm/vbh_comm_func.h"
#include "ord_cpas/order_channel_process_agent_service.h"


class CCpsAddr
{
public:
	CCpsAddr()
		: m_ipAddr("CPAS_IP_ADDR")
		, m_port("CPAS_PORT")
	{
	}

public:
	PER_BIND_ATTR(m_ipAddr, m_port);

public:
	CColumnWrapper< CDscString > m_ipAddr;
	CColumnWrapper< ACE_INT32 > m_port;
};


class CCpsAddrCriterion : public CSelectCriterion
{
public:
	CCpsAddrCriterion(const ACE_UINT32 nChannelID)
		: m_nChannelID(nChannelID)
	{
	}

public:
	virtual void SetCriterion(CPerSelect& rPerSelect) override
	{
		rPerSelect.Where(rPerSelect["CH_ID"] == m_nChannelID);
	}

private:
	const ACE_UINT32 m_nChannelID;
};
CChannelProcessAgentService::CChannelProcessAgentService(CDscString strIpAddr, ACE_INT32 nPort, ACE_UINT32 nChannelID, ACE_UINT16 nCpsId)
	:m_strIpAddr(strIpAddr)
	,m_nPort(nPort)
	,m_nChannelID(nChannelID)
	,m_nCpsId(nCpsId)
{
}

ACE_INT32 CChannelProcessAgentService::OnInit(void)
{
	//父类初始化
	if (CDscHtsClientService::OnInit())
	{
		DSC_RUN_LOG_ERROR("CDscHtsClientService service init failed!");

		return -1;
	}



	CDscDatabase database;
	CDBConnection dbConnection;
	dsc_vector_type(PROT_COMM::CDscIpAddr) vecCpsAddr; //cpas地址列表
	PROT_COMM::CDscIpAddr addr;

	if (CDscDatabaseFactoryDemon::instance()->CreateDatabase(database, dbConnection))
	{
		DSC_RUN_LOG_ERROR("connect database failed.");
		return -1;
	}
	else
	{
		CTableWrapper< CCollectWrapper<CCpsAddr> > lstCollectCpasAddr("CPS_CFG_LIST");
		CCpsAddrCriterion cpasAddrCriterion(m_nChannelID);

		if (::PerSelect(lstCollectCpasAddr, database, dbConnection, &cpasAddrCriterion))
		{
			DSC_RUN_LOG_ERROR("select from CPAS_CFG failed");

			return -1;
		}

		for (auto it = lstCollectCpasAddr->begin(); it != lstCollectCpasAddr->end(); ++it)
		{
			addr.SetIpAddr(*it->m_ipAddr);
			addr.SetPort(*it->m_port);

			vecCpsAddr.push_back(addr);
		}
	}
	ACE_INT32 nCpsNum = 1;
	for (auto& it : vecCpsAddr)
	{
		CCpsHandleInfo nHandleInfo;
		nHandleInfo.m_nHandleID = this->AllocHandleID();
		nHandleInfo.m_nHandleState = 0;
		nCpsNum++;
		m_vecCpsHandleInfo.push_back(nHandleInfo);
		this->DoConnect(it, NULL, nHandleInfo.m_nHandleID);
	}
	m_nKafkaValue = nCpsNum / 2 + 1;

	//在200到500之间	
	ACE_INT64 nTimeLong = rand() % 5;
	if (nTimeLong < 2)
	{
		nTimeLong = nTimeLong + 2;
	}
	m_raftVoteInfo.m_nTimeLong = nTimeLong;
	m_raftVoteInfo.m_nTerm = 0;

	m_voteState = EN_VOTE_FOLLOWER_STATE;

	if (1 == m_nKafkaValue)
	{
		m_pCps->ChangeCpsToMasterState();
	}

	return 0;
}

ACE_INT32 CChannelProcessAgentService::OnExit(void)
{


	return CDscHtsClientService::OnExit();
}


ACE_INT32 CChannelProcessAgentService::OnConnectedNodify(CMcpClientHandler* pMcpClientHandler)
{
	ACE_UINT32 nHandleID = pMcpClientHandler->GetHandleID();

	for (auto& it : m_vecCpsHandleInfo)
	{
		if (nHandleID == it.m_nHandleID)
		{
			it.m_nHandleState = 1;//把网络状态设置为好
		}
	}
	if (!m_startTimerFlag)
	{
		//第一次启动的时间选择长一些，为了给已经起来的节点机会，刚启动的时候选择时间会比较长
		this->SetDscTimer(this, m_raftVoteInfo.m_nTimeLong);
		m_startTimerFlag = 1;
	}

	return CDscHtsClientService::OnConnectedNodify(pMcpClientHandler);
}


void CChannelProcessAgentService::OnTimer(void)
{
	if (EN_VOTE_TIMER == m_eTimerType)
	{
		RaftVoteTimeOut();
	}
	else
	{
		RaftHeartBeatTimeOut();
	}

}


void CChannelProcessAgentService::OnNetworkError(CMcpHandler* pMcpHandler)
{

	ACE_UINT32 nHandleID = pMcpHandler->GetHandleID();

	for (auto& it : m_vecCpsHandleInfo)
	{
		if (nHandleID == it.m_nHandleID)
		{
			it.m_nHandleState = 0;//把网络状态设置为好
		}
	}
	return CDscHtsClientService::OnNetworkError(pMcpHandler);
}


void CChannelProcessAgentService::SendHeartBeatToAllServers()
{
	VBH::CRaftHeartBeatCpsCpas req;
	req.m_nLeaderId = m_nCpsId;
	req.m_nTerm = m_raftVoteInfo.m_nTerm;

	for (auto& it : m_vecCpsHandleInfo)
	{
		if (!it.m_nHandleState)
		{
			continue;
		}

		if (this->SendHtsMsg(req, it.m_nHandleID))
		{
			DSC_RUN_LOG_ERROR("send hts message:CRaftHeartBeatCpsCpas failed, channel-id:%d", m_nChannelID);
		}
	}
}

void CChannelProcessAgentService::RaftHeartBeatTimeOut()
{
	SendHeartBeatToAllServers();
	this->SetDscTimer(this, 1);
}

void CChannelProcessAgentService::RaftVoteTimeOut()
{
	/*1.Increment current term
	  2.Change to Candiate state
	  3.Vote for self
	  4.Send RequestVote to all other server
	*/
	DSC_RUN_LOG_INFO("RaftVoteTimeOut, m_nTerm:%d", m_raftVoteInfo.m_nTerm);
	VBH::CRaftVoteCpsCpasReq req;
	m_raftVoteInfo.m_nTerm++;
	m_voteState = EN_VOTE_CANDIDATE_STATE;

	m_raftVoteInfo.m_nVoteNum = 1;
	req.m_nLeaderId = m_nCpsId;
	req.m_nTerm = m_raftVoteInfo.m_nTerm;


	for (auto& it : m_vecCpsHandleInfo)
	{
		if (!it.m_nHandleState)
		{
			continue;
		}

		if (this->SendHtsMsg(req, it.m_nHandleID))
		{
			DSC_RUN_LOG_ERROR("send hts message:CRaftVoteCpsCpasReq failed, channel-id:%d", m_nChannelID);
		}
	}

	this->SetDscTimer(this, m_raftVoteInfo.m_nTimeLong);

}

void CChannelProcessAgentService::ReceiveRaftHeartBeat(VBH::CRaftHeartBeatCpsCpas& rRaftHeartBeatCpsCpas)
{
	/*重置定时器*/
	/*如果正在竞选要停止竞选*/
	/*收到任期比自己小的心跳，直接返回*/
	if (rRaftHeartBeatCpsCpas.m_nTerm < m_raftVoteInfo.m_nTerm)
	{
		return;
	}


	if (EN_VOTE_FOLLOWER_STATE == m_voteState)
	{
		/*重置定时器*/
		this->ResetDscTimer(this, m_raftVoteInfo.m_nTimeLong);
	}
	else if (EN_VOTE_CANDIDATE_STATE == m_voteState)
	{
		/*重置定时器*/
		this->ResetDscTimer(this, m_raftVoteInfo.m_nTimeLong);
		m_voteState = EN_VOTE_FOLLOWER_STATE;
		m_raftVoteInfo.m_nTerm = rRaftHeartBeatCpsCpas.m_nTerm;
		m_raftVoteInfo.m_nLeaderId = rRaftHeartBeatCpsCpas.m_nLeaderId;
		m_raftVoteInfo.m_nVoteNum = 0;
	}
	else if ((EN_VOTE_LEADER_STATE == m_voteState) && (rRaftHeartBeatCpsCpas.m_nTerm > m_raftVoteInfo.m_nTerm))
	{

		m_voteState = EN_VOTE_FOLLOWER_STATE;
		/*CPS切换到followe状态*/

		this->CancelDscTimer(this);
		this->ResetDscTimer(this, m_raftVoteInfo.m_nTimeLong);
		m_eTimerType = EN_VOTE_TIMER;
		m_pCps->ChangeCpsToFollowerState();
	}

}


ACE_INT32 CChannelProcessAgentService::GetRaftVoteRsp(VBH::CRaftVoteCpsCpasReq& rRaftVoteCpsCpasReq, ACE_UINT16& nResult)
{
	if (rRaftVoteCpsCpasReq.m_nTerm > m_raftVoteInfo.m_nTerm)
	{
		nResult = 1;
		m_raftVoteInfo.m_nTerm = rRaftVoteCpsCpasReq.m_nTerm;
	}
	else
	{
		nResult = 0;
	}
	return 0;
}

ACE_INT32 CChannelProcessAgentService::OnHtsMsg(VBH::CRaftVoteCpsCpasRsp& rRaftVoteCpsCpasRsp, CMcpHandler* pMcpHandler)
{
	VBH_MESSAGE_ENTER_TRACE(CChannelProcessAgentService, rRaftVoteCpsCpasRsp);
	/*1.Receive votes from majority of servers
	Become leader
	Send heartbeat to other servers
	start heartbeat timers
    2.not receive votes from majority of servers
	Increment VoteNum
    */
    /*无效票*/
	if (0 == rRaftVoteCpsCpasRsp.m_nResult)
	{
		return 0;
	}
	/*如果已经成为领导或者切换为跟随者，投票都没有用*/
	if (EN_VOTE_CANDIDATE_STATE != m_voteState)
	{
		return 0;
	}

	m_raftVoteInfo.m_nVoteNum++;
	if (m_raftVoteInfo.m_nVoteNum >= m_nKafkaValue)
	{
		m_voteState = EN_VOTE_LEADER_STATE;

		SendHeartBeatToAllServers();
		this->CancelDscTimer(this);
		this->ResetDscTimer(this, 1);
		m_eTimerType = EN_HEART_BEAT_TIMER;
		DSC_RUN_LOG_INFO("change Cps To Master State");
		m_pCps->ChangeCpsToMasterState();
		ACE_OS::printf("change Cps To Master State");
	}

	VBH_MESSAGE_LEAVE_TRACE(CChannelProcessAgentService, rRaftVoteCpsCpasRsp);

	return 0;
}
