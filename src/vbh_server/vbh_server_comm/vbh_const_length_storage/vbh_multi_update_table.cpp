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

	//������־�ļ�·��
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

	if (-1 == ACE_OS::stat(strBasePath.c_str(), &stat)) //������
	{
		//��������·��
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

	//���� log �ļ�
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
	//����������ҳ, �������ҳȥ������,
	char* pPageContent;
	CModifyPackage* pCurModifyPackage = DSC_THREAD_TYPE_NEW(CModifyPackage) CModifyPackage; //��ǰ���ڽ��е�modify-package�����ʱ�������޸�package��β
	CModifyPackage::CCodecPage codecPage;

	for (auto& it : m_lstCurDirtyPage)
	{
		pPageContent = AllocPageContent();
		::memcpy(pPageContent, it->m_pContent, this->GetPageSize());
		codecPage.m_bNewPage = it->m_bNewPage;
		codecPage.m_nPageID = it->m_nPageID;
		codecPage.m_pageDatae.Set(pPageContent, this->GetPageSize());

		pCurModifyPackage->m_mapModifyPages.insert(std::make_pair(it->m_nPageID, codecPage));

		it->m_bDirty = false; //ԭҳ�������
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

	//1. ������������־���ȣ�����֤��־�ļ������㹻
	DSC::CDscCodecGetSizer& rGetSizeState = encoder;

	rGetSizeState.GetSize(nPageCount);
	for (auto& it :pModifyPackage->m_mapModifyPages)
	{
		rGetSizeState.GetSize(it.second);
	}

	if (rGetSizeState.GetSize() > m_nLogFilesize) //���¿���log�ļ��Ĺ����ڴ��С
	{
		m_nLogFilesize = ACE_MALLOC_ROUNDUP(encoder.GetSize(), VBH_CLS::EN_CLS_LOG_FILE_BASE_SIZE);

		if (VBH::ResizeMmapFile(m_strLogFilePath, m_shmLog, m_pLogBuf, m_nLogFilesize))
		{
			DSC_RUN_LOG_ERROR("resize-mmap-log-file failed.");

			return -1;
		}
	}

	//2. ����
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
	//1. ��log-buf�н���� ��ҳ
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

	//2. ����ҳд���ļ�
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

	//��ǰ����
	while (pModifyPackage)
	{
		it = pModifyPackage->m_mapModifyPages.find(nPageID);
		if (it == pModifyPackage->m_mapModifyPages.end())
		{
			pModifyPackage = pModifyPackage->m_pPrev;
		}
		else
		{
			//�����L2-Cache���ҵ��ˣ���ֵһ��
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
