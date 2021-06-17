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
		EN_BEGIN_MODIFY_STORAGE = 1, //开始变更存储
		EN_END_MODIFY_STORAGE = 0 //完成变更存储
	};

	//order-cps配置信息
	class CCpsConfig //尽量进行内存对齐
	{
	public:
		ACE_INT32 m_nModifyStorageState = EN_END_MODIFY_STORAGE; //是否正在修改存储; //在重启后检测该标志，是则需要从log中回滚

		//流水号是只要使用就变化，无论何种情况都不回滚
		ACE_UINT32 m_nSequenceNumber = 0; //增加用户时的流水号

		//序列号只有完全成功才分配出去，失败时要回滚
		ACE_UINT64 m_nBlockID = 0; //系统(当前channel)当前区块高度 //已分配数值 //系统的区块高度从1计数;
		ACE_UINT64 m_nLastAlocWsID = 0; //新注册用户/新增information 的系统ID(当前channel) //已分配数值 //系统ID从1开始计数

		//上一个block的hash
		char m_blockHash[VBH_BLOCK_DIGEST_LENGTH]; //当前区块的hash值
	};

	//order-cps配置信息对应的日志
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
	ACE_UINT32 m_nPeerCount; //系统中的节点数，需要在数据库中配置
	CDscString m_strIpAddr;
	ACE_INT32 m_nPort;
	CMcpAsynchAcceptor<CBaseChannelProcessService>* m_pAcceptor = nullptr;

	ACE_Shared_Memory_MM m_shmCpsCfg;
	CCpsConfig* m_pConfig = nullptr; //配置
	ACE_Shared_Memory_MM m_shmCpsLog;
	CCpsConfigLog* m_pCfgLog = nullptr; //config的日志

	VBFS::CVbfs m_vbfs;
	CVbhMultiUpdateTable m_wsVersionTable; // write-set的version-table
};

#endif
