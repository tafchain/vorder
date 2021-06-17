#ifndef MASTER_ORDER_CHANNEL_PROCESS_SERVICE_H_789643352187976874354134968764
#define MASTER_ORDER_CHANNEL_PROCESS_SERVICE_H_789643352187976874354134968764

#include "ace/Shared_Memory_MM.h"

#include "dsc/protocol/mcp/mcp_server_handler.h"
#include "dsc/protocol/hts/dsc_hts_service.h"
#include "dsc/protocol/mcp/mcp_asynch_acceptor.h"
#include "dsc/container/bare_hash_map.h"
#include "dsc/mem_mng/dsc_stl_type.h"

#include "vbh_comm/comm_msg_def/vbh_comm_msg_es_def.h"
#include "vbh_comm/comm_msg_def/vbh_comm_msg_cps_cs_def.h"
#include "vbh_comm/vbh_encrypt_lib.h"

#include "vbh_server_comm/vbh_block_codec.h"
#include "vbh_server_comm/merkel_tree/merkel_tree.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_update_table.h"

#include "ord_cps/order_base_channel_process_service.h"
#include "ord_cps/i_order_channel_process_service.h"
#include "ord_cpas/i_order_channel_process_agent_service.h"
#include "vbh_comm/comm_msg_def/vbh_comm_msg_cps_cpas_def.h"

//TODO:在区块分发成功后，应该再增加1个确认消息从order到endorser
//order逐块给endorser发通知，endorser到客户端是逐条
class PLUGIN_EXPORT CChannelProcessService final : public CBaseChannelProcessService,public IOrderChannelProcessService
{

public:
	enum
	{
		EN_SERVICE_TYPE = VBH::EN_ORDER_CHANNEL_PROCESS_SERVICE_TYPE
	};

private:
	enum
	{
		EN_HASH_MAP_BITES = 16,
		EN_DISTRIBUTE_TIMEOUT_VALUE = 30,  //定时重发周期
		EN_WAIT_PEER_REGIST_TIMEOUT_VALUE = 60, //等待peer注册定时器定时周期
		EN_ORDER_SYNC_TIMEOUT_VALUE = 5, //order从peer进行同步的超时时间
		EN_FOLLOWER_ORDER_SYNC_TIMEOUT_VALUE = 10, //order从peer进行同步的超时时间
		EN_REPERSIST_BLOCK_QUEUE_HEADER_TIMEOUT_VALUE = 60 //持久化失败后，重新执行持久化的时间间隔
	};

	enum CpsState//cps服务的主状态
	{
		EN_FOLLOWER_STATE = 1,
		EN_MASTER_WAIT_STATE,
		EN_MASTER_START_STATE,
		EN_MASTER_SYNC_STATE,
		EN_MASTER_NORMAL_STATE,
	};
	enum ActionID//
	{
		CREAT_INFORMATION_ID = 1, //
	};

private:
	//记录每个peer连接的session
	class CCsConnectSessionHandler : public CMcpServerHandler, public CDscServiceTimerHandler // 成为 CMcpServerHandler 的子类
	{
	public:
		CCsConnectSessionHandler(CChannelProcessService& rService, ACE_HANDLE handle, const ACE_INT32 nHandleID);

	public:
		void OnTimer(void) final;

	public:
		bool m_bRecvDistBlockRsp = false; //收到 anchor 发送的分发block应答

		ACE_UINT16 m_nPeerID = 0; //committer 的peerID， //1个cbs只处理1个channel, 1个as也只处理1个channel，所以，只有1对1关系
		ACE_UINT16 m_nCasPort = 0; //committer代理的端口号
		ACE_UINT64 m_nRegistMaxBlockID; //注册时，peer的最高区块高度

		CDscString m_strCasIpAddr; //committer代理的IP地址
		CDscString m_strRegistMaxBlockHash; //注册时，最高区块的hash值

	public:
		ACE_UINT32 m_nIndex = CDscTypeArray<CCsConnectSessionHandler>::EN_INVALID_INDEX_ID; //使用 CDscTypeArray 容器必须具备的接口

	protected:
		CChannelProcessService& m_rCps;
	};

	using CXcsConnectSessionHandlerPtr = CCsConnectSessionHandler *;

	//等待peer注册定时器
	class CWaitPeerRegistSession : public CDscServiceTimerHandler
	{
	public:
		CWaitPeerRegistSession(CChannelProcessService& rCps);

	public:
		void OnTimer(void) final;

	protected:
		CChannelProcessService& m_rCps;
	};

	//Order同步定时器
	class COrderSyncSession : public CDscServiceTimerHandler
	{
	public:
		COrderSyncSession(CChannelProcessService& rCps);

	public:
		void OnTimer(void) final;

	public:
		ACE_UINT64 m_nKafkaBlockID; //达到kafka共识时的 最高区块高度
		CDscString m_strKafkaBlockHash; //达到kafka共识时的 最高区块hash

	protected:
		CChannelProcessService& m_rCps;
	};

	//Order同步定时器
	class CFollowerOrderSyncSession : public CDscServiceTimerHandler
	{
	public:
		CFollowerOrderSyncSession(CChannelProcessService& rCps);

	public:
		void OnTimer(void) final;

	protected:
		CChannelProcessService& m_rCps;
	};

	//用于定时打包的定时器
	class CPackTimerHandler : public CDscServiceTimerHandler
	{
	public:
		CPackTimerHandler(CChannelProcessService& rCps);

	public:
		void OnTimer(void) final;

	protected:
		CChannelProcessService& m_rCps;
	};

	//区块发送应答达成kafaka共识后，存储version信息失败而启动的 定时存储version信息的定时器
	class CRepersistBlockQueueHeaderSession : public CDscServiceTimerHandler
	{
	public:
		CRepersistBlockQueueHeaderSession(CChannelProcessService& rCps);

	public:
		void OnTimer(void) final;

	protected:
		CChannelProcessService& m_rCps;
	};

	//打包区块时的信息
	class CLocalBlockInfo
	{
	public:
		CLocalBlockInfo(const ACE_UINT32 nBlockBufSize);

		~CLocalBlockInfo();

		//重置缓冲区大小 //只能扩大，不能缩小
		void Resize(ACE_UINT32 nNewBufSize);

	public:
		ACE_UINT32 m_nBlockBufSize; //区块数据缓冲区大小
		ACE_UINT32 m_nBlockDataLen; //区块数据的实际大小
		ACE_UINT64 m_nBlockID; //当前区块ID
		ACE_UINT64 m_nLastAlocWsID; //当前区块中 第1个 新注册用户/新增information的 系统ID

		char* m_pBlockDataBuf; //存放区块数据的缓冲区，开辟的大小为 m_nMaxBlockSize
		char m_blockHash[VBH_BLOCK_DIGEST_LENGTH]; //当前块的hash

	public: //使用CDscUnboundQueue时，必须具有的成员
		CLocalBlockInfo* m_pNext = nullptr;
	};

	class CPeerInfo
	{
	public:
		ACE_UINT16 m_nCasPort; //committer代理的端口号
		ACE_UINT16 m_nPeerID;
		ACE_UINT32 m_nHandleID;
		ACE_UINT64 m_nRegistMaxBlockID; //peer当前最高区块高度
		CDscString m_strRegistMaxBlockHash; //最高区块的Hash值
		CDscString m_strCasIpAddr; //committer代理的IP地址

	public:
		bool operator< (const CPeerInfo& rPeerInfo);
	};

public:
	CChannelProcessService(const CDscString& strIpAddr, const ACE_INT32 nPort, const ACE_UINT32 nChannelID);

public:
	ACE_INT32 OnInit(void) final;
	ACE_INT32 OnExit(void) final;

public:
	virtual void ChangeCpsToMasterState(void);
	virtual void ChangeCpsToFollowerState(void);

	//同步定时器超时
	void OnSyncTimeOut(COrderSyncSession* pSession);
	void OnSyncTimeOut(CFollowerOrderSyncSession* pSession);

	//定时打包区块发送区块的函数
	void OnTimePackBlock(void);

	void SendQueryMaxBlockInfoMsg(const ACE_UINT32 nHandleID);
	void GetTheMostBlockHeightInfo(ACE_UINT32& nNormalFront, ACE_UINT32& nNormalRear, ACE_UINT32& nNormalPeerCount);
	void SendSyncBlockNotifyMsg(const ACE_UINT32 nNormalFront, const ACE_UINT32 nNormalRear);

	//首次发送区块 调用的函数
	void DistributeBlock(void);
	void SetChannelprocessAgentService(IOrderChannelprocessAgentService* pOrderChannelprocessAgentService);
	//超时重发区块
	void OnTimeDistributeBlock(CCsConnectSessionHandler* pHandler);

	//超时重新执行持久化操作 //作用对象：缓存的区块队列的队头
	void OnTimeRepersistence(CRepersistBlockQueueHeaderSession* pSession);

	ACE_INT32 ProposalTransaction(VBH::CSubmitProposalTransactionTasCpsReq& rSubmitTransReq, VBH::CSubmitProposalTransactionCpsTasRsp& rsp, VBH::CProposeTransaction& proposeTransaction);
	ACE_INT32 CheckOrderIsNormal(void);
	ACE_INT32 CheckWsItemVersion(VBH::CSimpleVector<VBH::CVbhWriteSetItem>& vecWsItem);


protected:
	BEGIN_HTS_MESSAGE_BIND
	BIND_HTS_MESSAGE(VBH::CRegistCsCpsReq)
	BIND_HTS_MESSAGE(VBH::CMasterSyncVersionTableCsCpsRsp)

	BIND_HTS_MESSAGE(VBH::CSubmitRegistUserTransactionTasCpsReq)
	BIND_HTS_MESSAGE(VBH::CSubmitProposalTransactionTasCpsReq)

	BIND_HTS_MESSAGE(VBH::CDistributeBlockXcsCpsRsp)
	BIND_HTS_MESSAGE(VBH::CQuerySyncSourcePeerCsCpsReq)

	BIND_HTS_MESSAGE(VBH::CRaftHeartBeatCpsCpas)
	BIND_HTS_MESSAGE(VBH::CRaftVoteCpsCpasReq)
	BIND_HTS_MESSAGE(VBH::CQueryMaxBlockInfoCsCpsRsp)
	BIND_HTS_MESSAGE(VBH::CRaftCpasCpsConnectSucess)
	BIND_HTS_MESSAGE(VBH::CQueryLeaderCpsTasCpsReq)
	END_HTS_MESSAGE_BIND

public:
	ACE_INT32 OnHtsMsg(VBH::CRegistCsCpsReq& rRegistReq, CMcpHandler* pMcpHandler);
	ACE_INT32 OnHtsMsg(VBH::CMasterSyncVersionTableCsCpsRsp& rRegrSyncVersionTableisterAs, CMcpHandler* pMcpHandler);

	ACE_INT32 OnHtsMsg(VBH::CSubmitRegistUserTransactionTasCpsReq& rSubmitTransReq, CMcpHandler* pMcpHandler);
	ACE_INT32 OnHtsMsg(VBH::CSubmitProposalTransactionTasCpsReq& rSubmitTransReq, CMcpHandler* pMcpHandler);

	ACE_INT32 OnHtsMsg(VBH::CDistributeBlockXcsCpsRsp& rDistBlockRsp, CMcpHandler* pMcpHandler);
	ACE_INT32 OnHtsMsg(VBH::CQuerySyncSourcePeerCsCpsReq& rQuerySyncSrcReq, CMcpHandler* pMcpHandler);
	ACE_INT32 OnHtsMsg(VBH::CSlaveSyncVersionTableCsCpsRsp& rSyncVersionTable, CMcpHandler* pMcpHandler);

	ACE_INT32 OnHtsMsg(VBH::CRaftHeartBeatCpsCpas& rRaftHeartBeatCpsCpas, CMcpHandler* pMcpHandler);
	ACE_INT32 OnHtsMsg(VBH::CRaftVoteCpsCpasReq& rRaftVoteCpsCpasReq, CMcpHandler* pMcpHandler);
	ACE_INT32 OnHtsMsg(VBH::CQueryMaxBlockInfoCsCpsRsp& rQueryMaxBlockInfoCsCpsRsp, CMcpHandler* pMcpHandler);
	ACE_INT32 OnHtsMsg(VBH::CRaftCpasCpsConnectSucess& rRaftCpasCpsConnectSucess, CMcpHandler* pMcpHandler);
	ACE_INT32 OnHtsMsg(VBH::CQueryLeaderCpsTasCpsReq& rQueryLeaderCpsTasCpsReq, CMcpHandler* pMcpHandler);

protected:
	CMcpServerHandler* AllocMcpHandler(ACE_HANDLE handle) final;

	void OnNetworkError(CMcpHandler* pMcpHandler) final;

private:
	//读取需要的数据库配置信息 //仅master需要的信息
	ACE_INT32 GetVbhMasterOrderProfile(void);

	//获取1个注册时的BestPeer
	CCsConnectSessionHandler* GetRegistPeerHandler(const ACE_UINT64 nMaxBlockID, const CDscString& strMaxBlockHash);

	//获取1个peer的连接
	CCsConnectSessionHandler* GetPeerHandler(const ACE_UINT16 nPeerID);

	void EraseHandler(const ACE_UINT16 m_nPeerID);

	//对区块列表的头元素进行持久化 //持久化包括写version表，修改config记录等 //成功，则将头元素释放，并返回0， 失败返回-1
	ACE_INT32 PersistentBlockQueueHeader(void);

	//回滚打包到一半的区块，在打包过程中遇到意外时，调用该函数
	void RollbackPartBlock(void);



private:
	CpsState m_nCpsState = EN_FOLLOWER_STATE; //service的主状态
	CRepersistBlockQueueHeaderSession m_repersistBlockQueueHeaderSession; //定时重新持久化的定时器句柄
	ACE_UINT32 m_nSeed = 0;
	ACE_UINT32 m_nKafkaValue; //kafka共识数值

	CDscTypeArray<CCsConnectSessionHandler> m_arrCsConnectHandler; //已和cbs建立连接的xcs地址列表
	dsc_vector_type(CPeerInfo) m_vecPeerInfo;
	CWaitPeerRegistSession m_waitPeerRegistSession; //等待peer注册定时器
	COrderSyncSession m_orderSyncSession; //order同步定时器
	COrderSyncSession m_followerOrderSyncSession; //order同步定时器

	ACE_UINT32 m_nPackTimeoutValue; //打包的定时时间
	ACE_UINT32 m_nPackMaxBlockSizeValue; //打包的定量大小
	ACE_UINT32 m_nBlockBufInitialSize; //区块缓冲区的初始大小，区块缓冲区在使用过程中会根据被装进去的最后1个事务来resize大小
	CPackTimerHandler m_packTimerHandler; //定时打包 timer 句柄

	ACE_UINT32 m_nMaxCacheBlockCount;
	ACE_UINT32 m_nQueueBlockNum = 0; //已经缓存的区块个数 //m_queueBlockInfo长度
	CDscUnboundQueue<CLocalBlockInfo> m_queueBlockInfo; //打包好，缓存待发送的区块
	CLocalBlockInfo* m_pCurBlockInfo = nullptr; //当前正在打包的区块，所有收到的事务打包该区块中
	VBH::CBlockEncoder m_blockEncoder; //区块打包器
	ACE_UINT32 m_nDistBlockRspPeerCount = 0;//正在分发的区块收到的应答个数
	ACE_INT32 m_nSelectCursorIdx = 0; //选中的handler下标
	bool m_nIsStartFollowerSysTimer = false;

private:
	IOrderChannelprocessAgentService* m_pOrderChannelprocessAgentService = nullptr;
};

#include "ord_cps/order_channel_process_service.inl"

#endif
