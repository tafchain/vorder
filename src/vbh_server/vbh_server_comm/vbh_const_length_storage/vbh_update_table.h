#ifndef VBH__UPDATE_H_43978348321832563832
#define VBH__UPDATE_H_43978348321832563832

#include "ace/Shared_Memory_MM.h"

#include "dsc/mem_mng/dsc_stl_type.h"
#include "dsc/codec/dsc_codec/dsc_codec.h"

#include "vbh_server_comm/vbh_server_comm_def_export.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_record_table.h"

//TODO: 所有的表，要修改为使用格式化工具格式，并生成目录；运行时程序中仅配置动态的cache等信息
class VBH_SERVER_COMM_DEF_EXPORT CVbhUpdateTable  final : public CVbhRecordTable
{
private:
	class CModifyMemLogItem
	{
	public:
		DSC_BIND_ATTR(m_nRecordID, m_itemOriginalValue)

	public:
		ACE_UINT64 m_nRecordID;
		DSC::CDscBlob m_itemOriginalValue; //存放item更改前的原始数据
		CPage* m_pPage = nullptr;

	public:
		CModifyMemLogItem* m_pNext = nullptr;
	};

	class CModifyMemLog
	{
	public:
		DSC_BIND_ATTR(m_queueUpdateLog)

	public:
		CDscSizeInfoUnboundQueue<CModifyMemLogItem> m_queueUpdateLog;
	};

public:
	// strBasePath为目录名,
	virtual ACE_INT32 Open(VBFS::CVbfs* pVbfs, const ACE_UINT32 nChannelID, const ACE_UINT32 nTableType) override;

	virtual void Close(void) override;

	template<typename RECORD_TYPE>
	ACE_INT32 Append(const ACE_UINT64 nRecordID, RECORD_TYPE& rRecord);

	template<typename RECORD_TYPE>
	ACE_INT32 Update(const ACE_UINT64 nRecordID, RECORD_TYPE& rRecord);

	//把变更保存到日志中，在ApplyModify之前被调用
	ACE_INT32 SaveToLog(void);
	ACE_INT32 Persistence(void);
	void CommitteTransaction(void);
	void RollbackCache(void);
	ACE_INT32 RollbackTransaction(void);
	ACE_INT32 RecoverFromLog(void);//启动时发现异常回滚

private:
	CModifyMemLog m_modifyMemLog; //变更日志,保存于内存中的所有item变更日志
	dsc_list_type(CPage*) m_lstDirtyPage; //update操作影响到的页， //其中的page仍旧存在于m_mapPage中

	//日志文件相关变量 //日志文件内容：当前记录条数(变更前)|待变更的item的ID以及变更前内容的列表
	CDscString m_strLogFilePath; //日志文件路径
	ACE_Shared_Memory_MM m_shmLog; //日志文件的共享内存对象
	char* m_pLogBuf = nullptr; //日志文件对应的共享内存缓冲区
	ACE_UINT32 m_nLogFilesize = 0; //日志文件的长度
};

#include "vbh_server_comm/vbh_const_length_storage/vbh_update_table.inl"

#endif
