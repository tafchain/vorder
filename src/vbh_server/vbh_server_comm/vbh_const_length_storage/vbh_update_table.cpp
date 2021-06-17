#include "ace/OS_NS_sys_stat.h"

#include "dsc/dsc_log.h"

#include "vbh_comm/vbh_comm_func.h"
#include "vbh_comm/vbh_comm_macro_def.h"

#include "vbh_server_comm/vbh_const_length_storage/vbh_update_table.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_cls_config_param.h"

ACE_INT32 CVbhUpdateTable::Open(VBFS::CVbfs* pVbfs, const ACE_UINT32 nChannelID, const ACE_UINT32 nTableType)
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

	if (-1 == ACE_OS::stat(m_strLogFilePath.c_str(), &stat)) //������
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

	if (VBH::LoadMmapFile(m_strLogFilePath, m_shmLog, m_pLogBuf, m_nLogFilesize))
	{
		m_shmLog.close();

		return -1;
	}

	return 0;
}

void CVbhUpdateTable::Close(void)
{
	CModifyMemLogItem* pUpdateRecordLog = m_modifyMemLog.m_queueUpdateLog.PopFront();
	CModifyMemLogItem* pTempUpdateRecordLog;

	while (pUpdateRecordLog)
	{
		pUpdateRecordLog->m_itemOriginalValue.FreeBuffer();
		pTempUpdateRecordLog = pUpdateRecordLog;
		pUpdateRecordLog = m_modifyMemLog.m_queueUpdateLog.PopFront();
		DSC_THREAD_TYPE_DELETE(pTempUpdateRecordLog);
	}

	m_lstDirtyPage.clear();
	m_shmLog.close();

	CVbhRecordTable::Close();
}

ACE_INT32 CVbhUpdateTable::SaveToLog(void)
{
	DSC::CDscHostCodecEncoder encodeState;

	//1. ������������־���ȣ�����֤��־�ļ������㹻
	DSC::CDscCodecGetSizer& rGetSizeState = encodeState;

	rGetSizeState.GetSize(m_modifyMemLog);

	if (rGetSizeState.GetSize() > m_nLogFilesize) //���¿���log�ļ��Ĺ����ڴ��С
	{
		m_nLogFilesize = ACE_MALLOC_ROUNDUP(encodeState.GetSize(), VBH_CLS::EN_CLS_LOG_FILE_BASE_SIZE);

		if (VBH::ResizeMmapFile(m_strLogFilePath, m_shmLog, m_pLogBuf, m_nLogFilesize))
		{
			DSC_RUN_LOG_ERROR("resize-mmap-log-file failed.");

			return -1;
		}
	}

	//2. ����
	encodeState.SetBuffer(m_pLogBuf);
	encodeState.Encode(m_modifyMemLog);

	return 0;
}

ACE_INT32 CVbhUpdateTable::Persistence(void)
{
	for (auto& it : m_lstDirtyPage)
	{
		if (this->WritePage(it) < 0)
		{
			return -1;
		}
	}

	return 0;
}

void CVbhUpdateTable::CommitteTransaction(void)
{
	//�������ҳ������
	for (auto& it : m_lstDirtyPage)
	{
		it->m_bNewPage = false;
		it->m_bDirty = false;
	}
	m_lstDirtyPage.clear();

	CModifyMemLogItem* pUpdateRecordLog = m_modifyMemLog.m_queueUpdateLog.PopFront();
	CModifyMemLogItem* pTempUpdateRecordLog;

	while (pUpdateRecordLog)
	{
		pUpdateRecordLog->m_itemOriginalValue.FreeBuffer();
		pTempUpdateRecordLog = pUpdateRecordLog;
		pUpdateRecordLog = m_modifyMemLog.m_queueUpdateLog.PopFront();
		DSC_THREAD_TYPE_DELETE(pTempUpdateRecordLog);
	}

	//�ٴ�д����־��������ԭ������־���� //�����д���ǲ���ʧ�ܵ�
	this->SaveToLog();
}

void CVbhUpdateTable::RollbackCache(void)
{
	char* pRecord;

	//��֮ǰappend��ҳ���ڴ���Ĩȥ��ͬʱ��ҳ���Ϊ��ҳ
	//��֮ǰupdage�ľ�ҳд���ڴ��У�ͬʱ��ҳ���Ϊ��ҳ
	CModifyMemLogItem* pUpdateRecordLog = m_modifyMemLog.m_queueUpdateLog.PopFront();
	CModifyMemLogItem* pTempUpdateRecordLog;

	while (pUpdateRecordLog)
	{
		pRecord = this->GetRecordPtr(pUpdateRecordLog->m_pPage, pUpdateRecordLog->m_nRecordID);
		memcpy(pRecord, pUpdateRecordLog->m_itemOriginalValue.c_str(), this->GetRecordLen()); //�ָ�����
		pUpdateRecordLog->m_itemOriginalValue.FreeBuffer();

		pTempUpdateRecordLog = pUpdateRecordLog;
		pUpdateRecordLog = m_modifyMemLog.m_queueUpdateLog.PopFront();
		DSC_THREAD_TYPE_DELETE(pTempUpdateRecordLog);
	}

	for (auto& it : m_lstDirtyPage)
	{
		if (it->m_bNewPage)
		{
			this->ReleasePage(it);
		}
		else
		{
			it->m_bDirty = false;
		}
	}
	m_lstDirtyPage.clear();
}

ACE_INT32 CVbhUpdateTable::RecoverFromLog(void)
{
	//1. ��log-buf�н�����־����
	DSC::CDscHostCodecDecoder decodeState(m_pLogBuf, m_nLogFilesize);
	CModifyMemLog modifyMemLog; //�����־,�������ڴ��е�����item�����־
	dsc_list_type(CPage*) lstDirtyPage; //update����Ӱ�쵽��ҳ�� //���е�page�Ծɴ�����m_mapPage��

	if (decodeState.Decode(modifyMemLog))
	{
		DSC_RUN_LOG_ERROR("decode modify-mem-log from logged buffer failed");
		return -1;
	}

	CPage* pPage;
	char* pRecord;

	CModifyMemLogItem* pUpdateRecordLog = modifyMemLog.m_queueUpdateLog.PopFront();
	CModifyMemLogItem* pTempUpdateRecordLog;

	while (pUpdateRecordLog)
	{
		pPage = this->GetPage(this->GetPageID(pUpdateRecordLog->m_nRecordID));
		pRecord = this->GetRecordPtr(pPage, pUpdateRecordLog->m_nRecordID);
		memcpy(pRecord, pUpdateRecordLog->m_itemOriginalValue.c_str(), this->GetRecordLen()); //�ָ�����

		if (!pPage->m_bDirty)
		{
			pPage->m_bDirty = true;
			lstDirtyPage.push_back(pPage);
		}

		pTempUpdateRecordLog = pUpdateRecordLog;
		pUpdateRecordLog = modifyMemLog.m_queueUpdateLog.PopFront();
		DSC_THREAD_TYPE_DELETE(pTempUpdateRecordLog);
	}

	for (auto& it : lstDirtyPage)
	{
		if (this->WritePage(it) < 0)
		{
			return -1;
		}
		it->m_bDirty = false;
	}
	lstDirtyPage.clear();

	return 0;
}
