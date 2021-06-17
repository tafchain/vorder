#ifndef ORDER_CHANNEL_PROCESS_SERVICE_H_D9E50DA498DD11E9882760F18A3A20D1
#define ORDER_CHANNEL_PROCESS_SERVICE_H_D9E50DA498DD11E9882760F18A3A20D1

#include "ace/Shared_Memory_MM.h"

#include "dsc/protocol/mcp/mcp_server_handler.h"
#include "dsc/protocol/hts/dsc_hts_service.h"
#include "dsc/protocol/mcp/mcp_asynch_acceptor.h"
#include "dsc/container/bare_hash_map.h"
#include "dsc/mem_mng/dsc_stl_type.h"
#include "dsc/mem_mng/dsc_allocator.h"

#include "vbh_comm/vbh_encrypt_lib.h"

#include "vbh_server_comm/vbh_block_codec.h"
#include "vbh_server_comm/merkel_tree/merkel_tree.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_multi_update_table.h"
#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs.h"

#define DEF_LAST_WRITE_SET_ALLOCATED_ID 0XFFFFFFFFFFFFFFFF

class PLUGIN_EXPORT CBaseChannelProcessService : public CDscHtsServerService 
{
protected:
	enum EnModifyStorage
	{
		EN_BEGIN_MODIFY_STORAGE = 1, //��ʼ����洢
		EN_END_MODIFY_STORAGE = 0 //��ɱ���洢
	};

	//order-cps������Ϣ
	class CCpsConfig //���������ڴ����
	{
	public:
		ACE_INT32 m_nModifyStorageState = EN_END_MODIFY_STORAGE; //�Ƿ������޸Ĵ洢; //����������ñ�־��������Ҫ��log�лع�

		//��ˮ����ֻҪʹ�þͱ仯�����ۺ�����������ع�
		ACE_UINT32 m_nSequenceNumber = 0; //�����û�ʱ����ˮ��

		//���к�ֻ����ȫ�ɹ��ŷ����ȥ��ʧ��ʱҪ�ع�
		ACE_UINT64 m_nBlockID = 0; //ϵͳ(��ǰchannel)��ǰ����߶� //�ѷ�����ֵ //ϵͳ������߶ȴ�1����;
		ACE_UINT64 m_nLastAlocWsID = 0; //��ע���û�/����information ��ϵͳID(��ǰchannel) //�ѷ�����ֵ //ϵͳID��1��ʼ����

		//��һ��block��hash
		char m_blockHash[VBH_BLOCK_DIGEST_LENGTH]; //��ǰ�����hashֵ
	};

	//order-cps������Ϣ��Ӧ����־
	class CCpsConfigLog
	{
	public:
		ACE_UINT64 m_nBlockID;
		ACE_UINT64 m_nLastAlocWsID;

		char m_blockHash[VBH_BLOCK_DIGEST_LENGTH];
	};

public:
	CBaseChannelProcessService(const CDscString& strIpAddr, const ACE_INT32 nPort, const ACE_UINT32 nChannelID);

public:
	virtual ACE_INT32 OnInit(void) override;
	virtual ACE_INT32 OnExit(void) override;

protected:
	ACE_UINT16 m_nOrderID;
	ACE_UINT32 m_nChannelID;
	ACE_UINT32 m_nPeerCount; //ϵͳ�еĽڵ�������Ҫ�����ݿ�������
	CDscString m_strIpAddr;
	ACE_INT32 m_nPort;
	CMcpAsynchAcceptor<CBaseChannelProcessService>* m_pAcceptor = nullptr;

	ACE_Shared_Memory_MM m_shmCpsCfg;
	CCpsConfig* m_pConfig = nullptr; //����
	ACE_Shared_Memory_MM m_shmCpsLog;
	CCpsConfigLog* m_pCfgLog = nullptr; //config����־

	VBFS::CVbfs m_vbfs;
	CVbhMultiUpdateTable m_wsVersionTable; // write-set��version-table
};

#endif
