#ifndef COMM_MSG_CPS_CPAS_DEF_H_456465465461245455678577867968878
#define COMM_MSG_CPS_CPAS_DEF_H_456465465461245455678577867968878
#include "dsc/codec/dsc_codec/dsc_codec.h"

#include "vbh_comm/vbh_comm_id_def.h"
#include "vbh_comm/vbh_comm_def_export.h"


namespace VBH
{

	//leader 发给follower的心跳消息
	class CRaftHeartBeatCpsCpas
	{
	public:
		enum
		{
			EN_MSG_ID = VBH::EN_RAFT_HEART_BEAT_CPS_CPAS
		};

	public:
		DSC_BIND_ATTR(m_nTerm, m_nLeaderId);

	public:
		ACE_UINT64 m_nTerm; //任期
		ACE_UINT16 m_nLeaderId; //领导ID
	};

	//竞选者发起选举请求
	class CRaftVoteCpsCpasReq
	{
	public:
		enum
		{
			EN_MSG_ID = VBH::EN_RAFT_VOTE_CPS_CPAS_REQ
		};

	public:
		DSC_BIND_ATTR(m_nTerm, m_nLeaderId);

	public:
		ACE_UINT64 m_nTerm; //任期
		ACE_UINT16 m_nLeaderId;
	};


	//竞选者发起选举响应
	class CRaftVoteCpsCpasRsp
	{
	public:
		enum
		{
			EN_MSG_ID = VBH::EN_RAFT_VOTE_CPS_CPAS_RSP
		};

	public:
		DSC_BIND_ATTR(m_nTerm, m_nResult);

	public:
		ACE_UINT64 m_nTerm; //任期
		ACE_UINT16 m_nResult;
	};

	class CRaftTimerTimeOut
	{
	public:
		enum
		{
			EN_MSG_ID = VBH::EN_RAFT_TIMER_TIME_OUT
		};

	public:
		DSC_BIND_ATTR(m_nTimerID);

	public:
		ACE_UINT64 m_nTimerID;//定时器ID
	};

	class CRaftCpasCpsConnectSucess
	{
	public:
		enum
		{
			EN_MSG_ID = VBH::EN_RAFT_CPAS_CPS_CONNECT_SUCESS,

		};

	public:
		DSC_BIND_ATTR(m_nCpasID);

	public:
		ACE_UINT64 m_nCpasID;//定时器ID
	};

}
#endif