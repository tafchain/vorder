#include "dsc/dsc_log.h"
#include "dsc/db/per/persistence.h"
#include "dsc/dsc_database_factory.h"

#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs.h"
#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs_raw.h"
#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs_file.h"
#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs_config_select_def.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_cls_config_param.h"

ACE_INT32 VBFS::CVbfs::Open(const ACE_UINT32 nChannelID)
{
	CDscDatabase database;
	CDBConnection dbConnection;
	ACE_UINT64 nFileSize;

	m_nChannelID = nChannelID;

	if (CDscDatabaseFactoryDemon::instance()->CreateDatabase(database, dbConnection))
	{
		DSC_RUN_LOG_ERROR("connect database failed.");

		return -1;
	}

	CTableWrapper< CVbfsConfigOnlyFileSize > vbfsCfg("VBFS_CONFIG");
	CVbfsCriterion criterion(nChannelID);

	if (::PerSelect(vbfsCfg, database, dbConnection, &criterion))
	{
		DSC_RUN_LOG_ERROR("select from VBFS_CONFIG failed.");

		return -1;
	}
	else
	{
		nFileSize = *vbfsCfg->m_fileSize;
		nFileSize <<= 30; //读取的数据以GB为单位

		m_strDeviceType = *vbfsCfg->m_deviceType;

		if (!((m_strDeviceType == DEF_RAW_VBFS_TYPE) || (m_strDeviceType == DEF_FILE_SYSTEM_VBFS_TYPE)))
		{
			DSC_RUN_LOG_ERROR("device type:%s is error, it can only be \"%s\" or\"%s\", channel-id:%u",
				m_strDeviceType.c_str(), DEF_RAW_VBFS_TYPE, DEF_FILE_SYSTEM_VBFS_TYPE, nChannelID);

			return -1;
		}
	}

	CTableWrapper< CCollectWrapper<CVbfsDeviceConfig> > vbfsDeviceCfg("VBFS_DEVICE_CONFIG");
	CVbfsDeviceCriterion deviceCriterion(nChannelID);

	if (::PerSelect(vbfsDeviceCfg, database, dbConnection, &deviceCriterion))
	{
		DSC_RUN_LOG_ERROR("select from XCS_CFG failed, channel-id:%u.", nChannelID);

		return -1;
	}

	VBFS::CVbfsConfigParam param;

	param.m_nFileSize = nFileSize;

	m_nAllocNum = vbfsDeviceCfg->size();
	m_pVbfsPtr = (IVbfsImplPtr*)DSC_THREAD_SIZE_MALLOC(sizeof(IVbfsImplPtr) * m_nAllocNum);
	ACE_OS::memset(m_pVbfsPtr, 0, sizeof(IVbfsImplPtr) * m_nAllocNum);

	for (auto it = vbfsDeviceCfg->begin(); it != vbfsDeviceCfg->end(); ++it)
	{
		m_nLastRawVbfsDeviceIndex = *it->m_deviceIndex;
		param.m_nFileNum = *it->m_fileNum;

		if (m_strDeviceType == DEF_RAW_VBFS_TYPE)
		{
			DSC_NEW(m_pLastVbfs, VBFS::CRawVbfs);
		}
		else
		{
			DSC_NEW(m_pLastVbfs, VBFS::CFileVbfs);
		}

		if (m_pLastVbfs->Open(*it->m_deviceUrl, param))
		{
			DSC_RUN_LOG_ERROR("open %s failed.", (*it->m_deviceUrl).c_str());
			this->Close();
			DSC_DELETE(m_pLastVbfs);

			return -1;
		}

		m_pVbfsPtr[m_nCurNum] = m_pLastVbfs;
		++m_nCurNum;

		if (m_pLastVbfs->GetAlocPhyFileNum() < m_pLastVbfs->GetMaxPhyFileNum())
		{
			break;
		}
	}

	if (m_pLastVbfs->GetAlocPhyFileNum() >= m_pLastVbfs->GetMaxPhyFileNum()) //最后1个文件仍旧是满的
	{
		DSC_RUN_LOG_ERROR("none device can alloc phy-file.");
		this->Close();

		return -1;
	}

	return 0;
}

void VBFS::CVbfs::Close(void)
{
	if (m_pVbfsPtr)
	{
		for (ACE_UINT32 i = 0; i < m_nCurNum; ++i)
		{
			m_pVbfsPtr[i]->Close();
			DSC_DELETE(m_pVbfsPtr[i]);
		}
		DSC_THREAD_SIZE_FREE((char*)m_pVbfsPtr, sizeof(IVbfsImplPtr) * m_nAllocNum);
		m_pVbfsPtr = nullptr;
	    m_nCurNum = 0;
	}
}


ACE_INT32 VBFS::CVbfs::Read(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID, char* pBuf, const ACE_UINT64 nBufSize, const ACE_UINT64 nBufOffset)
{
	ACE_UINT32 nRealFileID = nFileID;
	ACE_UINT32 nLastLogicFileID;
	ACE_UINT32 idx = 0;

	ACE_ASSERT(nFileType > VBH_CLS::EN_INVALID_TABLE_TYPE && nFileType < EN_MAX_FILE_TYPE);

	for (; idx < m_nCurNum; ++idx)
	{
		nLastLogicFileID = m_pVbfsPtr[idx]->GetAlocLogicFileNum(nFileType);

		if (nLastLogicFileID <= nRealFileID)
		{
			nRealFileID -= nLastLogicFileID;
		}
		else
		{
			break;
		}
	}

	return m_pVbfsPtr[idx]->Read(nFileType, nRealFileID, pBuf, nBufSize, nBufOffset);
}

ACE_INT32 VBFS::CVbfs::Write(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID, const char* pBuf, const ACE_UINT64 nBufSize, const ACE_UINT64 nBufOffset)
{
	ACE_UINT32 nRealFileID = nFileID;
	ACE_UINT32 nLastLogicFileID;
	ACE_UINT32 idx = 0;

	ACE_ASSERT(nFileType > VBH_CLS::EN_INVALID_TABLE_TYPE && nFileType < EN_MAX_FILE_TYPE);

	for (; idx < m_nCurNum; ++idx)
	{
		nLastLogicFileID = m_pVbfsPtr[idx]->GetAlocLogicFileNum(nFileType);

		if (nLastLogicFileID <= nRealFileID)
		{
			nRealFileID -= nLastLogicFileID;
		}
		else
		{
			break;
		}
	}

	return m_pVbfsPtr[idx]->Write(nFileType, nRealFileID, pBuf, nBufSize, nBufOffset);
}

ACE_INT32 VBFS::CVbfs::AllocatFile(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID)
{
	ACE_UINT32 nSegFileID = nFileID;

	ACE_ASSERT(nFileType > VBH_CLS::EN_INVALID_TABLE_TYPE && nFileType < EN_MAX_FILE_TYPE);

	for (ACE_UINT32 idx = 0; idx < m_nCurNum - 1; ++idx)
	{
		nSegFileID -= m_pVbfsPtr[idx]->GetAlocLogicFileNum(nFileType);
	}

	switch (m_pLastVbfs->AllocatFile(nFileType, nSegFileID))
	{
	case VBFS::EN_VBFS_IO_OK:
	{
		return 0;
	}
	case VBFS::EN_VBFS_IO_LOGIC_ERROR:
	{
		return -1;
	}
	case VBFS::EN_VBFS_IO_DISK_FULL:
	{
		CDscDatabase database;
		CDBConnection dbConnection;
		ACE_UINT64 nFileSize;

		if (CDscDatabaseFactoryDemon::instance()->CreateDatabase(database, dbConnection))
		{
			DSC_RUN_LOG_ERROR("connect database failed.");

			return -1;
		}

		CTableWrapper< CVbfsConfigOnlyFileSize > vbfsCfg("VBFS_CONFIG");
		CVbfsCriterion criterion(m_nChannelID);

		if (::PerSelect(vbfsCfg, database, dbConnection, &criterion))
		{
			DSC_RUN_LOG_ERROR("select from VBFS_CONFIG failed, channel-id:%d.", m_nChannelID);

			return -1;
		}
		else
		{
			nFileSize = *vbfsCfg->m_fileSize;
			nFileSize <<= 30; //读取的数据以GB为单位
		}

		CTableWrapper< CCollectWrapper<CVbfsDeviceConfig> > vbfsDeviceCfg("VBFS_DEVICE_CONFIG");
		CVbfsNewDeviceCriterion deviceCriterion(m_nChannelID, m_nLastRawVbfsDeviceIndex);

		if (::PerSelect(vbfsDeviceCfg, database, dbConnection, &deviceCriterion))
		{
			DSC_RUN_LOG_ERROR("select from VBFS_DEVICE_CONFIG failed, channel-id:%d, last-rawvbfs-device-index:%d.", m_nChannelID, m_nLastRawVbfsDeviceIndex);

			return -1;
		}

		if (vbfsDeviceCfg->IsEmpty())
		{
			//没有新的磁盘可用
			DSC_RUN_LOG_ERROR("exist device full, and no new device can use, channel-id:%d, last-rawvbfs-device-index:%d.", m_nChannelID, m_nLastRawVbfsDeviceIndex);

			return -1;
		}

		VBFS::CVbfsConfigParam param;
		VBFS::IVbfsImpl* pVbfsImpl = nullptr;
		CVbfsDeviceConfig* pVbfsDeviceConfig = *vbfsDeviceCfg->begin();

		param.m_nFileSize = nFileSize;
		param.m_nFileNum = *pVbfsDeviceConfig->m_fileNum;

		if (m_strDeviceType == DEF_RAW_VBFS_TYPE)
		{
			DSC_NEW(pVbfsImpl, VBFS::CRawVbfs);
		}
		else
		{
			DSC_NEW(pVbfsImpl, VBFS::CFileVbfs);
		}

		if (pVbfsImpl->Open(*pVbfsDeviceConfig->m_deviceUrl, param))
		{
			DSC_RUN_LOG_ERROR("open %s failed.", pVbfsDeviceConfig->m_deviceUrl->c_str());
			this->Close();
			DSC_DELETE(pVbfsImpl);

			return -1;
		}

		if (m_nCurNum == m_nAllocNum)
		{
			m_nAllocNum *= 2;

			IVbfsImplPtr* ppRawVbfsPtr = (IVbfsImplPtr*)DSC_THREAD_SIZE_MALLOC(sizeof(IVbfsImplPtr) * m_nAllocNum);

			ACE_OS::memset(ppRawVbfsPtr, 0, sizeof(IVbfsImplPtr) * m_nAllocNum);
			ACE_OS::memcpy(ppRawVbfsPtr, m_pVbfsPtr, sizeof(IVbfsImplPtr) * m_nCurNum);
			DSC_THREAD_SIZE_FREE((char*)m_pVbfsPtr, sizeof(IVbfsImplPtr) * m_nCurNum);
			m_pVbfsPtr = ppRawVbfsPtr;
		}

		m_nLastRawVbfsDeviceIndex = *pVbfsDeviceConfig->m_deviceIndex;
		m_pVbfsPtr[m_nCurNum] = pVbfsImpl;
		++m_nCurNum;

		m_pLastVbfs = pVbfsImpl;

		return this->AllocatFile(nFileType, nFileID);
	}
	case VBFS::EN_VBFS_IO_DISK_ERROR:
	{
		return -1;
	}
	default:
	{
		ACE_ASSERT(false);

		return -1;
	}
	}
}
