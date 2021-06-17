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

//TODO:������ַ��ɹ���Ӧ��������1��ȷ����Ϣ��order��endorser
//order����endorser��֪ͨ��endorser���ͻ���������
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
		EN_DISTRIBUTE_TIMEOUT_VALUE = 30,  //��ʱ�ط�����
		EN_WAIT_PEER_REGIST_TIMEOUT_VALUE = 60, //�ȴ�peerע�ᶨʱ����ʱ����
		EN_ORDER_SYNC_TIMEOUT_VALUE = 5, //order��peer����ͬ���ĳ�ʱʱ��
		EN_FOLLOWER_ORDER_SYNC_TIMEOUT_VALUE = 10, //order��peer����ͬ���ĳ�ʱʱ��
		EN_REPERSIST_BLOCK_QUEUE_HEADER_TIMEOUT_VALUE = 60 //�־û�ʧ�ܺ�����ִ�г־û���ʱ����
	};

	enum CpsState//cps�������״̬
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
	//��¼ÿ��peer���ӵ�session
	class CCsConnectSessionHandler : public CMcpServerHandler, public CDscServiceTimerHandler // ��Ϊ CMcpServerHandler ������
	{
	public:
		CCsConnectSessionHandler(CChannelProcessService& rService, ACE_HANDLE handle, const ACE_INT32 nHandleID);

	public:
		void OnTimer(void) final;

	public:
		bool m_bRecvDistBlockRsp = false; //�յ� anchor ���͵ķַ�blockӦ��

		ACE_UINT16 m_nPeerID = 0; //committer ��peerID�� //1��cbsֻ����1��channel, 1��asҲֻ����1��channel�����ԣ�ֻ��1��1��ϵ
		ACE_UINT16 m_nCasPort = 0; //committer����Ķ˿ں�
		ACE_UINT64 m_nRegistMaxBlockID; //ע��ʱ��peer���������߶�

		CDscString m_strCasIpAddr; //committer�����IP��ַ
		CDscString m_strRegistMaxBlockHash; //ע��ʱ����������hashֵ

	public:
		ACE_UINT32 m_nIndex = CDscTypeArray<CCsConnectSessionHandler>::EN_INVALID_INDEX_ID; //ʹ�� CDscTypeArray ��������߱��Ľӿ�

	protected:
		CChannelProcessService& m_rCps;
	};

	using CXcsConnectSessionHandlerPtr = CCsConnectSessionHandler *;

	//�ȴ�peerע�ᶨʱ��
	class CWaitPeerRegistSession : public CDscServiceTimerHandler
	{
	public:
		CWaitPeerRegistSession(CChannelProcessService& rCps);

	public:
		void OnTimer(void) final;

	protected:
		CChannelProcessService& m_rCps;
	};

	//Orderͬ����ʱ��
	class COrderSyncSession : public CDscServiceTimerHandler
	{
	public:
		COrderSyncSession(CChannelProcessService& rCps);

	public:
		void OnTimer(void) final;

	public:
		ACE_UINT64 m_nKafkaBlockID; //�ﵽkafka��ʶʱ�� �������߶�
		CDscString m_strKafkaBlockHash; //�ﵽkafka��ʶʱ�� �������hash

	protected:
		CChannelProcessService& m_rCps;
	};

	//Orderͬ����ʱ��
	class CFollowerOrderSyncSession : public CDscServiceTimerHandler
	{
	public:
		CFollowerOrderSyncSession(CChannelProcessService& rCps);

	public:
		void OnTimer(void) final;

	protected:
		CChannelProcessService& m_rCps;
	};

	//���ڶ�ʱ����Ķ�ʱ��
	class CPackTimerHandler : public CDscServiceTimerHandler
	{
	public:
		CPackTimerHandler(CChannelProcessService& rCps);

	public:
		void OnTimer(void) final;

	protected:
		CChannelProcessService& m_rCps;
	};

	//���鷢��Ӧ����kafaka��ʶ�󣬴洢version��Ϣʧ�ܶ������� ��ʱ�洢version��Ϣ�Ķ�ʱ��
	class CRepersistBlockQueueHeaderSession : public CDscServiceTimerHandler
	{
	public:
		CRepersistBlockQueueHeaderSession(CChannelProcessService& rCps);

	public:
		void OnTimer(void) final;

	protected:
		CChannelProcessService& m_rCps;
	};

	//�������ʱ����Ϣ
	class CLocalBlockInfo
	{
	public:
		CLocalBlockInfo(const ACE_UINT32 nBlockBufSize);

		~CLocalBlockInfo();

		//���û�������С //ֻ�����󣬲�����С
		void Resize(ACE_UINT32 nNewBufSize);

	public:
		ACE_UINT32 m_nBlockBufSize; //�������ݻ�������С
		ACE_UINT32 m_nBlockDataLen; //�������ݵ�ʵ�ʴ�С
		ACE_UINT64 m_nBlockID; //��ǰ����ID
		ACE_UINT64 m_nLastAlocWsID; //��ǰ������ ��1�� ��ע���û�/����information�� ϵͳID

		char* m_pBlockDataBuf; //����������ݵĻ����������ٵĴ�СΪ m_nMaxBlockSize
		char m_blockHash[VBH_BLOCK_DIGEST_LENGTH]; //��ǰ���hash

	public: //ʹ��CDscUnboundQueueʱ��������еĳ�Ա
		CLocalBlockInfo* m_pNext = nullptr;
	};

	class CPeerInfo
	{
	public:
		ACE_UINT16 m_nCasPort; //committer����Ķ˿ں�
		ACE_UINT16 m_nPeerID;
		ACE_UINT32 m_nHandleID;
		ACE_UINT64 m_nRegistMaxBlockID; //peer��ǰ�������߶�
		CDscString m_strRegistMaxBlockHash; //��������Hashֵ
		CDscString m_strCasIpAddr; //committer�����IP��ַ

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

	//ͬ����ʱ����ʱ
	void OnSyncTimeOut(COrderSyncSession* pSession);
	void OnSyncTimeOut(CFollowerOrderSyncSession* pSession);

	//��ʱ������鷢������ĺ���
	void OnTimePackBlock(void);

	void SendQueryMaxBlockInfoMsg(const ACE_UINT32 nHandleID);
	void GetTheMostBlockHeightInfo(ACE_UINT32& nNormalFront, ACE_UINT32& nNormalRear, ACE_UINT32& nNormalPeerCount);
	void SendSyncBlockNotifyMsg(const ACE_UINT32 nNormalFront, const ACE_UINT32 nNormalRear);

	//�״η������� ���õĺ���
	void DistributeBlock(void);
	void SetChannelprocessAgentService(IOrderChannelprocessAgentService* pOrderChannelprocessAgentService);
	//��ʱ�ط�����
	void OnTimeDistributeBlock(CCsConnectSessionHandler* pHandler);

	//��ʱ����ִ�г־û����� //���ö��󣺻����������еĶ�ͷ
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
	//��ȡ��Ҫ�����ݿ�������Ϣ //��master��Ҫ����Ϣ
	ACE_INT32 GetVbhMasterOrderProfile(void);

	//��ȡ1��ע��ʱ��BestPeer
	CCsConnectSessionHandler* GetRegistPeerHandler(const ACE_UINT64 nMaxBlockID, const CDscString& strMaxBlockHash);

	//��ȡ1��peer������
	CCsConnectSessionHandler* GetPeerHandler(const ACE_UINT16 nPeerID);

	void EraseHandler(const ACE_UINT16 m_nPeerID);

	//�������б��ͷԪ�ؽ��г־û� //�־û�����дversion���޸�config��¼�� //�ɹ�����ͷԪ���ͷţ�������0�� ʧ�ܷ���-1
	ACE_INT32 PersistentBlockQueueHeader(void);

	//�ع������һ������飬�ڴ����������������ʱ�����øú���
	void RollbackPartBlock(void);



private:
	CpsState m_nCpsState = EN_FOLLOWER_STATE; //service����״̬
	CRepersistBlockQueueHeaderSession m_repersistBlockQueueHeaderSession; //��ʱ���³־û��Ķ�ʱ�����
	ACE_UINT32 m_nSeed = 0;
	ACE_UINT32 m_nKafkaValue; //kafka��ʶ��ֵ

	CDscTypeArray<CCsConnectSessionHandler> m_arrCsConnectHandler; //�Ѻ�cbs�������ӵ�xcs��ַ�б�
	dsc_vector_type(CPeerInfo) m_vecPeerInfo;
	CWaitPeerRegistSession m_waitPeerRegistSession; //�ȴ�peerע�ᶨʱ��
	COrderSyncSession m_orderSyncSession; //orderͬ����ʱ��
	COrderSyncSession m_followerOrderSyncSession; //orderͬ����ʱ��

	ACE_UINT32 m_nPackTimeoutValue; //����Ķ�ʱʱ��
	ACE_UINT32 m_nPackMaxBlockSizeValue; //����Ķ�����С
	ACE_UINT32 m_nBlockBufInitialSize; //���黺�����ĳ�ʼ��С�����黺������ʹ�ù����л���ݱ�װ��ȥ�����1��������resize��С
	CPackTimerHandler m_packTimerHandler; //��ʱ��� timer ���

	ACE_UINT32 m_nMaxCacheBlockCount;
	ACE_UINT32 m_nQueueBlockNum = 0; //�Ѿ������������� //m_queueBlockInfo����
	CDscUnboundQueue<CLocalBlockInfo> m_queueBlockInfo; //����ã���������͵�����
	CLocalBlockInfo* m_pCurBlockInfo = nullptr; //��ǰ���ڴ�������飬�����յ�����������������
	VBH::CBlockEncoder m_blockEncoder; //��������
	ACE_UINT32 m_nDistBlockRspPeerCount = 0;//���ڷַ��������յ���Ӧ�����
	ACE_INT32 m_nSelectCursorIdx = 0; //ѡ�е�handler�±�
	bool m_nIsStartFollowerSysTimer = false;

private:
	IOrderChannelprocessAgentService* m_pOrderChannelprocessAgentService = nullptr;
};

#include "ord_cps/order_channel_process_service.inl"

#endif
