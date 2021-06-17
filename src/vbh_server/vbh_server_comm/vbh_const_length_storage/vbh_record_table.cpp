#include "ace/OS_NS_sys_stat.h"
#include "ace/OS_NS_fcntl.h"
#include "ace/OS_NS_unistd.h"

#include "dsc/dsc_log.h"

#include "vbh_server_comm/vbh_const_length_storage/vbh_record_table.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_cls_config_param.h"
#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs_config_select_def.h"

template<typename SELECT_TYPE>
ACE_INT32 ReadVbfsConfig(ACE_UINT64& nFileSize, ACE_UINT32& nPageSize, ACE_UINT32& nPageCacheCount, const ACE_UINT32 nChannelID)
{
	CDscDatabase database;
	CDBConnection dbConnection;

	if (CDscDatabaseFactoryDemon::instance()->CreateDatabase(database, dbConnection))
	{
		DSC_RUN_LOG_ERROR("connect database failed.");

		return -1;
	}
	else
	{
		CTableWrapper< SELECT_TYPE > vbfsCfg("VBFS_CONFIG");
		CVbfsCriterion criterion(nChannelID);

		if (::PerSelect(vbfsCfg, database, dbConnection, &criterion))
		{
			DSC_RUN_LOG_ERROR("select from VBFS_CONFIG get nothing, channel-id:%u.", nChannelID);

			return -1;
		}
		else
		{
			nFileSize = *vbfsCfg->m_fileSize;
			nFileSize <<= 30; //读取的数据以GB为单位

			nPageSize = *vbfsCfg->m_pageSize;
			nPageSize <<= 10; //数据以KB为单位

			nPageCacheCount = *vbfsCfg->m_pageCacheCount;
		}
	}

	return 0;
}

ACE_INT32 CVbhRecordTable::Open(VBFS::CVbfs* pVbfs, const ACE_UINT32 nChannelID, const ACE_UINT32 nTableType)
{
	ACE_UINT64 nFileSize;

	switch (nTableType)
	{
	case VBH_CLS::EN_WRITE_SET_VERSION_TABLE_TYPE:
	{
		if (ReadVbfsConfig<CVbfsConfigWriteSetVersion>(nFileSize, m_nPageSize, m_nMaxPageCacheNum, nChannelID))
		{
			return -1;
		}

		m_nRecordLen = VBH_CLS::CVersionTableItem::EN_SIZE;
	}
	break;
	case VBH_CLS::EN_BLOCK_INDEX_TABLE_TYPE:
	{
		if (ReadVbfsConfig<CVbfsConfigBlockIndex>(nFileSize, m_nPageSize, m_nMaxPageCacheNum, nChannelID))
		{
			return -1;
		}

		m_nRecordLen = VBH_CLS::CBcIndexTableItem::EN_SIZE;
	}
	break;
	case VBH_CLS::EN_WRITE_SET_INDEX_TABLE_TYPE:
	{
		if (ReadVbfsConfig<CVbfsConfigWriteSetIndex>(nFileSize, m_nPageSize, m_nMaxPageCacheNum, nChannelID))
		{
			return -1;
		}

		m_nRecordLen = VBH_CLS::CIndexTableItem::EN_SIZE;
	}
	break;
	case VBH_CLS::EN_WRITE_SET_HISTORY_TABLE_TYPE:
	{
		if (ReadVbfsConfig<CVbfsConfigWriteSetHistory>(nFileSize, m_nPageSize, m_nMaxPageCacheNum, nChannelID))
		{
			return -1;
		}

		m_nRecordLen = VBH_CLS::CHistoryTableItem::EN_SIZE;
	}
	break;
	default:
	{
		DSC_RUN_LOG_ERROR("unknown table-type:%u", nTableType);
		return -1;
	}
	}

	ACE_UINT32 nFilePageCount = nFileSize / m_nPageSize;

	m_nPageRecordNum = m_nPageSize / m_nRecordLen;
	m_pVbhPageTable = DSC_THREAD_TYPE_NEW(CVbhPageTable) CVbhPageTable(pVbfs, nTableType, m_nPageSize, nFilePageCount);

	return 0;
}

void CVbhRecordTable::Close(void)
{
	if (m_pVbhPageTable)
	{
		DSC_THREAD_TYPE_DELETE(m_pVbhPageTable);
		m_pVbhPageTable = nullptr;
	}

	//删除所有的缓存页 //从1个遍历器入手就可以了
	CPage* pPage = m_queuePage.PopFront();
	while (pPage)
	{
		this->FreePage(pPage);
		pPage = m_queuePage.PopFront();
	}

	char* pMemBlock = m_freePageContentCache.Pop();
	while (pMemBlock)
	{
		DSC_FREE(pMemBlock);
		pMemBlock = m_freePageContentCache.Pop();
	}
}

//脏页不换出到磁盘，新页换出
CVbhRecordTable::CPage* CVbhRecordTable::GetPage(const ACE_UINT64 nPageID)
{
	CPage* pPage = m_mapPage.Find(nPageID);

	if (pPage)
	{
		m_queuePage.Erase(pPage);//放到队尾，队尾是最活跃page
		m_queuePage.PushBack(pPage);
	}
	else //没有找到对应的页，则是要创建新页（换入或开辟）的场景
	{
		if (m_nMapPageNum > m_nMaxPageCacheNum) //控制m_mapPage的大小，即page-cache的大小 //比设定值小时，开辟新页，否则利用已有的不活跃页
		{
			pPage = m_queuePage.Front(); //m_queuePage的头部是最不活跃page
			while (pPage)
			{
				if (pPage->m_bDirty) //脏页不换出，因此跳过脏页
				{
					pPage = pPage->m_pDqueueNext;
				}
				else
				{
					break;
				}
			}

			//找到合适的页则将其从map中移除掉，没有找到则开辟新页
			if (pPage)
			{
				m_queuePage.Erase(pPage);
				m_mapPage.Erase(pPage);
				--m_nMapPageNum;
			}
			else//page全脏，将cache限制扩大
			{
				pPage = this->AllocPage();
				m_nMaxPageCacheNum += EN_STEP_ENLARGE_CACHE_PAGE_NUM; //扩大cache限制
			}
		}
		else
		{
			pPage = this->AllocPage();
		}

		if (this->ReadPage(pPage->m_pContent, nPageID))
		{
			DSC_RUN_LOG_ERROR("ReadPage failed, PageID:%lld.", nPageID);

			this->FreePage(pPage); //读取失败后，释放page
			pPage = nullptr;
		}
		else
		{
			pPage->m_nPageID = nPageID;
			m_mapPage.DirectInsert(nPageID, pPage);
			++m_nMapPageNum;
			m_queuePage.PushBack(pPage);
		}
	}

	return pPage;
}

void CVbhRecordTable::InsertPage(CPage* pPage)
{
	//如果cache已满，则释放最不活跃的not-dirty-page
	if (m_nMapPageNum > m_nMaxPageCacheNum)
	{
		CPage* pOldPage = m_queuePage.Front(); //队头page最不活跃
		while (pOldPage)
		{
			if (pOldPage->m_bDirty) //dirty-page不能换出，跳过
			{
				pOldPage = pOldPage->m_pDqueueNext;
			}
			else
			{
				break;
			}
		}

		if (pOldPage)
		{
			ReleasePage(pOldPage);
		}
		else //page全脏，cache扩大一倍(只是限制扩大一倍，并非一下子将内存池扩大到原来的两倍)
		{
			m_nMaxPageCacheNum += EN_STEP_ENLARGE_CACHE_PAGE_NUM; //扩大cache限制
		}
	}

	m_mapPage.DirectInsert(pPage->m_nPageID, pPage);
	++m_nMapPageNum;
	m_queuePage.PushBack(pPage);
}
