#ifndef VBH_READ_TABLE_H_4645644313487843132165489465131
#define VBH_READ_TABLE_H_4645644313487843132165489465131

#include "dsc/container/bare_hash_map.h"
#include "dsc/mem_mng/mem_block_cache.h"

#include "vbh_server_comm/vbh_unite_dqueue.h"
#include "vbh_server_comm/vbh_server_comm_def_export.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_page_table.h"

class VBH_SERVER_COMM_DEF_EXPORT CVbhRecordTable
{
private:
	enum
	{
		EN_STEP_ENLARGE_CACHE_PAGE_NUM = 64,
		EN_PAGE_CACHE_HASH_MAP_BITS = 20, //page-cache的规模
	};

protected:
	class CPage
	{
	public:
		bool m_bNewPage = false;
		bool m_bDirty = false; //脏标记
		char* m_pContent = nullptr;
		ACE_UINT64 m_nPageID;

	public:
		ACE_UINT64 m_nKey = 0;
		CPage* m_pPrev = nullptr;
		CPage* m_pNext = nullptr;

	public: //作为CVbhUniteDqueue的元素需要的成员变量
		CPage* m_pDqueuePrev = nullptr;
		CPage* m_pDqueueNext = nullptr;
	};

public:
	virtual ACE_INT32 Open(VBFS::CVbfs* pVbfs, const ACE_UINT32 nChannelID, const ACE_UINT32 nTableType);

	virtual void Close(void);

	template<typename RECORD_TYPE>
	ACE_INT32 Read(RECORD_TYPE& rRecord, const ACE_UINT64 nRecordID);

protected:
	//根据nRecordID计算PageID
	ACE_UINT64 GetPageID(const ACE_UINT64 nRecordID) const;

	//计算record数据在page中的偏移量
	ACE_OFF_T GetOffset(const ACE_UINT64 nRecordID) const;

	//从cache获取page //如果cache中不存在，则调用子类实现读取page //读取不到从文件加载，加载后插入cache
	CPage* GetPage(const ACE_UINT64 nPageID);

	//将1页数据写回文件
	ACE_INT32 WritePage(CPage* pPage);
	ACE_INT32 WritePage(const char* pPageContent, const ACE_UINT64 nPageID, const bool bNewPage);

	//插入1个page到page-cache
	void InsertPage(CPage* pPage);

	//开辟1块页内容内存
	char* AllocPageContent(void);
	//释放页内容内存
	void FreePageContent(char* pContent);

	//创建一个page对象，并开辟其中需要的内存
	CPage* AllocPage(void);
	//释放一个page对象，并释放其中的内存 //意义和 AllocPage 对应
	void FreePage(CPage* pPage);

	//即去除一个page的勾连，也释放这个page
	void ReleasePage(CPage* pPage);

	//从page中获取指定item的数据指针
	char* GetRecordPtr(CPage* pPage, const ACE_UINT64 nRecordID) const;
	char* GetRecordPtrByPageRecordID(CPage* pPage, const ACE_UINT64 nPageRecordID) const;

	ACE_UINT64 GetPageRecordNum(void) const;
	ACE_UINT64 GetRecordLen(void) const;
	ACE_UINT32 GetPageSize(void) const;

protected:
	//读取页， 默认实现为读取文件
	virtual ACE_INT32 ReadPage(char* pPageContent, const ACE_UINT64 nPageID);

protected:
	using page_map_type = CBareHashMap<ACE_UINT64, CPage, EN_PAGE_CACHE_HASH_MAP_BITS>; //page-id -> page
	using page_queue_type = CVbhUniteDqueue<CPage>;

private:
	ACE_UINT32 m_nMapPageNum = 0; //已经缓存的page个数
	page_map_type m_mapPage;
	page_queue_type m_queuePage; //m_mapPage是真正的page-cache容器，m_queuePage用于配合map类型，便利确定page的活跃度，头部最不活跃，尾部最活跃
	CMemBlockCache m_freePageContentCache; //空闲页内容的缓冲区

private:
	CVbhPageTable* m_pVbhPageTable = nullptr;
	ACE_UINT32 m_nPageSize;
	ACE_UINT64 m_nRecordLen; // index中每条记录都定长，长度由m_nRecordLen；初始化后不可改变
	ACE_UINT64 m_nPageRecordNum;// 表示1页中记录的个数
	ACE_UINT32 m_nMaxPageCacheNum; // 内存cache中page的最大个数
};

#include "vbh_server_comm/vbh_const_length_storage/vbh_record_table.inl"

#endif
