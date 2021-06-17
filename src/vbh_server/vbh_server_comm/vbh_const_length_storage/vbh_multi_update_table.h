#ifndef VBH_MULTI_UPDATE_TABLE_H_87976435132165464321321
#define VBH_MULTI_UPDATE_TABLE_H_87976435132165464321321

#include "ace/Shared_Memory_MM.h"

#include "dsc/codec/dsc_codec/dsc_codec.h"

#include "vbh_server_comm/vbh_server_comm_def_export.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_record_table.h"

//TODO: 所有的表，要修改为使用格式化工具格式，并生成目录；运行时程序中仅配置动态的cache等信息
class VBH_SERVER_COMM_DEF_EXPORT CVbhMultiUpdateTable final : public CVbhRecordTable
{
protected:
	//表示1次打包的变更
	class CModifyPackage
	{
	public:
		enum
		{
			EN_MODIFY_PAGE_CACHE_HASH_MAP_BITS = 10
		};

		class CCodecPage //可用于编解码的page
		{
		public:
			DSC_BIND_ATTR(m_bNewPage, m_nPageID, m_pageDatae);

		public:
			bool m_bNewPage; //标记是否是新页
			ACE_UINT64 m_nPageID;
			DSC::CDscBlob m_pageDatae;
		};

	public:
		dsc_unordered_map_type(ACE_UINT64, CCodecPage) m_mapModifyPages; //修改中涉及的脏页 //打包后拷贝的页, //用于快速查找

	public: //使用CDscDqueue时，必须具有的成员
		CModifyPackage* m_pNext = nullptr;
		CModifyPackage* m_pPrev = nullptr;
	};

public:
	// strBasePath为目录名, 路径名必须以 '/'结尾 //strStoragePath为存储数据的裸设备名称
	virtual ACE_INT32 Open(VBFS::CVbfs* pVbfs, const ACE_UINT32 nChannelID, const ACE_UINT32 nTableType) override;
	virtual void Close(void) override;

	//约束上层业务ID从1开始，连续插入，不得有空洞
	template<typename RECORD_TYPE>
	ACE_INT32 Append(const ACE_UINT64 nRecordID, RECORD_TYPE& rRecord);

	//Update时:更新指定条目的数据 //目前简单实现约束，page cache足够大，在事务处理期间脏页不会被换出
	template<typename RECORD_TYPE>
	ACE_INT32 Update(const ACE_UINT64 nRecordID, RECORD_TYPE& rRecord);

	//回滚未打包的缓冲区 //回滚未调用 PackModify 时的多次Update调用
	void RollbackUnpackCache(void);

	//打包本次所有的变更，并将其缓存起来，以备后续真正提交到缓存
	void PackModify(void);

	//把变更保存到日志中，在ApplyModify之前被调用
	ACE_INT32 SaveToLog(void);
	ACE_INT32 Persistence(void);
	void CommitteTransaction(void);

	//依照日志的指引重做一次
	ACE_INT32 RedoByLog(void);

protected:
	//从cache获取page //如果cache中不存在，从文件加载
	virtual ACE_INT32 ReadPage(char* pPageContent, const ACE_UINT64 nPageID) override;

	//释放1个modify-package
	void FreeModifyPackage(CModifyPackage* pModifyPackage);

private:
	CDscDqueue<CModifyPackage> m_queueModifyPackage; //修改package队列
	dsc_list_type(CPage*) m_lstCurDirtyPage; //当前update操作影响到的页， //其中的page仍旧存在于m_mapPage中

	//日志文件相关变量 //日志文件内容：当前记录条数(变更前)|待变更的item的ID以及变更前内容的列表
	CDscString m_strLogFilePath; //日志文件路径
	ACE_Shared_Memory_MM m_shmLog; //日志文件的共享内存对象
	char* m_pLogBuf = nullptr; //日志文件对应的共享内存缓冲区
	ACE_UINT32 m_nLogFilesize = 0; //日志文件的长度
};

#include "vbh_server_comm/vbh_const_length_storage/vbh_multi_update_table.inl"

#endif
