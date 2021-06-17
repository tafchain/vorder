#include "ace/OS_NS_sys_stat.h"

#include "dsc/dsc_log.h"

#include "vbh_comm/vbh_comm_func.h"
#include "vbh_comm/vbh_comm_macro_def.h"

#include "vbh_server_comm/vbh_const_length_storage/vbh_multi_update_table.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_cls_config_param.h"


ACE_INT32 CVbhMultiUpdateTable::Open(VBFS::CVbfs* pVbfs, const ACE_UINT32 nChannelID, const ACE_UINT32 nTableType)
{
	if (CVbhRecordTable::Open(pVbfs, nChannelID, nTableType))
	{
		return -1;
	}

	//构造日志文件路径
	CDscString strBasePath(CDscAppManager::Instance()->GetWorkRoot());

	strBasePath += DSC_FILE_PATH_SPLIT_CHAR;
	strBasePath += DEF_STORAGE_DIR_NAME;
	strBasePath += DSC_FILE_PATH_SPLIT_CHAR;
	strBasePath += "channel_";
	strBasePath += nChannelID;
	strBasePath += DSC_FILE_PATH_SPLIT_CHAR;
	strBasePath += VBH_CLS::GetClsTableName(nTableType);
	strBasePath += DSC_FILE_PATH_SPLIT_CHAR;

	m_strLogFilePath = strBasePath;
	m_strLogFilePath += DEF_CLS_LOG_FILE_NAME;

	ACE_stat stat;

	if (-1 == ACE_OS::stat(strBasePath.c_str(), &stat)) //不存在
	{
		//创建基础路径
		if (-1 == DSC::DscRecurMkdir(strBasePath.c_str(), strBasePath.size()))
		{
			DSC_RUN_LOG_ERROR("create needed directory failed.%s", strBasePath.c_str());
			return -1;
		}

		if (VBH::CreateMmapFile(m_strLogFilePath, VBH_CLS::EN_CLS_LOG_FILE_BASE_SIZE))
		{
			DSC_RUN_LOG_ERROR("create file:%s failed.", m_strLogFilePath.c_str());
			return -1;
		}
	}

	//加载 log 文件
	if (VBH::LoadMmapFile(m_strLogFilePath, m_shmLog, m_pLogBuf, m_nLogFilesize))
	{
		m_shmLog.close();

		return -1;
	}

	return 0;
}

void CVbhMultiUpdateTable::Close(void)
{
	CModifyPackage* pModifyPackage = m_queueModifyPackage.PopFront();
	while (pModifyPackage)
	{
		FreeModifyPackage(pModifyPackage);
		pModifyPackage = m_queueModifyPackage.PopFront();
	}

	for (auto& it : m_lstCurDirtyPage)
	{
		this->FreePage(it);
	}
	m_lstCurDirtyPage.clear();

	m_shmLog.close();

	CVbhRecordTable::Close();
}

void CVbhMultiUpdateTable::RollbackUnpackCache(void)
{
	for (auto& it : m_lstCurDirtyPage)
	{
		this->ReleasePage(it);
	}

	m_lstCurDirtyPage.clear();
}

void CVbhMultiUpdateTable::PackModify(void)
{
	//拷贝所有脏页, 拷贝后的页去掉脏标记,
	char* pPageContent;
	CModifyPackage* pCurModifyPackage = DSC_THREAD_TYPE_NEW(CModifyPackage) CModifyPackage; //当前正在进行的modify-package，打包时，放在修改package队尾
	CModifyPackage::CCodecPage codecPage;

	for (auto& it : m_lstCurDirtyPage)
	{
		pPageContent = AllocPageContent();
		::memcpy(pPageContent, it->m_pContent, this->GetPageSize());
		codecPage.m_bNewPage = it->m_bNewPage;
		codecPage.m_nPageID = it->m_nPageID;
		codecPage.m_pageDatae.Set(pPageContent, this->GetPageSize());

		pCurModifyPackage->m_mapModifyPages.insert(std::make_pair(it->m_nPageID, codecPage));

		it->m_bDirty = false; //原页清除脏标记
		it->m_bNewPage = false;
	}

	m_queueModifyPackage.PushBack(pCurModifyPackage);
	m_lstCurDirtyPage.clear();
}

ACE_INT32 CVbhMultiUpdateTable::SaveToLog(void)
{
	DSC::CDscHostCodecEncoder encoder;
	CModifyPackage* pModifyPackage = m_queueModifyPackage.Front();
	ACE_UINT32 nPageCount = pModifyPackage->m_mapModifyPages.size();

	//1. 计算待保存的日志长度，并保证日志文件长度足够
	DSC::CDscCodecGetSizer& rGetSizeState = encoder;

	rGetSizeState.GetSize(nPageCount);
	for (auto& it :pModifyPackage->m_mapModifyPages)
	{
		rGetSizeState.GetSize(it.second);
	}

	if (rGetSizeState.GetSize() > m_nLogFilesize) //重新开辟log文件的共享内存大小
	{
		m_nLogFilesize = ACE_MALLOC_ROUNDUP(encoder.GetSize(), VBH_CLS::EN_CLS_LOG_FILE_BASE_SIZE);

		if (VBH::ResizeMmapFile(m_strLogFilePath, m_shmLog, m_pLogBuf, m_nLogFilesize))
		{
			DSC_RUN_LOG_ERROR("resize-mmap-log-file failed.");

			return -1;
		}
	}

	//2. 编码
	encoder.SetBuffer(m_pLogBuf);
	encoder.Encode(nPageCount);
	for (auto& it : pModifyPackage->m_mapModifyPages)
	{
		encoder.Encode(it.second);
	}

	return 0;
}

ACE_INT32 CVbhMultiUpdateTable::Persistence(void)
{
	CModifyPackage* pModifyPackage = m_queueModifyPackage.Front();

	for (auto& it : pModifyPackage->m_mapModifyPages)
	{
		if (this->WritePage(it.second.m_pageDatae.GetBuffer(), it.second.m_nPageID, it.second.m_bNewPage) < 0)
		{
			DSC_RUN_LOG_ERROR("write page failed, page id:%lld, new-page-flage:%d.", it.second.m_nPageID, it.second.m_bNewPage);

			return -1;
		}
	}

	return 0;
}

ACE_INT32 CVbhMultiUpdateTable::RedoByLog(void)
{
	//1. 从log-buf中解码出 脏页
	DSC::CDscHostCodecDecoder decoder(m_pLogBuf, m_nLogFilesize);
	ACE_UINT32 nPageCount;
	CModifyPackage::CCodecPage page;
	dsc_list_type(CModifyPackage::CCodecPage) lstPages;

	if (decoder.Decode(nPageCount))
	{
		DSC_RUN_LOG_ERROR("decode modify-page-count from logged buffer failed");
		return -1;
	}

	for (ACE_UINT32 idx = 0; idx < nPageCount; ++idx)
	{
		if (decoder.Decode(page))
		{
			DSC_RUN_LOG_ERROR("decode modify-page from logged buffer failed");
			return -1;
		}

		lstPages.push_back(page);
	}

	//2. 将脏页写入文件
	for (auto& it : lstPages)
	{
		if (this->WritePage(it.m_pageDatae.GetBuffer(), it.m_nPageID, it.m_bNewPage) < 0)
		{
			DSC_RUN_LOG_ERROR("write page failed, page-id:%lld, new-page-flage:%d", it.m_nPageID, it.m_bNewPage);
			return -1;
		}
	}

	return 0;
}

ACE_INT32 CVbhMultiUpdateTable::ReadPage(char* pPageContent, const ACE_UINT64 nPageID)
{
	CModifyPackage* pModifyPackage = m_queueModifyPackage.Back();
	dsc_unordered_map_type(ACE_UINT64, CModifyPackage::CCodecPage)::iterator it;

	//向前遍历
	while (pModifyPackage)
	{
		it = pModifyPackage->m_mapModifyPages.find(nPageID);
		if (it == pModifyPackage->m_mapModifyPages.end())
		{
			pModifyPackage = pModifyPackage->m_pPrev;
		}
		else
		{
			//如果从L2-Cache中找到了，则赋值一份
			::memcpy(pPageContent, it->second.m_pageDatae.GetBuffer(), this->GetPageSize());

			return 0;
		}
	}

	return CVbhRecordTable::ReadPage(pPageContent, nPageID);
}

void CVbhMultiUpdateTable::FreeModifyPackage(CModifyPackage* pModifyPackage)
{
	for (auto& it : pModifyPackage->m_mapModifyPages)
	{
		this->FreePageContent(it.second.m_pageDatae.GetBuffer());
	}

	DSC_THREAD_TYPE_DELETE(pModifyPackage);
}
