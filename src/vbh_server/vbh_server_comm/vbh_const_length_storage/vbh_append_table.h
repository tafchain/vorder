#ifndef VBH_APPEND_H_5872349721682135623456
#define VBH_APPEND_H_5872349721682135623456

#include "ace/Shared_Memory_MM.h"

#include "dsc/mem_mng/dsc_stl_type.h"

#include "vbh_server_comm/vbh_server_comm_def_export.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_record_table.h"

class VBH_SERVER_COMM_DEF_EXPORT CVbhAppendTable : public CVbhRecordTable
{
private:
	class CAppendTableConfig
	{
	public:
		ACE_UINT64 m_nLastRecordID = 1; //系统中第一个可用ID //未分配的最小值
	};

	class CAppendTableLog
	{
	public:
		ACE_UINT64 m_nLastRecordID;
	};

public:
	// strPath为目录名, 路径名必须以 '/'结尾
	virtual ACE_INT32 Open(VBFS::CVbfs* pVbfs, const ACE_UINT32 nChannelID, const ACE_UINT32 nTableType) override;
	virtual void Close(void) override;

	template<typename RECORD_TYPE>
	ACE_INT32 Append(ACE_UINT64& nRecordID, RECORD_TYPE& rRecord);

	//弹出最后1个RrecodeID对应的记录
	void PopBack(const ACE_UINT64 nRecordID);

	//把变更保存到日志中，在ApplyModify之前被调用
	ACE_INT32 SaveToLog(void);
	ACE_INT32 Persistence(void);
	void CommitteTransaction(void);
	void RollbackCache(void);
	ACE_INT32 RollbackTransaction(void);
	ACE_INT32 RecoverFromLog(void);//启动时发现异常回滚

private:
	ACE_UINT64 m_nLastRecordID; //系统中第一个可用ID //未分配的最小值
	CPage* m_pAppendPage = nullptr;
	dsc_deque_type(CPage*) m_queueAppendPage; //append时存放新增数据（脏数据）的Page //m_bNewPage=false的page在m_mapPage中，其他不在
	ACE_Shared_Memory_MM m_shmClsCtrlInfo;
	CAppendTableConfig* m_pCfg = nullptr;

	CDscString m_strLogFilePath; //日志文件路径
	ACE_Shared_Memory_MM m_shmLog; //日志文件的共享内存对象
	CAppendTableLog* m_pLog = nullptr;//日志文件对应的共享内存缓冲区
};

#include "vbh_server_comm/vbh_const_length_storage/vbh_append_table.inl"

#endif
