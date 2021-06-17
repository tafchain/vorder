#include "ace/OS_NS_fcntl.h"
#include "ace/OS_NS_unistd.h"
#include "ace/OS_NS_sys_stat.h"
#include "ace/OS_NS_sys_socket.h"

#include "vbh_comm/vbh_comm_func.h"
#include "vbh_comm/vbh_comm_macro_def.h"
#include "vbh_comm/vbh_comm_error_code.h"
#include "vbh_comm/vbh_comm_wrap_msg_def.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_cls_config_param.h"

#include "ord_cps/order_channel_process_service.h"




CChannelProcessService::CCsConnectSessionHandler::CCsConnectSessionHandler(CChannelProcessService& rService, ACE_HANDLE handle, const ACE_INT32 nHandleID)
	: CMcpServerHandler(rService, handle, nHandleID)
	, m_rCps(rService)
{
}

CChannelProcessService::CWaitPeerRegistSession::CWaitPeerRegistSession(CChannelProcessService& rCps)
	: m_rCps(rCps)
{
}

void CChannelProcessService::CWaitPeerRegistSession::OnTimer(void)
{
	//TODO: 告警
}

CChannelProcessService::COrderSyncSession::COrderSyncSession(CChannelProcessService& rCps)
	: m_rCps(rCps)
{
}

CChannelProcessService::CFollowerOrderSyncSession::CFollowerOrderSyncSession(CChannelProcessService& rCps)
	: m_rCps(rCps)
{
}

void CChannelProcessService::COrderSyncSession::OnTimer(void)
{
	m_rCps.OnSyncTimeOut(this);
}

void CChannelProcessService::CFollowerOrderSyncSession::OnTimer(void)
{
	m_rCps.OnSyncTimeOut(this);
}


CChannelProcessService::CPackTimerHandler::CPackTimerHandler(CChannelProcessService& rCps)
	: m_rCps(rCps)
{
}

CChannelProcessService::CRepersistBlockQueueHeaderSession::CRepersistBlockQueueHeaderSession(CChannelProcessService& rCps)
	: m_rCps(rCps)
{
}

CChannelProcessService::CLocalBlockInfo::CLocalBlockInfo(const ACE_UINT32 nBlockBufSize)
	: m_nBlockBufSize(nBlockBufSize)
{
	m_pBlockDataBuf = DSC_THREAD_SIZE_MALLOC(m_nBlockBufSize);
	m_nBlockDataLen = VBH::CBcBlockHeader::EN_SIZE; //区块长度中至少包含1个区块头的长度
}

CChannelProcessService::CLocalBlockInfo::~CLocalBlockInfo()
{
	DSC_THREAD_SIZE_FREE(m_pBlockDataBuf, m_nBlockBufSize);
}

void CChannelProcessService::CLocalBlockInfo::Resize(ACE_UINT32 nNewBufSize)
{
	ACE_ASSERT(nNewBufSize > m_nBlockBufSize);

	char* pBuf = DSC_THREAD_SIZE_MALLOC(nNewBufSize);

	memcpy(pBuf, m_pBlockDataBuf, m_nBlockDataLen);

	DSC_THREAD_SIZE_FREE(m_pBlockDataBuf, m_nBlockBufSize);
	m_pBlockDataBuf = pBuf;
	m_nBlockBufSize = nNewBufSize;
}

CChannelProcessService::CChannelProcessService(const CDscString& strIpAddr, const ACE_INT32 nPort, const ACE_UINT32 nChannelID)
	: CBaseChannelProcessService(strIpAddr, nPort, nChannelID)
	, m_repersistBlockQueueHeaderSession(*this)
	, m_waitPeerRegistSession(*this)
	, m_orderSyncSession(*this)
	, m_packTimerHandler(*this)
	, m_followerOrderSyncSession(*this)
{
}

ACE_INT32 CChannelProcessService::OnInit(void)
{
	//父类初始化
	if (CBaseChannelProcessService::OnInit())
	{
		DSC_RUN_LOG_ERROR("CChannelProcessService init failed!");

		return -1;
	}

	//读取配置参数
	if (GetVbhMasterOrderProfile())
	{
		DSC_RUN_LOG_ERROR("GetVbhMasterOrderProfile failed!");
		return -1;
	}

	//如果退出前日志仍旧在使用中，则从日志中恢复系统状态
	if (m_pConfig->m_nModifyStorageState == EN_BEGIN_MODIFY_STORAGE)
	{
		if (m_wsVersionTable.RedoByLog())
		{
			DSC_RUN_LOG_ERROR("write-set-version-table redo by log failed.");
			return -1;
		}

		m_pConfig->m_nBlockID = m_pCfgLog->m_nBlockID;
		m_pConfig->m_nLastAlocWsID = m_pCfgLog->m_nLastAlocWsID;
		::memcpy(m_pConfig->m_blockHash, m_pCfgLog->m_blockHash, VBH_BLOCK_DIGEST_LENGTH);

		m_pConfig->m_nModifyStorageState = EN_BEGIN_MODIFY_STORAGE;
	}

	this->SetDscTimer(&m_waitPeerRegistSession, EN_WAIT_PEER_REGIST_TIMEOUT_VALUE, true);
	m_nSeed = ACE_OS::time(0);
	ACE_OS::srand(m_nSeed);

	m_pConfig->m_nSequenceNumber = m_nChannelID; //保证多个channel的序列号绝不一致；

	m_pCurBlockInfo = DSC_THREAD_TYPE_NEW(CLocalBlockInfo) CLocalBlockInfo(m_nPackMaxBlockSizeValue);
	m_pCurBlockInfo->m_nBlockID = m_pConfig->m_nBlockID + 1;
	m_pCurBlockInfo->m_nLastAlocWsID = m_pConfig->m_nLastAlocWsID;
	m_blockEncoder.InitSetEncodeBuffer(m_pCurBlockInfo->m_pBlockDataBuf);

	DSC_RUN_LOG_INFO("master order channel process service:%d init succeed!", this->GetID());

	return 0;
}

ACE_INT32 CChannelProcessService::OnExit(void)
{
	this->CancelDscTimer(&m_repersistBlockQueueHeaderSession);
	this->CancelDscTimer(&m_waitPeerRegistSession);
	this->CancelDscTimer(&m_orderSyncSession);
	this->CancelDscTimer(&m_packTimerHandler);

	//释放区块缓存
	if (m_pCurBlockInfo)
	{
		DSC_THREAD_TYPE_DELETE(m_pCurBlockInfo);
	}

	m_pCurBlockInfo = m_queueBlockInfo.PopFront();
	while(m_pCurBlockInfo)
	{
		DSC_THREAD_TYPE_DELETE(m_pCurBlockInfo);
		m_pCurBlockInfo = m_queueBlockInfo.PopFront();
	}

	return CBaseChannelProcessService::OnExit();
}

ACE_INT32 CChannelProcessService::OnHtsMsg(VBH::CRaftHeartBeatCpsCpas& rRaftHeartBeatCpsCpas, CMcpHandler* pMcpHandler)
{
	VBH_MESSAGE_ENTER_TRACE(CChannelProcessService, rRaftHeartBeatCpsCpas);

	m_pOrderChannelprocessAgentService->ReceiveRaftHeartBeat(rRaftHeartBeatCpsCpas);
	if (!m_nIsStartFollowerSysTimer)
	{
		this->SetDscTimer(&m_followerOrderSyncSession, EN_FOLLOWER_ORDER_SYNC_TIMEOUT_VALUE, true);
		m_nIsStartFollowerSysTimer = true;
	}

	VBH_MESSAGE_LEAVE_TRACE(CChannelProcessService, rRaftHeartBeatCpsCpas);
	return 0;
}

ACE_INT32 CChannelProcessService::OnHtsMsg(VBH::CRaftVoteCpsCpasReq& rRaftVoteCpsCpasReq, CMcpHandler* pMcpHandler)
{
	VBH_MESSAGE_ENTER_TRACE(CChannelProcessService, rRaftVoteCpsCpasReq);
	VBH::CRaftVoteCpsCpasRsp rsp;
	rsp.m_nTerm = rRaftVoteCpsCpasReq.m_nTerm;
	rsp.m_nTerm = 0;//先设置成不通过

	m_pOrderChannelprocessAgentService->GetRaftVoteRsp(rRaftVoteCpsCpasReq, rsp.m_nResult);

	if (this->SendHtsMsg(rsp, pMcpHandler))
	{
		DSC_RUN_LOG_ERROR("send hts message failed, channel-id:%d.", m_nChannelID);
	}

	VBH_MESSAGE_LEAVE_TRACE(CChannelProcessService, rRaftVoteCpsCpasReq);

	return 0;
}

ACE_INT32 CChannelProcessService::OnHtsMsg(VBH::CRaftCpasCpsConnectSucess& rRaftCpasCpsConnectSucess, CMcpHandler* pMcpHandler)
{

	DSC_RUN_LOG_INFO("cpas cps connect sucess");

	return 0;
}

ACE_INT32 CChannelProcessService::OnHtsMsg(VBH::CQueryLeaderCpsTasCpsReq& rQueryLeaderCpsTasCpsReq, CMcpHandler* pMcpHandler)
{
	 
	if (EN_FOLLOWER_STATE != m_nCpsState)
	{
		VBH::CQueryLeaderCpsCpsTasRsp rsp;
		if (this->SendHtsMsg(rsp, pMcpHandler))
		{
			DSC_RUN_LOG_ERROR("send CQueryLeaderCpsCpsTasRsp failed.");
		}
		DSC_RUN_LOG_INFO("send CQueryLeaderCpsCpsTasRsp sucess");
	}


	return 0;
}


void CChannelProcessService::OnSyncTimeOut(COrderSyncSession* pSession)
{
	CCsConnectSessionHandler* pHandler = GetRegistPeerHandler(pSession->m_nKafkaBlockID, pSession->m_strKafkaBlockHash);

	if (pHandler)
	{
		VBH::CMasterSyncVersionTableCpsCsReq req;

		req.m_nBlockID = m_pConfig->m_nBlockID + 1;

		if (this->SendHtsMsg(req, pHandler))
		{
			DSC_RUN_LOG_ERROR("send CMasterSyncVersionTableCpsXcsReq failed.");
		}
	}
}

void CChannelProcessService::OnSyncTimeOut(CFollowerOrderSyncSession* pSession)
{
	
	if (m_nSelectCursorIdx >= m_arrCsConnectHandler.Size())
	{
		m_nSelectCursorIdx = 0;
	}

	VBH::CSlaveSyncVersionTableCpsCsReq req;

	req.m_nBlockID = m_pConfig->m_nBlockID + 1;

	if (this->SendHtsMsg(req, m_arrCsConnectHandler[m_nSelectCursorIdx]))
	{
		DSC_RUN_LOG_ERROR("send CSlaveSyncVersionTableCpsXcsReq failed.");
	}

	++m_nSelectCursorIdx;
}

void CChannelProcessService::OnTimePackBlock()
{
	//事务个数不为0时，才执行打包；
	if (m_blockEncoder.m_bcBlockHeader.m_nTransCount)
	{
		//1. 填充区块头
		m_blockEncoder.m_bcBlockHeader.m_nOrderID = m_nOrderID;
		m_blockEncoder.m_bcBlockHeader.m_nBlockID = m_pCurBlockInfo->m_nBlockID;
		m_blockEncoder.m_bcBlockHeader.m_nBlockTime = this->GetCurTime();
		//打包当前区块头时，需要取上1块的hash值
		//如果待发送队列不空，则取队尾的hash; 否则取config中的hsh
		if (m_nQueueBlockNum)
		{
			::memcpy(m_blockEncoder.m_bcBlockHeader.m_preBlockHash.data(), m_queueBlockInfo.Back()->m_blockHash, VBH_BLOCK_DIGEST_LENGTH);
		}
		else
		{
			::memcpy(m_blockEncoder.m_bcBlockHeader.m_preBlockHash.data(), m_pConfig->m_blockHash, VBH_BLOCK_DIGEST_LENGTH);
		}

		m_blockEncoder.EncodeBlockHeader();
		VBH::vbhDigest(m_pCurBlockInfo->m_pBlockDataBuf, m_pCurBlockInfo->m_nBlockDataLen, m_pCurBlockInfo->m_blockHash); //计算当前打包块的hash

		//2 打包version-table中的修改
		m_wsVersionTable.PackModify();

		//3. 将区块信息放入打包队列, 并开辟新的区块
		CLocalBlockInfo* pBlockInfo = DSC_THREAD_TYPE_NEW(CLocalBlockInfo) CLocalBlockInfo(m_nBlockBufInitialSize);
		pBlockInfo->m_nBlockID = m_pCurBlockInfo->m_nBlockID + 1;
		pBlockInfo->m_nLastAlocWsID = m_pCurBlockInfo->m_nLastAlocWsID;
		m_blockEncoder.InitSetEncodeBuffer(pBlockInfo->m_pBlockDataBuf);

		m_queueBlockInfo.PushBack(m_pCurBlockInfo);
		++m_nQueueBlockNum;
		VBH_TRACE_MESSAGE("pack block, channel-id:%d, block-id:%lld", m_nChannelID, m_pCurBlockInfo->m_nBlockID);

		m_pCurBlockInfo = pBlockInfo;

		//缓存中item个数为1, 说明之前缓存为空, 需要启动发送 //一旦启动后，发送逻辑会自驱动，即发送完一个再发一个直到发送完毕
		if (m_nQueueBlockNum == 1)
		{
			DistributeBlock();
		}
	}
}

void CChannelProcessService::DistributeBlock()
{
	if (m_arrCsConnectHandler.Size() && m_nQueueBlockNum) //有XCS连接存在且有待发区块时，就发送
	{
		//1. 找到队头的待发送区块
		CLocalBlockInfo* pBlockInfo = m_queueBlockInfo.Front();

		m_nDistBlockRspPeerCount = 0;

		//2. 遍历所有的peer，发送数据，并为每个peer开启重发定时器
		VBH::CDistributeBlockCpsCsReq req;

		req.m_nBlockID = pBlockInfo->m_nBlockID;
		req.m_vbhBlock.Set(pBlockInfo->m_pBlockDataBuf, pBlockInfo->m_nBlockDataLen);

		for (ACE_UINT32 idx = 0; idx < m_arrCsConnectHandler.Size(); ++idx)
		{
			if (this->SendHtsMsg(req, m_arrCsConnectHandler[idx]))
			{
				DSC_RUN_LOG_ERROR("send CDistributeBlockCpsXcsReq failed.");
				--idx; //出错处理

			}
			else
			{
				m_arrCsConnectHandler[idx]->m_bRecvDistBlockRsp = false; //清空收到应答标志
				this->SetDscTimer(m_arrCsConnectHandler[idx], EN_DISTRIBUTE_TIMEOUT_VALUE);

				VBH_TRACE_MESSAGE("send block:%llu to peer:%d", req.m_nBlockID, m_arrCsConnectHandler[idx]->m_nPeerID);
			}
		}
	}
}

void CChannelProcessService::OnTimeDistributeBlock(CCsConnectSessionHandler* pHandler)
{
	//1. 找到队头的cache
	CLocalBlockInfo* pBlockInfo = m_queueBlockInfo.Front();
	VBH::CDistributeBlockCpsCsReq req;

	req.m_nBlockID = pBlockInfo->m_nBlockID;
	req.m_vbhBlock.Set(pBlockInfo->m_pBlockDataBuf, pBlockInfo->m_nBlockDataLen);

	this->SendHtsMsg(req, pHandler);
	DSC_RUN_LOG_WARNING("timeout, on-time retrans block: %llu, cached-block-count:%d", req.m_nBlockID, m_nQueueBlockNum);
}

void CChannelProcessService::OnTimeRepersistence(CRepersistBlockQueueHeaderSession* pSession)
{
	//3对区块头进行持久化, 成功则尝试发送下一个区块
	if (!PersistentBlockQueueHeader())
	{
		this->CancelDscTimer(&m_repersistBlockQueueHeaderSession);

		if (m_nQueueBlockNum)//4. 如果队列不空，继续发送下1个区块
		{
			DistributeBlock();
		}
	}
}

bool CChannelProcessService::CPeerInfo::operator< (const CPeerInfo& rPeerInfo)
{
	if (m_nRegistMaxBlockID == rPeerInfo.m_nRegistMaxBlockID)
	{
		return m_strRegistMaxBlockHash < rPeerInfo.m_strRegistMaxBlockHash;
	}
	else
	{
		return m_nRegistMaxBlockID > rPeerInfo.m_nRegistMaxBlockID; //为了确保降序排列，特地大小于号颠倒一下。
	}
}

void CChannelProcessService::SendQueryMaxBlockInfoMsg(const ACE_UINT32 nHandleID)
{
	VBH::CQueryMaxBlockInfoCpsCsReq req;
	req.m_nOrderID = m_nOrderID;
	if (this->SendHtsMsg(req, nHandleID))
	{
		DSC_RUN_LOG_ERROR("send CQueryMaxBlockInfoCpsCsReq failed.");
	}
	DSC_RUN_LOG_INFO("send CQueryMaxBlockInfoCpsCsReq Sucess.");
}

void CChannelProcessService::SendSyncBlockNotifyMsg(const ACE_UINT32 nNormalFront, const ACE_UINT32 nNormalRear)
{
	VBH::CSyncBlockOnRegistCpsCsNotify syncBlockNotify;
	ACE_UINT32 nNormalCursor = nNormalFront;

	for (ACE_UINT32 i = 0; i < m_vecPeerInfo.size(); ++i)
	{
		if (i < nNormalFront || i > nNormalRear)
		{
			if (m_vecPeerInfo[i].m_nRegistMaxBlockID == m_vecPeerInfo[nNormalFront].m_nRegistMaxBlockID)
			{
				syncBlockNotify.m_nMaxBlockHashIsValid = VBH::CSyncBlockOnRegistCpsCsNotify::EN_HASH_INVALID;
			}
			else
			{
				syncBlockNotify.m_nMaxBlockHashIsValid = VBH::CSyncBlockOnRegistCpsCsNotify::EN_HASH_NOT_VERIFY;
			}

			syncBlockNotify.m_nRegisterBlockID = m_vecPeerInfo[i].m_nRegistMaxBlockID;
			syncBlockNotify.m_nSyncSrcPeerID = m_vecPeerInfo[nNormalCursor].m_nPeerID;
			syncBlockNotify.m_nKafkaBlockID = m_vecPeerInfo[nNormalCursor].m_nRegistMaxBlockID;
			syncBlockNotify.m_nSyncSrcPort = m_vecPeerInfo[nNormalCursor].m_nCasPort;
			syncBlockNotify.m_strSyncSrcIpAddr = m_vecPeerInfo[nNormalCursor].m_strCasIpAddr;

			if (this->SendHtsMsg(syncBlockNotify, m_vecPeerInfo[i].m_nHandleID))
			{
				DSC_RUN_LOG_ERROR("send CSyncBlockOnRegistCpsCsNotify failed.");
			}

			++nNormalCursor;
			if (nNormalCursor > nNormalRear)
			{
				nNormalCursor = nNormalFront;
			}
		}
	}
}


void CChannelProcessService::ChangeCpsToMasterState(void)
{

	if (m_arrCsConnectHandler.Size() >= m_nKafkaValue)
	{
		//连接的数量足够，就给所有连接的peer发送查询消息，状态切换到开始状态
		for (ACE_UINT32 i = 0; i < m_arrCsConnectHandler.Size(); ++i)
		{
			SendQueryMaxBlockInfoMsg(m_arrCsConnectHandler[i]->GetHandleID());
		}
		m_nCpsState = EN_MASTER_START_STATE;
	}
	else
	{
		//如果连接的数量太少，就切换到等待状态，继续等待注册的到来
	    //在注册的处理流程中触发后面的查询流程，如果是注册一直不够就只能等待了
		m_nCpsState = EN_MASTER_WAIT_STATE;
	}
	this->CancelDscTimer(&m_followerOrderSyncSession);
	m_nIsStartFollowerSysTimer = false;
	DSC_RUN_LOG_INFO("change Cps State:%d", m_nCpsState);

}

void CChannelProcessService::ChangeCpsToFollowerState(void)
{
	//释放缓存
	this->CancelDscTimer(&m_repersistBlockQueueHeaderSession);
	this->CancelDscTimer(&m_waitPeerRegistSession);
	this->CancelDscTimer(&m_orderSyncSession);
	this->CancelDscTimer(&m_packTimerHandler);

	//释放区块缓存
	if (m_pCurBlockInfo)
	{
		DSC_THREAD_TYPE_DELETE(m_pCurBlockInfo);
	}

	m_pCurBlockInfo = m_queueBlockInfo.PopFront();
	while (m_pCurBlockInfo)
	{
		DSC_THREAD_TYPE_DELETE(m_pCurBlockInfo);
		m_pCurBlockInfo = m_queueBlockInfo.PopFront();
	}

	m_nCpsState == EN_FOLLOWER_STATE;


	DSC_RUN_LOG_ERROR("change Cps To Follower State");
	ACE_OS::printf("change Cps To Follower State");

	//ToDO:要启动follower定时同步定时器

}

void CChannelProcessService::GetTheMostBlockHeightInfo(ACE_UINT32& nNormalFront, ACE_UINT32& nNormalRear, ACE_UINT32& nNormalPeerCount)
{
	//2.扫描m_vecXasConnectHandler，筛出“正常peer集”、“非正常peer集”，“正常peer”定义:最高块ID一致，hash一致，且集合peer个数大于等于kafka设定
	//___________________________________
	//| 10 | 10 | 10 | 10 | 10 | 10 | 9 | ... |
	//------------------------------------
	//| A | A | B | B | B | C  | C  | X |
	//|------------------------------------
	//一下算法筛出hash是'B''B''B'的peer

	//2.vector排序,做二级排序，一级：ID；第二级：hash
	std::sort(m_vecPeerInfo.begin(), m_vecPeerInfo.end());

	//3.遍历查找拥有最大BlockID中Block Hash相同的最大子集，约束：m_nKafkaValue > 1
	ACE_UINT32 nFront = 0;
	ACE_UINT32 nRear = 0;
	ACE_UINT32 nCount = 1; //遍历过程中记录1段区间
	CDscString strMaxBlockHash = m_vecPeerInfo[0].m_strRegistMaxBlockHash;

	for (ACE_UINT32 i = 1; i < m_vecPeerInfo.size() && m_vecPeerInfo[i].m_nRegistMaxBlockID == m_vecPeerInfo[0].m_nRegistMaxBlockID; ++i)
	{
		if (strMaxBlockHash == m_vecPeerInfo[i].m_strRegistMaxBlockHash)
		{
			++nRear;
			++nCount;
		}
		else
		{
			if (nCount > nNormalPeerCount)
			{
				nNormalFront = nFront;
				nNormalRear = nRear;
				nNormalPeerCount = nCount;
			}

			nFront = i;
			nRear = i;
			nCount = 1;
			strMaxBlockHash = m_vecPeerInfo[i].m_strRegistMaxBlockHash;
		}
	}

	if (nCount > nNormalPeerCount)
	{
		nNormalFront = nFront;
		nNormalRear = nRear;
		nNormalPeerCount = nCount;
	}

}


ACE_INT32 CChannelProcessService::OnHtsMsg(VBH::CRegistCsCpsReq& rRegistReq, CMcpHandler* pMcpHandler)
{
	VBH_MESSAGE_ENTER_TRACE(CMasterChannelProcessService, VBH::CRegistCsCpsReq);

	CCsConnectSessionHandler* pAnchorConnectHandler = dynamic_cast<CCsConnectSessionHandler*> (pMcpHandler);

	//TODO: 确认此处的SO_SNDBUF设置是否能够生效
	ACE_OS::setsockopt(pMcpHandler->get_handle(), SOL_SOCKET, SO_SNDBUF, (char*)(&m_nPackMaxBlockSizeValue), sizeof(int));

	//1.拣重
	if (pAnchorConnectHandler->m_nIndex == CDscTypeArray<CCsConnectSessionHandler>::EN_INVALID_INDEX_ID) //防止重复插入
	{
		//查重：同一个peer-id，不同的连接句柄
		for (ACE_UINT32 i = 0; i < m_arrCsConnectHandler.Size(); ++i)
		{
			if (rRegistReq.m_nPeerID == m_arrCsConnectHandler[i]->m_nPeerID)
			{
				DSC_RUN_LOG_WARNING("repeat regist req, peer id:%d.", rRegistReq.m_nPeerID);

				return -1;
			}
		}

		m_arrCsConnectHandler.Insert(pAnchorConnectHandler);
	}
	else
	{
		DSC_RUN_LOG_INFO("repeat regist req, peer id:%d.", pAnchorConnectHandler->m_nPeerID);
	}

	//2.更新信息，重复消息场景下以后来消息为准
	pAnchorConnectHandler->m_nPeerID = rRegistReq.m_nPeerID;
	pAnchorConnectHandler->m_nCasPort = rRegistReq.m_nCasPort;
	pAnchorConnectHandler->m_strCasIpAddr = rRegistReq.m_strCasIpAddr;
	pAnchorConnectHandler->m_nRegistMaxBlockID = rRegistReq.m_nMaxBlockID;
	pAnchorConnectHandler->m_strRegistMaxBlockHash = rRegistReq.m_strMaxBlockHash;


	DSC_RUN_LOG_INFO("receive peer:%d regist message, order state:%d, receive block ID: %d", rRegistReq.m_nPeerID, m_nCpsState, rRegistReq.m_nMaxBlockID);

	if (m_nCpsState == EN_FOLLOWER_STATE)
	{
		return 0;
	}


	if (m_nCpsState == EN_MASTER_WAIT_STATE)
	{
		if (m_arrCsConnectHandler.Size() >= m_nKafkaValue)
		{
			for (ACE_UINT32 i = 0; i < m_arrCsConnectHandler.Size(); ++i)
			{
				SendQueryMaxBlockInfoMsg(m_arrCsConnectHandler[i]->GetHandleID());
			}
			m_nCpsState = EN_MASTER_START_STATE;
		}
	}
	else if (m_nCpsState == EN_MASTER_START_STATE)
	{
		SendQueryMaxBlockInfoMsg(pMcpHandler->GetHandleID());
	}
	else if (m_nCpsState == EN_MASTER_SYNC_STATE) //在order同步状态
	{
		if (rRegistReq.m_nMaxBlockID < m_orderSyncSession.m_nKafkaBlockID) //新注册peer高度未达到最高区块高度,任选1个节点令其进行同步
		{
			CCsConnectSessionHandler* pHandler = GetRegistPeerHandler(m_orderSyncSession.m_nKafkaBlockID, m_orderSyncSession.m_strKafkaBlockHash);

			if (pHandler)
			{
				VBH::CSyncBlockOnRegistCpsCsNotify notify;

				notify.m_nMaxBlockHashIsValid = VBH::CSyncBlockOnRegistCpsCsNotify::EN_HASH_NOT_VERIFY;
				notify.m_nSyncSrcPeerID = pHandler->m_nPeerID;
				notify.m_nSyncSrcPort = pHandler->m_nCasPort;
				notify.m_nRegisterBlockID = rRegistReq.m_nMaxBlockID;
				notify.m_nKafkaBlockID = m_orderSyncSession.m_nKafkaBlockID;
				notify.m_strSyncSrcIpAddr = pHandler->m_strCasIpAddr;

				if (this->SendHtsMsg(notify, pMcpHandler))
				{
					DSC_RUN_LOG_ERROR("send CSyncBlockOnRegistCpsXcsNotify failed.");
				}
			}
		}
		else if (rRegistReq.m_nMaxBlockID > m_orderSyncSession.m_nKafkaBlockID) //令其删块后，再来注册
		{
			VBH::CBackspaceBlockCpsCsNotify notify;

			notify.m_nKafkaBlockID = m_orderSyncSession.m_nKafkaBlockID;
			notify.m_nRegisterBlockID = rRegistReq.m_nMaxBlockID;

			if (this->SendHtsMsg(notify, pMcpHandler))
			{
				DSC_RUN_LOG_ERROR("send CBackspaceBlockCpsXcsNotify failed.");
			}
			//此处不必从列表中移除
		}
		else //rRegisterAs.m_nMaxBlockID == nMaxBlockID
		{
			if (rRegistReq.m_strMaxBlockHash != m_orderSyncSession.m_strKafkaBlockHash) //区块高度一样，但hash值不一样，需要退块后同步，再来注册
			{
				CCsConnectSessionHandler* pBestHandler = GetRegistPeerHandler(m_orderSyncSession.m_nKafkaBlockID, m_orderSyncSession.m_strKafkaBlockHash);
				VBH::CSyncBlockOnRegistCpsCsNotify notify;

				notify.m_nMaxBlockHashIsValid = VBH::CSyncBlockOnRegistCpsCsNotify::EN_HASH_INVALID;
				notify.m_nSyncSrcPeerID = pBestHandler->m_nPeerID;
				notify.m_nSyncSrcPort = pBestHandler->m_nCasPort;
				notify.m_nRegisterBlockID = rRegistReq.m_nMaxBlockID;
				notify.m_nKafkaBlockID = m_orderSyncSession.m_nKafkaBlockID;
				notify.m_strSyncSrcIpAddr = pBestHandler->m_strCasIpAddr;

				if (this->SendHtsMsg(notify, pMcpHandler))
				{
					DSC_RUN_LOG_ERROR("send CSyncBlockOnRegistCpsXcsNotify failed.");
				}
			}
		}
	}
	else if (m_nCpsState == EN_MASTER_NORMAL_STATE) //在master状态
	{
		if (rRegistReq.m_nMaxBlockID < m_pConfig->m_nBlockID) //发送同步通知给对端
		{
			VBH::CSyncBlockOnRegistCpsCsOnOrderMasterStateNotify notify;
			VBH::CSyncSourcePeerCasAddress addr;

			for (ACE_UINT32 idx = 0; idx < m_arrCsConnectHandler.Size(); ++idx)
			{
				if (m_arrCsConnectHandler[idx]->m_nPeerID != rRegistReq.m_nPeerID)
				{
					addr.m_nPeerID = m_arrCsConnectHandler[idx]->m_nPeerID;
					addr.m_nPort = m_arrCsConnectHandler[idx]->m_nCasPort;
					addr.m_strIpAddr = m_arrCsConnectHandler[idx]->m_strCasIpAddr;

					notify.m_lstPeerAddr.push_back(addr);
				}
			}
			notify.m_nKafkaBlockID = m_pConfig->m_nBlockID;
			notify.m_strKafkaBlockHash.assign(m_pConfig->m_blockHash, VBH_BLOCK_DIGEST_LENGTH);
			DSC_RUN_LOG_INFO("send CSyncBlockOnRegistCpsCsOnOrderMasterStateNotify sucess，registReq nMaxBlockID：%d， Config nBlockID：%d",rRegistReq.m_nMaxBlockID, m_pConfig->m_nBlockID);
			if (this->SendHtsMsg(notify, pMcpHandler))
			{
				DSC_RUN_LOG_ERROR("send CSyncBlockOnRegistCpsCsOnOrderMasterStateNotify failed.");
			}
		}
		else
		{
			CLocalBlockInfo* pBlockInfo = m_queueBlockInfo.Front();

			if (pBlockInfo) //有正在被发送的区块
			{
				if (rRegistReq.m_nMaxBlockID > pBlockInfo->m_nBlockID) //区块ID就不对
				{
					VBH::CInvalidPeerCpsCsNotify notify;

					notify.m_nInvalidReason = VBH::CInvalidPeerCpsCsNotify::EN_REGIST_BLOCK_ID_INVALID;

					if (this->SendHtsMsg(notify, pMcpHandler))
					{
						DSC_RUN_LOG_ERROR("send CInvalidPeerCpsCsNotify failed.");
					}
				}
				else if (rRegistReq.m_nMaxBlockID == pBlockInfo->m_nBlockID
					&& !rRegistReq.m_strMaxBlockHash.IsEqual(pBlockInfo->m_blockHash, VBH_BLOCK_DIGEST_LENGTH))
				{
					VBH::CBackspaceBlockCpsCsNotify notify;

					notify.m_nRegisterBlockID = rRegistReq.m_nMaxBlockID;
					notify.m_nKafkaBlockID = pBlockInfo->m_nBlockID - 1;

					if (this->SendHtsMsg(notify, pMcpHandler))
					{
						DSC_RUN_LOG_ERROR("send CBackspaceBlockCpsCsNotify failed.");
					}
				}
				else if (rRegistReq.m_nMaxBlockID == m_pConfig->m_nBlockID
					&& !rRegistReq.m_strMaxBlockHash.IsEqual(m_pConfig->m_blockHash, VBH_BLOCK_DIGEST_LENGTH)
					&& m_pConfig->m_nBlockID > 0)
				{
					VBH::CBackspaceBlockCpsCsNotify notify;

					notify.m_nRegisterBlockID = rRegistReq.m_nMaxBlockID;
					notify.m_nKafkaBlockID = m_pConfig->m_nBlockID - 1;

					if (this->SendHtsMsg(notify, pMcpHandler))
					{
						DSC_RUN_LOG_ERROR("send CBackspaceBlockCpsCsNotify failed.");
					}
				}
				else
				{
					VBH::CDistributeBlockCpsCsReq req;

					req.m_nBlockID = pBlockInfo->m_nBlockID;
					req.m_vbhBlock.Set(pBlockInfo->m_pBlockDataBuf, pBlockInfo->m_nBlockDataLen);

					if (this->SendHtsMsg(req, pMcpHandler))
					{
						DSC_RUN_LOG_ERROR("send CDistributeBlockCpsXcsReq failed.");
					}
					else
					{
						pAnchorConnectHandler->m_bRecvDistBlockRsp = false; //清空收到应答标志
						this->SetDscTimer(pAnchorConnectHandler, EN_DISTRIBUTE_TIMEOUT_VALUE);

						VBH_TRACE_MESSAGE("send block:%llu to peer:%d", req.m_nBlockID, pAnchorConnectHandler->m_nPeerID);
					}
				}
			}
			else //没有正在被发送的区块
			{
				if (rRegistReq.m_nMaxBlockID > (m_pConfig->m_nBlockID + 1))
				{
					VBH::CInvalidPeerCpsCsNotify notify;

					notify.m_nInvalidReason = VBH::CInvalidPeerCpsCsNotify::EN_REGIST_BLOCK_ID_INVALID;

					if (this->SendHtsMsg(notify, pMcpHandler))
					{
						DSC_RUN_LOG_ERROR("send CInvalidPeerCpsCsNotify failed.");
					}
				}
				else if (rRegistReq.m_nMaxBlockID == (m_pConfig->m_nBlockID + 1))
				{
					VBH::CBackspaceBlockCpsCsNotify notify;

					notify.m_nRegisterBlockID = rRegistReq.m_nMaxBlockID;
					notify.m_nKafkaBlockID = m_pConfig->m_nBlockID;

					if (this->SendHtsMsg(notify, pMcpHandler))
					{
						DSC_RUN_LOG_ERROR("send CBackspaceBlockCpsCsNotify failed.");
					}
				}
				else if (!rRegistReq.m_strMaxBlockHash.IsEqual(m_pConfig->m_blockHash, VBH_BLOCK_DIGEST_LENGTH)
					&& m_pConfig->m_nBlockID > 0)
				{
					VBH::CBackspaceBlockCpsCsNotify notify;

					notify.m_nRegisterBlockID = rRegistReq.m_nMaxBlockID;
					notify.m_nKafkaBlockID = m_pConfig->m_nBlockID - 1;

					if (this->SendHtsMsg(notify, pMcpHandler))
					{
						DSC_RUN_LOG_ERROR("send CBackspaceBlockCpsCsNotify failed.");
					}
				}
			}
		}
	}

	VBH_MESSAGE_LEAVE_TRACE(CMasterChannelProcessService, VBH::CRegistCsCpsReq);

	return 0;
}

ACE_INT32  CChannelProcessService::OnHtsMsg(VBH::CQueryMaxBlockInfoCsCpsRsp& rQueryMaxBlockInfoCsCpsRsp, CMcpHandler* pMcpHandler)
{

	VBH_MESSAGE_ENTER_TRACE(CMasterChannelProcessService, VBH::CQueryMaxBlockInfoCsCpsRsp);

	CCsConnectSessionHandler* pAnchorConnectHandler = dynamic_cast<CCsConnectSessionHandler*> (pMcpHandler);


	//查重：同一个peer-id，不同的连接句柄
	for (ACE_UINT32 i = 0; i < m_vecPeerInfo.size(); ++i)
	{
		if (rQueryMaxBlockInfoCsCpsRsp.m_nPeerID == m_vecPeerInfo[i].m_nPeerID)
		{
			DSC_RUN_LOG_WARNING("repeat regist req, peer id:%d.", rQueryMaxBlockInfoCsCpsRsp.m_nPeerID);
			return -1;
		}
	}

	CPeerInfo pi;
	pi.m_nCasPort = rQueryMaxBlockInfoCsCpsRsp.m_nCasPort;
	pi.m_nPeerID = rQueryMaxBlockInfoCsCpsRsp.m_nPeerID;
	pi.m_nHandleID = pMcpHandler->GetHandleID();
	pi.m_nRegistMaxBlockID = rQueryMaxBlockInfoCsCpsRsp.m_nMaxBlockID;
	pi.m_strRegistMaxBlockHash = rQueryMaxBlockInfoCsCpsRsp.m_strMaxBlockHash;
	pi.m_strCasIpAddr = rQueryMaxBlockInfoCsCpsRsp.m_strCasIpAddr;

	m_vecPeerInfo.push_back(pi);

	DSC_RUN_LOG_INFO("CQueryMaxBlockInfoCsCpsRsp, peer id:%d, MaxBlockID: %d,\r\n", rQueryMaxBlockInfoCsCpsRsp.m_nPeerID, pi.m_nRegistMaxBlockID);
	DSC_RUN_LOG_WARNING("MaxBlockHash: %s,\r\n", pi.m_strRegistMaxBlockHash.c_str());

	if (m_vecPeerInfo.size() < m_nKafkaValue)
	{
		return 0;//数量不够继续等
	}

	ACE_UINT32 nNormalFront = 0;//排序后第一个"正常peer"下标；
	ACE_UINT32 nNormalRear = 0;//排序后最后一个"正常peer"下标；
	ACE_UINT32 nNormalPeerCount = 0; //记录最好的那段区间

	GetTheMostBlockHeightInfo(nNormalFront, nNormalRear, nNormalPeerCount);

	if (nNormalPeerCount >= m_nKafkaValue) //拥有最高区块高度的节点数达到了kafka共识
	{
		SendSyncBlockNotifyMsg(nNormalFront, nNormalRear);
		if (m_pConfig->m_nBlockID == m_vecPeerInfo[nNormalFront].m_nRegistMaxBlockID)
		{
			m_nCpsState = EN_MASTER_NORMAL_STATE;
			DSC_RUN_LOG_INFO("===order enter master-normal-state: 5===");

			SetDscTimer(&m_packTimerHandler, m_nPackTimeoutValue, true); //设置打包定时器

			this->CancelDscTimer(&m_waitPeerRegistSession);
		}
		else if (m_pConfig->m_nBlockID < m_vecPeerInfo[nNormalFront].m_nRegistMaxBlockID) //order需要从peer同步
		{
			//随机选取1个peer进行同步
			VBH::CMasterSyncVersionTableCpsCsReq req;

			req.m_nBlockID = m_pConfig->m_nBlockID + 1; //要同步的区块高度为当前区块高度+1
			if (this->SendHtsMsg(req, m_vecPeerInfo[nNormalFront].m_nHandleID))
			{
				DSC_RUN_LOG_ERROR("send CMasterSyncVersionTableCpsXcsReq failed.");
			}
			else
			{
				m_nCpsState = EN_MASTER_SYNC_STATE;
				DSC_RUN_LOG_INFO("===order enter master-sync-state: 4===");

				this->CancelDscTimer(&m_waitPeerRegistSession);

				//创建并启动order同步定时器
				m_orderSyncSession.m_nKafkaBlockID = m_vecPeerInfo[nNormalFront].m_nRegistMaxBlockID;
				m_orderSyncSession.m_strKafkaBlockHash = m_vecPeerInfo[nNormalFront].m_strRegistMaxBlockHash;
				this->SetDscTimer(&m_orderSyncSession, EN_ORDER_SYNC_TIMEOUT_VALUE, true);
			}
		}
		else
		{
			//不会出现的情况，检查程序逻辑
			DSC_RUN_LOG_ERROR("Order BlockID[%llu] > Best Peer BlockID[%llu].", m_pConfig->m_nBlockID, m_vecPeerInfo[nNormalFront].m_nRegistMaxBlockID);
		}
	}
	else
	{
		//未注册数+BestPeer数 < Kafka数，需要进行退格处理 //退格后需要从连接列表中移除
		if ((m_nPeerCount - m_vecPeerInfo.size() + nNormalPeerCount) < m_nKafkaValue)
		{
			//Order的区块必然 <= 【最高区块高度-1】
			ACE_ASSERT(m_pConfig->m_nBlockID <= m_vecPeerInfo[nNormalFront].m_nRegistMaxBlockID - 1);

			//1. 对高于最高 【最高区块高度-1】 的节点，执行退块指令；对没有达到【最高区块高度-1】的节点，执行同步指令
			const ACE_UINT64 nKafkaBlockID = m_vecPeerInfo[nNormalFront].m_nRegistMaxBlockID - 1;
			ACE_UINT32 nNormalCursor = 0;
			VBH::CBackspaceBlockCpsCsNotify backspaceBlockNotify;
			VBH::CSyncBlockOnRegistCpsCsNotify syncBlockNotify;

			for (ACE_UINT32 idx = 0; idx < m_vecPeerInfo.size(); ++idx)
			{
				if (m_vecPeerInfo[idx].m_nRegistMaxBlockID > nKafkaBlockID)
				{ //扣块
					backspaceBlockNotify.m_nRegisterBlockID = m_vecPeerInfo[idx].m_nRegistMaxBlockID;
					backspaceBlockNotify.m_nKafkaBlockID = nKafkaBlockID;

					if (this->SendHtsMsg(backspaceBlockNotify, m_vecPeerInfo[idx].m_nHandleID))
					{
						DSC_RUN_LOG_ERROR("send CBackspaceBlockCpsXcsNotify failed.");
					}
					else
					{
						//发送成功，将peer从注册列表中移除
						this->EraseHandler(m_vecPeerInfo[idx].m_nPeerID);
					}
				}
				else if (m_vecPeerInfo[idx].m_nRegistMaxBlockID < nKafkaBlockID)
				{//补块
					syncBlockNotify.m_nMaxBlockHashIsValid = VBH::CSyncBlockOnRegistCpsCsNotify::EN_HASH_NOT_VERIFY;
					syncBlockNotify.m_nRegisterBlockID = m_vecPeerInfo[idx].m_nRegistMaxBlockID;
					syncBlockNotify.m_nKafkaBlockID = nKafkaBlockID;
					syncBlockNotify.m_nSyncSrcPeerID = m_vecPeerInfo[nNormalCursor].m_nPeerID;
					syncBlockNotify.m_nSyncSrcPort = m_vecPeerInfo[nNormalCursor].m_nCasPort;
					syncBlockNotify.m_strSyncSrcIpAddr = m_vecPeerInfo[nNormalCursor].m_strCasIpAddr;

					if (this->SendHtsMsg(syncBlockNotify, m_arrCsConnectHandler[idx]))
					{
						DSC_RUN_LOG_ERROR("send CSyncBlockOnRegistCpsXcsNotify failed.");
					}

					++nNormalCursor;
					if (m_vecPeerInfo[nNormalCursor].m_nRegistMaxBlockID <= nKafkaBlockID)
					{
						nNormalCursor = 0;
					}
				}
			}
		}
	}
	return 0;
}

ACE_INT32 CChannelProcessService::OnHtsMsg(VBH::CMasterSyncVersionTableCsCpsRsp& rSyncVersionTable, CMcpHandler* pMcpHandler)
{
	VBH_MESSAGE_ENTER_TRACE(CMasterChannelProcessService, VBH::CMasterSyncVersionTableCsCpsRsp);

	DSC_RUN_LOG_INFO("receive CMasterSyncVersionTableCsCpsRsp .");

	if (m_nCpsState == EN_MASTER_SYNC_STATE) //只在order同步状态接收同步应答
	{
		this->ResetDscTimer(&m_orderSyncSession, EN_ORDER_SYNC_TIMEOUT_VALUE); //重置定时器

		if ((m_pConfig->m_nBlockID + 1) == rSyncVersionTable.m_nBlockID)
		{
			if (!rSyncVersionTable.m_lstKv.empty()) //version-table要非空
			{
				//1. 校验version值是否是 一次+1 的增加
				bool bIsOk = true;
				VBH_CLS::CVersionTableItem versionTableItem;

				for (auto& it : rSyncVersionTable.m_lstKv)
				{
					if (it.m_nVersion) //非0的version, 之前肯定在表中已存在了
					{
						if (m_wsVersionTable.Read(versionTableItem, it.m_nAllocatedID))
						{
							DSC_RUN_LOG_ERROR("read write-set-version-table failed, aloc-id:%#llu", it.m_nAllocatedID);
							bIsOk = false;
							break;
						}
						if ((versionTableItem.m_nVersion + 1) != it.m_nVersion)
						{
							DSC_RUN_LOG_ERROR("cps current version:%u, sync-verion:%u, write-set-id:%llu", versionTableItem.m_nVersion, it.m_nVersion, it.m_nAllocatedID);
							bIsOk = false;
							break;
						}
					}
				}
				DSC_RUN_LOG_INFO("version  check is OK? ：%d ", bIsOk);
				//2. 校验过程未出错，则将所有的version号存入
				if (bIsOk)
				{
					ACE_UINT64 nMaxAlocWsID = 0;

					for (auto& it : rSyncVersionTable.m_lstKv)
					{
						if (it.m_nAllocatedID > nMaxAlocWsID)
						{
							nMaxAlocWsID = it.m_nAllocatedID;
						}

						versionTableItem.m_nVersion = it.m_nVersion;

						if (versionTableItem.m_nVersion) //
						{
							if (m_wsVersionTable.Update(it.m_nAllocatedID, versionTableItem))
							{
								//todo: 通过网管告警
								DSC_RUN_LOG_ERROR("update write-set-version-table failed, key-aloc-id:%#llX, channnel-id:%d", it.m_nAllocatedID, m_nChannelID);
								bIsOk = false; //失败标记
								break;
							}
						}
						else //version == 0，是新增的情况
						{
							if (m_wsVersionTable.Append(it.m_nAllocatedID, versionTableItem))
							{
								//todo: 通过网管告警
								DSC_RUN_LOG_ERROR("append write-set-version-table failed, key-aloc-id:%#llX, channnel-id:%d", it.m_nAllocatedID, m_nChannelID);
								bIsOk = false; //失败标记
								break;
							}
						}
					}
					DSC_RUN_LOG_INFO("update table is OK? ：%d ", bIsOk);
					if (bIsOk) //过程中没有失败
					{
						//3. 打包变更
						m_wsVersionTable.PackModify();

						//4. 开始存储流程 //存储过程中失败，则通过再次请求来触发存储操作
						//4.1 写version-table日志
						if (DSC_UNLIKELY(m_wsVersionTable.SaveToLog()))
						{
							//TODO: 向网管告警
							DSC_RUN_LOG_ERROR("write-set-version-table save-to-log failed.");
						}
						else
						{
							//4.2 记录config日志
							m_pCfgLog->m_nBlockID = rSyncVersionTable.m_nBlockID;
							m_pCfgLog->m_nLastAlocWsID = nMaxAlocWsID;
							::memcpy(m_pCfgLog->m_blockHash, rSyncVersionTable.m_strBlockHash.data(), VBH_BLOCK_DIGEST_LENGTH);

							//5. 置标志，存盘，清标志
							//==============开始操作
							m_pConfig->m_nModifyStorageState = EN_BEGIN_MODIFY_STORAGE;
							//5.1 version-table的持久化
							if (DSC_UNLIKELY(m_wsVersionTable.Persistence()))
							{
								//TODO: 向网管告警
								DSC_RUN_LOG_ERROR("write-set-version table persistence failed.");
								m_pConfig->m_nModifyStorageState = EN_END_MODIFY_STORAGE; //==============结束操作
							}
							else
							{
								//5.2 config的持久化
								m_pConfig->m_nBlockID = m_pCfgLog->m_nBlockID;
								m_pConfig->m_nLastAlocWsID = m_pCfgLog->m_nLastAlocWsID;
								::memcpy(m_pConfig->m_blockHash, m_pCfgLog->m_blockHash, VBH_BLOCK_DIGEST_LENGTH);
								m_pConfig->m_nModifyStorageState = EN_END_MODIFY_STORAGE; //==============结束操作

								//6. 提交事务，进行提交清理
								m_wsVersionTable.CommitteTransaction();
							}
						}
					}
				}
				else
				{
					//TODO: 序列号冲突, 向网管告警，//肯定是peer状态错了
				}
			}


			if (m_pConfig->m_nBlockID == m_orderSyncSession.m_nKafkaBlockID)
			{
				m_nCpsState = EN_MASTER_NORMAL_STATE;
				DSC_RUN_LOG_INFO("===order enter master-state: 5===");

				SetDscTimer(&m_packTimerHandler, m_nPackTimeoutValue, true); //设置打包定时器

				this->CancelDscTimer(&m_orderSyncSession);
			}
			else //未达到指定的区块高度，则再发起同步
			{
				VBH::CMasterSyncVersionTableCpsCsReq req;

				req.m_nBlockID = m_pConfig->m_nBlockID + 1;
				DSC_RUN_LOG_INFO("send CMasterSyncVersionTableCpsXcsReq ：%d ", req.m_nBlockID);
				if (this->SendHtsMsg(req, pMcpHandler))
				{
					DSC_RUN_LOG_ERROR("send CMasterSyncVersionTableCpsXcsReq failed.");
				}
			}
		}
		else //block-id不匹配
		{
			DSC_RUN_LOG_ERROR("recv CMasterSyncVersionTableXcsCpsRsp, block-id:%lld, but expect block-id:%lld", rSyncVersionTable.m_nBlockID, m_pConfig->m_nBlockID + 1);
		}

	}
	else
	{
		DSC_RUN_LOG_ERROR("receive CMasterSyncVersionTableXcsCpsRsp when not EN_ORDER_SYNC_STATE.");
	}

	VBH_MESSAGE_LEAVE_TRACE(CMasterChannelProcessService, VBH::CMasterSyncVersionTableCsCpsRsp);

	return 0;
}

//注册用户的事务类型 //事务类型和流水号合并一处使用，放在事务结构外部 //打包事务时为注册用户分配的流水号，用于校验 //transaction-key的校验，存放在区块中
ACE_INT32 CChannelProcessService::OnHtsMsg(VBH::CSubmitRegistUserTransactionTasCpsReq& rSubmitTransReq, CMcpHandler* pMcpHandler)
{
	VBH_MESSAGE_ENTER_TRACE(CMasterChannelProcessService, VBH::CSubmitRegistUserTransactionTasCpsReq);

	VBH::CSubmitRegistUserTransactionCpsTasRsp rsp;
	VBH::CRegistUserTransaction registUserTrans;
	VBH::CProposeTransaction proposeTrans;
	ACE_UINT32 nRegistTransSequenceNumber; //注册事务的序列号

	rsp.m_nReturnCode = VBH::EN_OK_TYPE;
	rsp.m_nTasSessionID = rSubmitTransReq.m_nTasSessionID;

	if (m_nCpsState == EN_MASTER_NORMAL_STATE)
	{
		if (m_nQueueBlockNum < m_nMaxCacheBlockCount) //cache已经达到上限了，暂时不接收新达到的事务
		{
			//判断剩余的allocated-user-id是否够分配
			if (DSC_LIKELY(m_pCurBlockInfo->m_nLastAlocWsID < DEF_LAST_WRITE_SET_ALLOCATED_ID)) //系统ID在此次分配时已经溢出了
			{
				registUserTrans.m_cltPubKey = rSubmitTransReq.m_cltPubKey;
				registUserTrans.m_svrPriKey = rSubmitTransReq.m_svrPriKey;
				registUserTrans.m_envelopeKey = rSubmitTransReq.m_envelopeKey;

				//TODO: 后面实现时: 如果有创建时转账 转账发起方，提案，签名，都会被附加到 req 中提交到order
				proposeTrans.m_vecWsItem.Open(1);
				proposeTrans.m_vecWsItem[0].m_nVersion = 0;
				proposeTrans.m_vecWsItem[0].m_value = rSubmitTransReq.m_userInfo;

				//1. 获取事务(两个事务)编码后的长度 //编码时还要再加上流水号编码空间
				DSC::CDscCodecGetSizer getSizer;

				//1.1 sequence-number + VBH::CRegistUserTransaction
				getSizer.GetSize(nRegistTransSequenceNumber);
				getSizer.GetSize(registUserTrans);

				//1.2 sequence-number + VBH::CProposeTransaction
				getSizer.GetSize(rsp.m_userKey.m_nSequenceNumber);
				getSizer.GetSize(proposeTrans);

				//2 区块剩余空间不够容纳该事务，则重置区块大小
				if (getSizer.GetSize() + m_pCurBlockInfo->m_nBlockDataLen > m_pCurBlockInfo->m_nBlockBufSize)
				{
					m_nBlockBufInitialSize = getSizer.GetSize() + m_pCurBlockInfo->m_nBlockDataLen;
					m_pCurBlockInfo->Resize(m_nBlockBufInitialSize);
					m_blockEncoder.ResetEncodeBuffer(m_pCurBlockInfo->m_pBlockDataBuf, m_pCurBlockInfo->m_nBlockDataLen);
				}

				//3. 为事务生成事务ID
				nRegistTransSequenceNumber = VBH::CTransactionSequenceNumber::CombineSequenceNumber(VBH::CTransactionSequenceNumber::EN_REGIST_USER_TRANSACTION_TYPE, m_pConfig->m_nSequenceNumber);
				rsp.m_userKey.m_nSequenceNumber = VBH::CTransactionSequenceNumber::CombineSequenceNumber(VBH::CTransactionSequenceNumber::EN_PROPOSE_TRANSACTION_TYPE, m_pConfig->m_nSequenceNumber);
				VBH::CTransactionSequenceNumber::SequenceNumberInc(m_pConfig->m_nSequenceNumber); //事务流水号+1
				rsp.m_registTransUrl.m_nBlockID = m_pCurBlockInfo->m_nBlockID;
				rsp.m_registTransUrl.m_nTransIdx = m_blockEncoder.m_bcBlockHeader.m_nTransCount;//事务编号：blockID + 块内事务数组下标，数组下标从0开始

				//4. 为user生成系统ID
				rsp.m_userKey.m_nAllocatedID = m_pCurBlockInfo->m_nLastAlocWsID + 1;
				registUserTrans.m_nUserID = rsp.m_userKey.m_nAllocatedID;
				proposeTrans.m_vecWsItem[0].m_nAllocatedID = rsp.m_userKey.m_nAllocatedID;
			}
			else
			{
				DSC_RUN_LOG_ERROR("system-id has been used up, channel-id:%d.", m_nChannelID);
				rsp.m_nReturnCode = VBH::EN_WRITE_SET_SYSTEM_ID_USED_UP;
			}
		}
		else //cache未满
		{
			DSC_RUN_LOG_ERROR("block cache already full, channel-id:%d.", m_nChannelID);
			rsp.m_nReturnCode = VBH::EN_ORDER_CPS_BLOCK_CACHE_ALREADY_FULL;
		}
	}
	else
	{
		DSC_RUN_LOG_ERROR("order-cps is not ready, channel-id:%d.", m_nChannelID);
		rsp.m_nReturnCode = VBH::EN_ORDER_IS_NOT_READY_TYPE;
	}

	//3. 发送应答
	if (this->SendHtsMsg(rsp, pMcpHandler))
	{
		DSC_RUN_LOG_ERROR("send hts message error, channel-id:%d", m_nChannelID);
		rsp.m_nReturnCode = VBH::EN_NETWORK_ERROR_TYPE;
	}

	//只有在完全没有出错的情况下，才进行实际的编码
	if(VBH::EN_OK_TYPE == rsp.m_nReturnCode)
	{
		//到目前为止，流程都成功，记录分配的系统序号（否则分配的系统序号自动回滚）
		m_pCurBlockInfo->m_nLastAlocWsID = rsp.m_userKey.m_nAllocatedID;

		//4 对事务进行编码，如果编码后数据达到预定值，则发送区块

		// 填充事务，并测算待编码的事务码流长度
		//==========开始编码
		//------- 注册事务
		DSC::CDscNetCodecEncoder& encoder = m_blockEncoder.BeginEncodeTransaction();
		encoder.Encode(nRegistTransSequenceNumber);
		encoder.Encode(registUserTrans);
		m_blockEncoder.EndEncodeTransaction();

		//-------提案事务
		m_blockEncoder.BeginEncodeTransaction();
		encoder.Encode(rsp.m_userKey.m_nSequenceNumber);
		encoder.Encode(proposeTrans);
		m_blockEncoder.EndEncodeTransaction();
		//==========结束编码

		m_pCurBlockInfo->m_nBlockDataLen = m_blockEncoder.GetEncodeDataSize(); //记录编码后数据长度

		//记录新创建用户的版本号到user-version-table
		VBH_CLS::CVersionTableItem versionTableItem;

		versionTableItem.m_nVersion = 0;

		if (m_wsVersionTable.Append(rsp.m_userKey.m_nAllocatedID, versionTableItem))
		{
			//向网管告警
			DSC_RUN_LOG_ERROR("write-set version table append failed, alock-id:%lld", rsp.m_userKey.m_nAllocatedID);

			//append失败，说明有更深层次的问题，最简单做法：丢弃整个未打包完成的块，并告警
			RollbackPartBlock();
			ResetDscTimer(&m_packTimerHandler, m_nPackTimeoutValue); //重置定时器
		}
		else
		{
			//满足区块打包条件了
			if ((m_blockEncoder.m_bcBlockHeader.m_nTransCount == VBH::CBcBlockHeader::EN_MAX_TRANSACTION_COUNT_IN_BLOCK)
				|| (m_pCurBlockInfo->m_nBlockDataLen >= m_nPackMaxBlockSizeValue))
			{
				OnTimePackBlock();
				ResetDscTimer(&m_packTimerHandler, m_nPackTimeoutValue); //重置定时器
			}
		}
	}

	VBH_MESSAGE_LEAVE_TRACE(CMasterChannelProcessService, VBH::CSubmitRegistUserTransactionTasCpsReq);

	return 0;
}



ACE_INT32 CChannelProcessService::CheckOrderIsNormal()
{

	if (m_nCpsState != EN_MASTER_NORMAL_STATE)
	{
		DSC_RUN_LOG_ERROR("order-cps is not ready, channel-id:%d.", m_nChannelID);
		return VBH::EN_ORDER_IS_NOT_READY_TYPE;
	}

	if (m_nQueueBlockNum >= m_nMaxCacheBlockCount)
	{

		DSC_RUN_LOG_ERROR("block cache already full, channel-id:%d.", m_nChannelID);
		return VBH::EN_ORDER_CPS_BLOCK_CACHE_ALREADY_FULL;
	}

	return VBH::EN_OK_TYPE;
}

ACE_INT32 CChannelProcessService::CheckWsItemVersion(VBH::CSimpleVector<VBH::CVbhWriteSetItem>& vecWsItem)
{
	VBH_CLS::CVersionTableItem versionTableItem;

	for (ACE_UINT16 nWsIdx = 0; nWsIdx < vecWsItem.Size(); ++nWsIdx)
	{
		if (m_wsVersionTable.Read(versionTableItem, vecWsItem[nWsIdx].m_nAllocatedID))
		{
			DSC_RUN_LOG_ERROR("read write-set-version-table failed, aloc-id:%lld", vecWsItem[nWsIdx].m_nAllocatedID);

			return VBH::EN_SYSTEM_ERROR_TYPE;
		}

		if ((vecWsItem[nWsIdx].m_nVersion <= versionTableItem.m_nVersion) && (vecWsItem[nWsIdx].m_nVersion != 0xFFFFFFFF)) //小于等于要检查是否环回
		{
			DSC_RUN_LOG_WARNING("write set version confic, vWs:%u, vTable:%u, key-aloc-id:%llu, cached-block-count:%u",
				vecWsItem[nWsIdx].m_nVersion, versionTableItem.m_nVersion, vecWsItem[nWsIdx].m_nAllocatedID,
				m_nQueueBlockNum );

			return VBH::EN_LOGIC_FAILED_ERROR_TYPE;
		}
	}
	return VBH::EN_OK_TYPE;

}

ACE_INT32 CChannelProcessService::ProposalTransaction(VBH::CSubmitProposalTransactionTasCpsReq& rSubmitTransReq, VBH::CSubmitProposalTransactionCpsTasRsp &rsp, VBH::CProposeTransaction & proposeTransaction)
{

	rsp.m_nReturnCode = CheckOrderIsNormal();

	if (VBH::EN_OK_TYPE != rsp.m_nReturnCode)
	{
		return 0;
	}

	DSC::CDscNetCodecDecoder decoder(rSubmitTransReq.m_transContent.GetBuffer(), rSubmitTransReq.m_transContent.GetSize());
	if (decoder.Decode(proposeTransaction))
	{
		DSC_RUN_LOG_ERROR("decode transaction failed, tas-session-id:%d, channel-id:%d.", rSubmitTransReq.m_nTasSessionID, m_nChannelID);
		rsp.m_nReturnCode = VBH::EN_DECODE_ERROR_TYPE;
		return 0;
	}
	//生成information id不需要检查
	if (CREAT_INFORMATION_ID != rSubmitTransReq.m_nActionID)
	{
		rsp.m_nReturnCode == CheckWsItemVersion(proposeTransaction.m_vecWsItem);

		if (VBH::EN_OK_TYPE != rsp.m_nReturnCode)
		{
			return 0;
		}
	}

	if (rsp.m_nReturnCode == VBH::EN_OK_TYPE)
	{
		//3. 计算事务编码后size
		DSC::CDscCodecGetSizer getSizer;

		getSizer.GetSize(rsp.m_alocTransKey.m_nSequenceNumber); //sequence-number
		getSizer.AddSize(rSubmitTransReq.m_transContent.GetSize()); //cc已经打包好的 VBH::CProposeTransaction 结构

		//4. 区块剩余空间不够容纳该事务，则重置区块大小
		if (getSizer.GetSize() + m_pCurBlockInfo->m_nBlockDataLen > m_pCurBlockInfo->m_nBlockBufSize)
		{
			m_nBlockBufInitialSize = getSizer.GetSize() + m_pCurBlockInfo->m_nBlockDataLen;
			m_pCurBlockInfo->Resize(m_nBlockBufInitialSize);
			m_blockEncoder.ResetEncodeBuffer(m_pCurBlockInfo->m_pBlockDataBuf, m_pCurBlockInfo->m_nBlockDataLen);
		}

		//5. 为事务生成事务ID
		rsp.m_alocTransKey.m_nSequenceNumber = VBH::CTransactionSequenceNumber::CombineSequenceNumber(VBH::CTransactionSequenceNumber::EN_PROPOSE_TRANSACTION_TYPE, m_pConfig->m_nSequenceNumber);
		VBH::CTransactionSequenceNumber::SequenceNumberInc(m_pConfig->m_nSequenceNumber); //事务流水号+1
		rsp.m_alocTransKey.m_nBlockID = m_pCurBlockInfo->m_nBlockID;
		rsp.m_alocTransKey.m_nTransIdx = m_blockEncoder.m_bcBlockHeader.m_nTransCount; //事务编号：blockID + 块内事务数组下标，数组下标从0开始

		if (CREAT_INFORMATION_ID == rSubmitTransReq.m_nActionID)
		{
			rsp.m_vecInfoID.Open(proposeTransaction.m_vecWsItem.Size());
			for (ACE_UINT16 idx = 0; idx < proposeTransaction.m_vecWsItem.Size(); ++idx)
			{
				proposeTransaction.m_vecWsItem[idx].m_nAllocatedID = m_pCurBlockInfo->m_nLastAlocWsID + idx + 1;
				proposeTransaction.m_vecWsItem[idx].m_nVersion = 0;
				rsp.m_vecInfoID[idx].m_nAllocatedID = m_pCurBlockInfo->m_nLastAlocWsID + idx + 1;
				rsp.m_vecInfoID[idx].m_nSequenceNumber = rsp.m_alocTransKey.m_nSequenceNumber;
			}
		}
	}
	return 0;

}
//发起提案的事务类型 //事务类型和流水号合并一处使用，放在事务结构外部 //打包事务时为注册用户分配的流水号，用于校验 //transaction-key的校验，存放在区块中
//发起提案事务 仅存在概念类型，不定义实际的类
//其中，CProposeTransaction在CC时已经打包好为一个数据块，数据块转发到CPS时，可以直接放在打包后的 流水号 数据后面
ACE_INT32 CChannelProcessService::OnHtsMsg(VBH::CSubmitProposalTransactionTasCpsReq& rSubmitTransReq, CMcpHandler* pMcpHandler)
{
	VBH_MESSAGE_ENTER_TRACE(CMasterChannelProcessService, VBH::CSubmitProposalTransactionTasCpsReq);
	DSC_RUN_LOG_FINE("CSubmitProposalTransactionTasCpsReq %d", m_nChannelID);
	VBH::CSubmitProposalTransactionCpsTasRsp rsp;
	rsp.m_nReturnCode = VBH::EN_OK_TYPE;
	rsp.m_nTasSessionID = rSubmitTransReq.m_nTasSessionID;
	VBH::CProposeTransaction proposeTrans;
	rsp.m_nActionID = rSubmitTransReq.m_nActionID;

	ProposalTransaction(rSubmitTransReq, rsp, proposeTrans);

	//5. 发送应答
	if (this->SendHtsMsg(rsp, pMcpHandler))
	{
		DSC_RUN_LOG_ERROR("send hts message failed, channel-id:%d.", m_nChannelID);
		rsp.m_nReturnCode = VBH::EN_NETWORK_ERROR_TYPE;
	}

	//6. 整个过程没有出错，则将事务打包到cache中，//如果编码后数据达到预定值，则发送区块
	if (VBH::EN_OK_TYPE == rsp.m_nReturnCode)
	{
		//对事务进行编码
		DSC::CDscNetCodecEncoder& encoder = m_blockEncoder.BeginEncodeTransaction();

		//=========开始编码
		encoder.Encode(rsp.m_alocTransKey.m_nSequenceNumber); //sequence-number
		encoder.Encode(proposeTrans);
		//=========结束编码

		m_blockEncoder.EndEncodeTransaction();
		m_pCurBlockInfo->m_nBlockDataLen = m_blockEncoder.GetEncodeDataSize(); //记录编码后数据长度

		//7. 将本次事务的版本号更新到version-table中
		VBH_CLS::CVersionTableItem versionTableItem;
		bool bUpdateSuccess = true;

		for (ACE_UINT16 nWsIdx = 0; nWsIdx < proposeTrans.m_vecWsItem.Size(); ++nWsIdx)
		{
			versionTableItem.m_nVersion = proposeTrans.m_vecWsItem[nWsIdx].m_nVersion;
			if ((CREAT_INFORMATION_ID != rSubmitTransReq.m_nActionID))
			{

				if (m_wsVersionTable.Update(proposeTrans.m_vecWsItem[nWsIdx].m_nAllocatedID, versionTableItem))
				{
					DSC_RUN_LOG_ERROR("update write-set-version-table failed, key-aloc-id:%llu, channnel-id:%d", proposeTrans.m_vecWsItem[nWsIdx].m_nAllocatedID, m_nChannelID);

					bUpdateSuccess = false;
					RollbackPartBlock();
					ResetDscTimer(&m_packTimerHandler, m_nPackTimeoutValue); //重置定时器
					break;
				}
			}
			else
			{
				m_pCurBlockInfo->m_nLastAlocWsID++;
				if (m_wsVersionTable.Append(proposeTrans.m_vecWsItem[nWsIdx].m_nAllocatedID, versionTableItem))
				{
					DSC_RUN_LOG_ERROR("Append write-set-version-table failed, key-aloc-id:%llu, channnel-id:%d", proposeTrans.m_vecWsItem[nWsIdx].m_nAllocatedID, m_nChannelID);

					bUpdateSuccess = false;
					RollbackPartBlock();
					ResetDscTimer(&m_packTimerHandler, m_nPackTimeoutValue); //重置定时器
					break;
				}
			}
		}


		if (bUpdateSuccess)
		{
			//满足条件则发送
			if ((m_blockEncoder.m_bcBlockHeader.m_nTransCount == VBH::CBcBlockHeader::EN_MAX_TRANSACTION_COUNT_IN_BLOCK)
				|| (m_pCurBlockInfo->m_nBlockDataLen >= m_nPackMaxBlockSizeValue))
			{
				OnTimePackBlock();
				ResetDscTimer(&m_packTimerHandler, m_nPackTimeoutValue); //重置定时器
			}
		}
	}

	VBH_MESSAGE_LEAVE_TRACE(CMasterChannelProcessService, VBH::CSubmitProposalTransactionTasCpsReq);
	return 0;
}

ACE_INT32 CChannelProcessService::OnHtsMsg(VBH::CDistributeBlockXcsCpsRsp& rDistBlockRsp, CMcpHandler* pMcpHandler)
{
	VBH_MESSAGE_ENTER_TRACE(CMasterChannelProcessService, VBH::CDistributeBlockXcsCpsRsp);

	VBH_TRACE_MESSAGE("recv peer:%d 's distribute block rsp, block-id:%lld, channel-id:%d.", rDistBlockRsp.m_nPeerID, rDistBlockRsp.m_nBlockID, m_nChannelID);

	CLocalBlockInfo* pBlockInfo = m_queueBlockInfo.Front();

	if (pBlockInfo && (pBlockInfo->m_nBlockID == rDistBlockRsp.m_nBlockID))
	{
		CCsConnectSessionHandler* pAnchorConnectHandle = dynamic_cast<CCsConnectSessionHandler*> (pMcpHandler);

		if (pAnchorConnectHandle->m_bRecvDistBlockRsp) //之前没有收到该连接的应答 //去重
		{
			//重复应答消息
		}
		else
		{
			pAnchorConnectHandle->m_bRecvDistBlockRsp = true;
			++m_nDistBlockRspPeerCount;

			//1. 判断是否已经满足发送成功条件（超过阈值个数的peer都发送会了应答）
			if (m_nDistBlockRspPeerCount >= m_nKafkaValue) //超过阈值，则表示发送成功
			{
				//2. 取消所有的重发定时器
				for (ACE_UINT32 idx = 0; idx < m_arrCsConnectHandler.Size(); ++idx)
				{
					this->CancelDscTimer(m_arrCsConnectHandler[idx]);
				}

				//3. 对区块头进行持久化, 失败则启动重试定时器，成功则尝试发送下一个区块
				if (DSC_UNLIKELY(PersistentBlockQueueHeader()))
				{
					this->SetDscTimer(&m_repersistBlockQueueHeaderSession, EN_REPERSIST_BLOCK_QUEUE_HEADER_TIMEOUT_VALUE, true);
				}
				else
				{
					if (m_nQueueBlockNum)//4. 如果队列不空，继续发送下1个区块
					{
						DistributeBlock();
					}
				}
			}
			else
			{
				this->CancelDscTimer(pAnchorConnectHandle);//取消自己的重发定时器
			}
		}
	}
	else
	{
		DSC_RUN_LOG_WARNING("recv-rsp block-id:%lld, not match sending block, channel-id:%d.", rDistBlockRsp.m_nBlockID, m_nChannelID);
	}

	VBH_MESSAGE_LEAVE_TRACE(CMasterChannelProcessService, VBH::CDistributeBlockXcsCpsRsp);

	return 0;
}

ACE_INT32 CChannelProcessService::OnHtsMsg(VBH::CQuerySyncSourcePeerCsCpsReq& rQuerySyncSrcReq, CMcpHandler* pMcpHandler)
{
	VBH_MESSAGE_ENTER_TRACE(CMasterChannelProcessService, VBH::CQuerySyncSourcePeerCsCpsReq);

	VBH::CQuerySyncSourcePeerCpsCsRsp rsp;
	VBH::CSyncSourcePeerCasAddress addr;

	for (ACE_UINT32 idx = 0; idx < m_arrCsConnectHandler.Size(); ++idx)
	{
		if (m_arrCsConnectHandler[idx]->m_nPeerID != rQuerySyncSrcReq.m_nPeerID)
		{
			addr.m_nPeerID = m_arrCsConnectHandler[idx]->m_nPeerID;
			addr.m_nPort = m_arrCsConnectHandler[idx]->m_nCasPort;
			addr.m_strIpAddr = m_arrCsConnectHandler[idx]->m_strCasIpAddr;

			rsp.m_lstPeerAddr.push_back(addr);
		}
	}
	rsp.m_nKafkaBlockID = m_pConfig->m_nBlockID;
	rsp.m_strKafkaBlockHash.assign(m_pConfig->m_blockHash, VBH_BLOCK_DIGEST_LENGTH);

	if (this->SendHtsMsg(rsp, pMcpHandler))
	{
		DSC_RUN_LOG_ERROR("send CQuerySyncSourcePeerCpsCsRsp failed.");
	}

	VBH_MESSAGE_LEAVE_TRACE(CMasterChannelProcessService, VBH::CQuerySyncSourcePeerCsCpsReq);

	return 0;
}

CMcpServerHandler* CChannelProcessService::AllocMcpHandler(ACE_HANDLE handle)
{
	return DSC_THREAD_DYNAMIC_TYPE_NEW(CCsConnectSessionHandler) CCsConnectSessionHandler(*this, handle, this->AllocHandleID());
}

void CChannelProcessService::OnNetworkError(CMcpHandler* pMcpHandler)
{
	CCsConnectSessionHandler* pAnchorConnectHandler = dynamic_cast<CCsConnectSessionHandler*> (pMcpHandler);

	//AS的连接出错，则删除该连接的信息
	this->CancelDscTimer(pAnchorConnectHandler);

	//在anchor发起网络连接，收到注册请求前并未加入队列
	if (pAnchorConnectHandler->m_nIndex != CDscTypeArray<CCsConnectSessionHandler>::EN_INVALID_INDEX_ID)
	{
		DSC_RUN_LOG_INFO("peer:%d's connection network error, erase this handler.", pAnchorConnectHandler->m_nPeerID);
		m_arrCsConnectHandler.Erase(pAnchorConnectHandler);
	}

	CDscHtsServerService::OnNetworkError(pMcpHandler);
}

ACE_INT32 CChannelProcessService::GetVbhMasterOrderProfile(void)
{
	ACE_INT32 nKafkaValue;
	if (VBH::GetVbhProfileInt("KAFKA_VALUE", nKafkaValue))
	{
		DSC_RUN_LOG_ERROR("read KAFKA_VALUE failed.");
		return -1;
	}
	if (nKafkaValue <= 1)
	{
		DSC_RUN_LOG_ERROR("KAFKA_VALUE[%d] value invalid", nKafkaValue);
		return -1;
	}
	m_nKafkaValue = (ACE_UINT32)nKafkaValue;
	if (m_nKafkaValue > m_nPeerCount)
	{
		DSC_RUN_LOG_ERROR("KAFKA_VALUE[%d] must be <= PEER_COUNT[%d]", m_nKafkaValue, m_nPeerCount);
		return -1;
	}

	//读取数据库中 仅属于master-order 的配置参数
	ACE_INT32 nPackTimeoutValue;

	if (VBH::GetVbhProfileInt("PACK_TIMEOUT", nPackTimeoutValue))
	{
		DSC_RUN_LOG_ERROR("read PACK_TIMEOUT failed.");

		return -1;
	}
	if (nPackTimeoutValue < 0)
	{
		DSC_RUN_LOG_ERROR("PACK_TIMEOUT[%d] value invalid", nPackTimeoutValue);
		return -1;
	}
	m_nPackTimeoutValue = (ACE_UINT32)nPackTimeoutValue;

	ACE_INT32 nPackMaxBlockSizeValue;

	if (VBH::GetVbhProfileInt("PACK_MAX_BLOCK_SIZE", nPackMaxBlockSizeValue))
	{
		DSC_RUN_LOG_ERROR("read PACK_MAX_BLOCK_SIZE failed.");

		return -1;
	}
	if (nPackMaxBlockSizeValue < 0)
	{
		DSC_RUN_LOG_ERROR("PACK_MAX_BLOCK_SIZE[%d] value invalid", nPackMaxBlockSizeValue);
		return -1;
	}
	m_nPackMaxBlockSizeValue = (ACE_UINT32)nPackMaxBlockSizeValue;
	m_nPackMaxBlockSizeValue *= 1024 * 1024; //读取的单位为MB，类中存储B
	m_nBlockBufInitialSize = m_nPackMaxBlockSizeValue;

	ACE_INT32 nMaxCacheBlockCount;

	if (VBH::GetVbhProfileInt("MAX_CACHE_BLOCK_COUNT", nMaxCacheBlockCount))
	{
		DSC_RUN_LOG_ERROR("read MAX_CACHE_BLOCK_COUNT failed.");

		return -1;
	}
	if (nMaxCacheBlockCount < 0)
	{
		DSC_RUN_LOG_ERROR("MAX_CACHE_BLOCK_COUNT[%d] value invalid", nMaxCacheBlockCount);
		return -1;
	}
	m_nMaxCacheBlockCount = (ACE_UINT32)nMaxCacheBlockCount;

	return 0;
}

CChannelProcessService::CCsConnectSessionHandler* CChannelProcessService::GetRegistPeerHandler(const ACE_UINT64 nMaxBlockID, const CDscString& strMaxBlockHash)
{
	for (ACE_UINT32 idx = 0; idx < m_arrCsConnectHandler.Size(); ++idx)
	{
		if ((m_arrCsConnectHandler[idx]->m_nRegistMaxBlockID == nMaxBlockID)
			&& (m_arrCsConnectHandler[idx]->m_strRegistMaxBlockHash == strMaxBlockHash))
		{
			return m_arrCsConnectHandler[idx];
		}
	}

	return nullptr;
}

CChannelProcessService::CCsConnectSessionHandler* CChannelProcessService::GetPeerHandler(const ACE_UINT16 nPeerID)
{
	for (ACE_UINT32 idx = 0; idx < m_arrCsConnectHandler.Size(); ++idx)
	{
		if (m_arrCsConnectHandler[idx]->m_nPeerID == nPeerID)
		{
			return m_arrCsConnectHandler[idx];
		}
	}

	return nullptr;
}

void CChannelProcessService::EraseHandler(const ACE_UINT16 nPeerID)
{
	for (ACE_UINT32 i = 0; i < m_arrCsConnectHandler.Size(); ++i)
	{
		if (m_arrCsConnectHandler[i]->m_nPeerID == nPeerID)
		{
			m_arrCsConnectHandler.Erase(m_arrCsConnectHandler[i]);
			m_arrCsConnectHandler[i]->m_nIndex = CDscTypeArray<CCsConnectSessionHandler>::EN_INVALID_INDEX_ID;

			break;
		}
	}
}


ACE_INT32 CChannelProcessService::PersistentBlockQueueHeader(void)
{
	ACE_INT32 nReturnCode = 0;
	CLocalBlockInfo* pBlockInfo = m_queueBlockInfo.Front();
	
	//1. 将已经成功分发的区块信息永久存储
	//1.1 写version-table日志
	if (DSC_UNLIKELY(m_wsVersionTable.SaveToLog()))
	{
		//TODO: 向网管告警
		DSC_RUN_LOG_ERROR("write-set-version-table save-to-log failed.");

		nReturnCode = -1;
	}
	else
	{
		//1.2 记录config日志
		m_pCfgLog->m_nBlockID = pBlockInfo->m_nBlockID;
		m_pCfgLog->m_nLastAlocWsID = pBlockInfo->m_nLastAlocWsID;
		::memcpy(m_pCfgLog->m_blockHash, pBlockInfo->m_blockHash, VBH_BLOCK_DIGEST_LENGTH);

		//2 置标志，存盘，清标志
		//==============开始操作
		m_pConfig->m_nModifyStorageState = EN_BEGIN_MODIFY_STORAGE;
		//2.1 version-table的持久化
		if (DSC_UNLIKELY(m_wsVersionTable.Persistence()))
		{
			//TODO: 向网管告警
			DSC_RUN_LOG_ERROR("write-set-version table persistence failed.");
			m_pConfig->m_nModifyStorageState = EN_END_MODIFY_STORAGE; //==============结束操作

			nReturnCode = -1;
		}
		else
		{
			//2.2 config的持久化
			m_pConfig->m_nBlockID = m_pCfgLog->m_nBlockID;
			m_pConfig->m_nLastAlocWsID = m_pCfgLog->m_nLastAlocWsID;
			::memcpy(m_pConfig->m_blockHash, m_pCfgLog->m_blockHash, VBH_BLOCK_DIGEST_LENGTH);
			m_pConfig->m_nModifyStorageState = EN_END_MODIFY_STORAGE; //==============结束操作

			//3. 提交事务，进行提交清理
			m_wsVersionTable.CommitteTransaction(); 

			//4. 弹出已处理的区块, 并释放区块内存
			m_queueBlockInfo.PopFront();
			--m_nQueueBlockNum;
			DSC_THREAD_TYPE_DELETE(pBlockInfo);
		}
	}

	return nReturnCode;
}

void CChannelProcessService::RollbackPartBlock(void)
{
	//1. write-set-version表回滚
	m_wsVersionTable.RollbackUnpackCache();

	ACE_ASSERT(m_pCurBlockInfo);

	//2. 重新初始化当前打包区块的缓冲区
	DSC_THREAD_TYPE_DELETE(m_pCurBlockInfo);
	m_pCurBlockInfo = DSC_THREAD_TYPE_NEW(CLocalBlockInfo) CLocalBlockInfo(m_nPackMaxBlockSizeValue);

	if (m_nQueueBlockNum) //有已经打包的区块了, 参数从最后1个打包好的块中取
	{
		CLocalBlockInfo* pTmpBlockInfo = m_queueBlockInfo.Back();

		m_pCurBlockInfo->m_nBlockID = pTmpBlockInfo->m_nBlockID + 1;
		m_pCurBlockInfo->m_nLastAlocWsID = pTmpBlockInfo->m_nLastAlocWsID;
	}
	else
	{
		m_pCurBlockInfo->m_nBlockID = m_pConfig->m_nBlockID + 1;
		m_pCurBlockInfo->m_nLastAlocWsID = m_pConfig->m_nLastAlocWsID;
	}

	m_blockEncoder.InitSetEncodeBuffer(m_pCurBlockInfo->m_pBlockDataBuf);
}


ACE_INT32 CChannelProcessService::OnHtsMsg(VBH::CSlaveSyncVersionTableCsCpsRsp& rSyncVersionTable, CMcpHandler* pMcpHandler)
{
	VBH_MESSAGE_ENTER_TRACE(CSlaveChannelProcessService, VBH::CSlaveSyncVersionTableCsCpsRsp);

	if ((m_pConfig->m_nBlockID + 1) == rSyncVersionTable.m_nBlockID)
	{
		//1. 校验version的流水号是否正确
		bool bIsOk = true;
		VBH_CLS::CVersionTableItem versionTableItem;

		for (auto& it : rSyncVersionTable.m_lstKv)
		{
			if (it.m_nVersion)
			{
				if (m_wsVersionTable.Read(versionTableItem, it.m_nAllocatedID))
				{
					DSC_RUN_LOG_ERROR("read write-set-version-table failed, aloc-id:%#llu", it.m_nAllocatedID);
					bIsOk = false;
					break;
				}
				if ((versionTableItem.m_nVersion + 1) != it.m_nVersion)
				{
					DSC_RUN_LOG_ERROR("cps current version:%u, sync-verion:%u, write-set-id:%llu", versionTableItem.m_nVersion, it.m_nVersion, it.m_nAllocatedID);
					bIsOk = false;
					break;
				}
			}
		}

		//2. 校验过程未出错，则将所有的version号存入
		if (bIsOk)
		{
			ACE_UINT64 nMaxAlocWsID = 0;

			for (auto& it : rSyncVersionTable.m_lstKv)
			{
				if (it.m_nAllocatedID > nMaxAlocWsID)
				{
					nMaxAlocWsID = it.m_nAllocatedID;
				}

				versionTableItem.m_nVersion = it.m_nVersion;

				if (versionTableItem.m_nVersion) //
				{
					if (m_wsVersionTable.Update(it.m_nAllocatedID, versionTableItem))
					{
						//todo: 通过网管告警
						DSC_RUN_LOG_ERROR("update write-set-version-table failed, key-aloc-id:%#llX, channnel-id:%d", it.m_nAllocatedID, m_nChannelID);
						bIsOk = false; //失败标记
						break;
					}
				}
				else //version == 0，是新增的情况
				{
					if (m_wsVersionTable.Append(it.m_nAllocatedID, versionTableItem))
					{
						//todo: 通过网管告警
						DSC_RUN_LOG_ERROR("append write-set-version-table failed, key-aloc-id:%#llX, channnel-id:%d", it.m_nAllocatedID, m_nChannelID);
						bIsOk = false; //失败标记
						break;
					}
				}
			}

			if (bIsOk)
			{
				//3. 打包变更
				m_wsVersionTable.PackModify();

				//4. 开始存储流程 //存储过程中失败，则通过再次请求来触发存储操作
				//4.1 写version-table日志
				if (DSC_UNLIKELY(m_wsVersionTable.SaveToLog()))
				{
					//TODO: 向网管告警
					DSC_RUN_LOG_ERROR("write-set-version-table save-to-log failed.");
				}
				else
				{
					//4.2 记录config日志
					m_pCfgLog->m_nBlockID = rSyncVersionTable.m_nBlockID;
					m_pCfgLog->m_nLastAlocWsID = nMaxAlocWsID;
					::memcpy(m_pCfgLog->m_blockHash, rSyncVersionTable.m_strBlockHash.data(), VBH_BLOCK_DIGEST_LENGTH);

					//5. 置标志，存盘，清标志
					//==============开始操作
					m_pConfig->m_nModifyStorageState = EN_BEGIN_MODIFY_STORAGE;
					//5.1 version-table的持久化
					if (DSC_UNLIKELY(m_wsVersionTable.Persistence()))
					{
						//TODO: 向网管告警
						DSC_RUN_LOG_ERROR("write-set-version table persistence failed.");
						m_pConfig->m_nModifyStorageState = EN_END_MODIFY_STORAGE; //==============结束操作
					}
					else
					{
						//5.2 config的持久化
						m_pConfig->m_nBlockID = m_pCfgLog->m_nBlockID;
						m_pConfig->m_nLastAlocWsID = m_pCfgLog->m_nLastAlocWsID;
						::memcpy(m_pConfig->m_blockHash, m_pCfgLog->m_blockHash, VBH_BLOCK_DIGEST_LENGTH);
						m_pConfig->m_nModifyStorageState = EN_END_MODIFY_STORAGE; //==============结束操作

						//6. 提交事务，进行提交清理
						m_wsVersionTable.CommitteTransaction();
					}
				}
			}
		}
		else
		{
			//TODO: 序列号冲突, 向网管告警，//肯定是peer状态错了
		}
	}
	else //block-id不匹配
	{
		DSC_RUN_LOG_ERROR("recv CSlaveSyncVersionTableCsCpsRsp, block-id:%lld, but expect block-id:%lld", rSyncVersionTable.m_nBlockID, m_pConfig->m_nBlockID + 1);
	}

	VBH_MESSAGE_LEAVE_TRACE(CSlaveChannelProcessService, VBH::CSlaveSyncVersionTableCsCpsRsp);

	return 0;
}