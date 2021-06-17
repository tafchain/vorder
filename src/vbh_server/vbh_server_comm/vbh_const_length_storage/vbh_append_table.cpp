#include "ace/OS_NS_sys_stat.h"

#include "dsc/dsc_log.h"

#include "vbh_comm/vbh_comm_func.h"
#include "vbh_comm/vbh_comm_macro_def.h"

#include "vbh_server_comm/vbh_const_length_storage/vbh_append_table.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_cls_config_param.h"

ACE_INT32 CVbhAppendTable::Open(VBFS::CVbfs* pVbfs, const ACE_UINT32 nChannelID, const ACE_UINT32 nTableType)
{
	if (CVbhRecordTable::Open(pVbfs, nChannelID, nTableType))
	{
		return -1;
	}

	//������־�ļ�·��
	CDscString strBasePath(CDscAppManager::Instance()->GetWorkRoot());
	CDscString strCtrlFilePath;
	ACE_stat stat;

	strBasePath += DSC_FILE_PATH_SPLIT_CHAR;
	strBasePath += DEF_STORAGE_DIR_NAME;
	strBasePath += DSC_FILE_PATH_SPLIT_CHAR;
	strBasePath += "channel_";
	strBasePath += nChannelID;
	strBasePath += DSC_FILE_PATH_SPLIT_CHAR;
	strBasePath += VBH_CLS::GetClsTableName(nTableType);
	strBasePath += DSC_FILE_PATH_SPLIT_CHAR;

	strCtrlFilePath = strBasePath;
	strCtrlFilePath += DEF_CLS_CONFIG_FILE_NAME;
	m_strLogFilePath = strBasePath;
	m_strLogFilePath += DEF_CLS_LOG_FILE_NAME;

	if (-1 == ACE_OS::stat(strBasePath.c_str(), &stat)) //Ŀ¼������
	{
		//��������·��
		if (-1 == DSC::DscRecurMkdir(strBasePath.c_str(), strBasePath.size()))
		{
			DSC_RUN_LOG_ERROR("create needed directory failed.%s", strBasePath.c_str());
			return -1;
		}

		CAppendTableConfig cfg;
		CAppendTableLog log;

		//���� �����ļ�
		if (VBH::CreateCfgFile(strCtrlFilePath, cfg))
		{
			DSC_RUN_LOG_ERROR("create file:%s failed.", strCtrlFilePath.c_str());
			return -1;
		}

		//���� ��־�ļ�
		if (VBH::CreateCfgFile(m_strLogFilePath, log))
		{
			DSC_RUN_LOG_ERROR("create file:%s failed.", strCtrlFilePath.c_str());
			return -1;
		}
	}

	//���� ͷ�ļ�
	if (VBH::LoadMmapCfgFile(strCtrlFilePath, m_shmClsCtrlInfo, m_pCfg))
	{
		return -1;
	}
	m_nLastRecordID = m_pCfg->m_nLastRecordID;

	//���� log �ļ�
	if (VBH::LoadMmapCfgFile(strCtrlFilePath, m_shmLog, m_pLog))
	{
		m_shmClsCtrlInfo.close();

		return -1;
	}

	return 0;
}

void CVbhAppendTable::Close(void)
{
	m_queueAppendPage.clear();
	m_pAppendPage = nullptr;

	m_shmClsCtrlInfo.close();
	m_shmLog.close();

	CVbhRecordTable::Close();
}

void CVbhAppendTable::PopBack(const ACE_UINT64 nRecordID)
{
	--m_nLastRecordID;

	ACE_ASSERT(nRecordID == m_nLastRecordID);

	if (!(m_nLastRecordID % this->GetPageRecordNum())) //�ͷŵ�ID����ҳ�Ŀ�ʼ��Ҫͬʱ�ͷ���ҳ
	{
		if (m_pAppendPage)
		{
			this->ReleasePage(m_pAppendPage);

			if (m_queueAppendPage.size())
			{
				m_pAppendPage = m_queueAppendPage.back();
				m_queueAppendPage.pop_back();
			}
			else
			{
				m_pAppendPage = nullptr;
			}
		}
	}
}

ACE_INT32 CVbhAppendTable::Persistence(void)
{
	if (m_nLastRecordID > m_pCfg->m_nLastRecordID)
	{
		for (auto& it : m_queueAppendPage)
		{
			if (this->WritePage(it) < 0)
			{
				return -1;
			}
		}

		if (this->WritePage(m_pAppendPage) < 0)
		{
			return -1;
		}
	}
	else
	{
		//û��append���������账��
	}

	m_pCfg->m_nLastRecordID = m_nLastRecordID;

	return 0;
}

void CVbhAppendTable::CommitteTransaction(void)
{
	//�������appendҲ�����ǣ����������append����
	for (auto& it : m_queueAppendPage)
	{
		it->m_bDirty = false;
	}
	m_queueAppendPage.clear();

	//�������appen-page�����Ǻ��½����
	if (m_pAppendPage)
	{
		m_pAppendPage->m_bDirty = false;
		m_pAppendPage->m_bNewPage = false;
		m_pAppendPage = nullptr;
	}
	//m_appendPage.m_pPage ��ָ���ҳ����δʹ�õĿռ䣬���ţ������´�appendʱ���record
}

void CVbhAppendTable::RollbackCache(void)
{
	if (m_nLastRecordID > m_pCfg->m_nLastRecordID)
	{
		for (auto& it : m_queueAppendPage)
		{
			if (it->m_bNewPage)
			{
				this->ReleasePage(it);
			}
			else
			{
				//�Ѿ���cache���ˣ�������
				it->m_bDirty = false;
			}
		}
		m_queueAppendPage.clear();

		if (m_pAppendPage->m_bNewPage) //����ҳ��ɾ��page, ȥ����ҳ���
		{
			this->ReleasePage(m_pAppendPage);
			m_pAppendPage = nullptr;
		}
		else
		{
			m_pAppendPage->m_bDirty = false;
			m_pAppendPage->m_bNewPage = false;
		}
	}
	else
	{
		//û��append���������账��
	}

	m_nLastRecordID = m_pCfg->m_nLastRecordID;
}
