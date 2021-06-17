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

	//构造日志文件路径
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

	if (-1 == ACE_OS::stat(strBasePath.c_str(), &stat)) //目录不存在
	{
		//创建基础路径
		if (-1 == DSC::DscRecurMkdir(strBasePath.c_str(), strBasePath.size()))
		{
			DSC_RUN_LOG_ERROR("create needed directory failed.%s", strBasePath.c_str());
			return -1;
		}

		CAppendTableConfig cfg;
		CAppendTableLog log;

		//创建 配置文件
		if (VBH::CreateCfgFile(strCtrlFilePath, cfg))
		{
			DSC_RUN_LOG_ERROR("create file:%s failed.", strCtrlFilePath.c_str());
			return -1;
		}

		//创建 日志文件
		if (VBH::CreateCfgFile(m_strLogFilePath, log))
		{
			DSC_RUN_LOG_ERROR("create file:%s failed.", strCtrlFilePath.c_str());
			return -1;
		}
	}

	//加载 头文件
	if (VBH::LoadMmapCfgFile(strCtrlFilePath, m_shmClsCtrlInfo, m_pCfg))
	{
		return -1;
	}
	m_nLastRecordID = m_pCfg->m_nLastRecordID;

	//加载 log 文件
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

	if (!(m_nLastRecordID % this->GetPageRecordNum())) //释放的ID在新页的开始，要同时释放新页
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
		//没做append操作，无需处理
	}

	m_pCfg->m_nLastRecordID = m_nLastRecordID;

	return 0;
}

void CVbhAppendTable::CommitteTransaction(void)
{
	//清除所有append也的脏标记，并清除整个append数组
	for (auto& it : m_queueAppendPage)
	{
		it->m_bDirty = false;
	}
	m_queueAppendPage.clear();

	//清除单独appen-page的脏标记和新建标记
	if (m_pAppendPage)
	{
		m_pAppendPage->m_bDirty = false;
		m_pAppendPage->m_bNewPage = false;
		m_pAppendPage = nullptr;
	}
	//m_appendPage.m_pPage 所指向的页还有未使用的空间，留着，用于下次append时添加record
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
				//已经在cache中了，不处理
				it->m_bDirty = false;
			}
		}
		m_queueAppendPage.clear();

		if (m_pAppendPage->m_bNewPage) //是新页，删除page, 去掉新页标记
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
		//没做append操作，无需处理
	}

	m_nLastRecordID = m_pCfg->m_nLastRecordID;
}
