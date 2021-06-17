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
		EN_PAGE_CACHE_HASH_MAP_BITS = 20, //page-cache�Ĺ�ģ
	};

protected:
	class CPage
	{
	public:
		bool m_bNewPage = false;
		bool m_bDirty = false; //����
		char* m_pContent = nullptr;
		ACE_UINT64 m_nPageID;

	public:
		ACE_UINT64 m_nKey = 0;
		CPage* m_pPrev = nullptr;
		CPage* m_pNext = nullptr;

	public: //��ΪCVbhUniteDqueue��Ԫ����Ҫ�ĳ�Ա����
		CPage* m_pDqueuePrev = nullptr;
		CPage* m_pDqueueNext = nullptr;
	};

public:
	virtual ACE_INT32 Open(VBFS::CVbfs* pVbfs, const ACE_UINT32 nChannelID, const ACE_UINT32 nTableType);

	virtual void Close(void);

	template<typename RECORD_TYPE>
	ACE_INT32 Read(RECORD_TYPE& rRecord, const ACE_UINT64 nRecordID);

protected:
	//����nRecordID����PageID
	ACE_UINT64 GetPageID(const ACE_UINT64 nRecordID) const;

	//����record������page�е�ƫ����
	ACE_OFF_T GetOffset(const ACE_UINT64 nRecordID) const;

	//��cache��ȡpage //���cache�в����ڣ����������ʵ�ֶ�ȡpage //��ȡ�������ļ����أ����غ����cache
	CPage* GetPage(const ACE_UINT64 nPageID);

	//��1ҳ����д���ļ�
	ACE_INT32 WritePage(CPage* pPage);
	ACE_INT32 WritePage(const char* pPageContent, const ACE_UINT64 nPageID, const bool bNewPage);

	//����1��page��page-cache
	void InsertPage(CPage* pPage);

	//����1��ҳ�����ڴ�
	char* AllocPageContent(void);
	//�ͷ�ҳ�����ڴ�
	void FreePageContent(char* pContent);

	//����һ��page���󣬲�����������Ҫ���ڴ�
	CPage* AllocPage(void);
	//�ͷ�һ��page���󣬲��ͷ����е��ڴ� //����� AllocPage ��Ӧ
	void FreePage(CPage* pPage);

	//��ȥ��һ��page�Ĺ�����Ҳ�ͷ����page
	void ReleasePage(CPage* pPage);

	//��page�л�ȡָ��item������ָ��
	char* GetRecordPtr(CPage* pPage, const ACE_UINT64 nRecordID) const;
	char* GetRecordPtrByPageRecordID(CPage* pPage, const ACE_UINT64 nPageRecordID) const;

	ACE_UINT64 GetPageRecordNum(void) const;
	ACE_UINT64 GetRecordLen(void) const;
	ACE_UINT32 GetPageSize(void) const;

protected:
	//��ȡҳ�� Ĭ��ʵ��Ϊ��ȡ�ļ�
	virtual ACE_INT32 ReadPage(char* pPageContent, const ACE_UINT64 nPageID);

protected:
	using page_map_type = CBareHashMap<ACE_UINT64, CPage, EN_PAGE_CACHE_HASH_MAP_BITS>; //page-id -> page
	using page_queue_type = CVbhUniteDqueue<CPage>;

private:
	ACE_UINT32 m_nMapPageNum = 0; //�Ѿ������page����
	page_map_type m_mapPage;
	page_queue_type m_queuePage; //m_mapPage��������page-cache������m_queuePage�������map���ͣ�����ȷ��page�Ļ�Ծ�ȣ�ͷ�����Ծ��β�����Ծ
	CMemBlockCache m_freePageContentCache; //����ҳ���ݵĻ�����

private:
	CVbhPageTable* m_pVbhPageTable = nullptr;
	ACE_UINT32 m_nPageSize;
	ACE_UINT64 m_nRecordLen; // index��ÿ����¼��������������m_nRecordLen����ʼ���󲻿ɸı�
	ACE_UINT64 m_nPageRecordNum;// ��ʾ1ҳ�м�¼�ĸ���
	ACE_UINT32 m_nMaxPageCacheNum; // �ڴ�cache��page��������
};

#include "vbh_server_comm/vbh_const_length_storage/vbh_record_table.inl"

#endif
