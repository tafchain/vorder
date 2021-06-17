#include "ace/OS_NS_string.h"

#include "dsc/dsc_log.h"

#include "vbh_server_comm/vbh_const_length_storage/vbh_cls_config_param.h"
#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs_raw.h"

ACE_INT32 VBFS::CRawVbfs::Open(const CDscString& strPath, const VBFS::CVbfsConfigParam& param)
{
	//TODO: 在windows下使用大文件模拟裸设备，可以方便测试
#if defined(WIN32) || defined(_IOS_PLATFORM_)
	DSC_RUN_LOG_ERROR("windows ios not support raw storage.");

	return -1;
#else
	int nMode = O_RDWR | O_BINARY | O_DIRECT | O_SYNC;

	m_storageHandle = ACE_OS::open(strPath.c_str(), nMode);
	if (ACE_INVALID_HANDLE == m_storageHandle)
	{
		int nLastError = ACE_OS::last_error();

		DSC_RUN_LOG_ERROR("open file:%s failed, last error code:%d, last error reason:%s.", strPath.c_str(), nLastError, ACE_OS::strerror(nLastError));

		return -1;
	}
	m_fsCfgParam = param;

	//512对齐
	const ACE_UINT32 nAlignSize = DSC_ROUND_UP(sizeof(SPhyFileIndex) * param.m_nFileNum, DEF_MEM_ALIGN_SIZE);
	SPhyFileIndex* pPhyFileIdx;

	DSC_MEM_ALIGN(pPhyFileIdx, nAlignSize);
	if (!pPhyFileIdx)
	{
		DSC_RUN_LOG_ERROR("malloc vbfs SPhyFileIndex failed, align size:%d.", nAlignSize);
		ACE_OS::close(m_storageHandle);

		return -1;
	}

	if (-1 == ACE_OS::pread(m_storageHandle, pPhyFileIdx, nAlignSize, 0))
	{
		int nLastError = ACE_OS::last_error();

		DSC_RUN_LOG_ERROR("read storage m_sVbfsHead failed, last error:%d, last error reason:%s.", nLastError, ACE_OS::strerror(nLastError));
		ACE_OS::close(m_storageHandle);
		ACE_OS::free(pPhyFileIdx);

		return -1;
	}

	m_nFirstPhyFileOffset = nAlignSize;
	::memset(m_arrLogicFileIdx, 0, sizeof(SLogicFileIndex) * EN_MAX_FILE_TYPE);

	for (ACE_UINT32 i = 0; i < param.m_nFileNum; ++i)
	{
		if (pPhyFileIdx[i].m_nLogicFileType >= EN_MAX_FILE_TYPE)
		{
			DSC_RUN_LOG_ERROR("logic file type%d error, cann't more than:%d.", pPhyFileIdx[i].m_nLogicFileType, EN_MAX_FILE_TYPE);
			ACE_OS::close(m_storageHandle);
			ACE_OS::free(pPhyFileIdx);

			return -1;
		}

		if (pPhyFileIdx[i].m_nLogicFileType != VBH_CLS::EN_INVALID_TABLE_TYPE)
		{
			SLogicFileIndex& rLogicFileIdx = m_arrLogicFileIdx[pPhyFileIdx[i].m_nLogicFileType];

			ResizeLogicFileIndex(rLogicFileIdx);
			rLogicFileIdx.m_pPhyFileIdx[rLogicFileIdx.m_nCurNum] = i;//赋上物理fileID
			++rLogicFileIdx.m_nCurNum;
		}
		else
		{
			m_nStartFreePhyFileIdx = i;
			break;
		}
	}

	const ACE_UINT32 n = DSC_ROUND_DOWN(m_nStartFreePhyFileIdx, EN_DIRET_IO_PHY_FILE_INDEX_NUM);

	DSC_MEM_ALIGN(m_pPhyFileIdx, sizeof(SPhyFileIndex) * EN_DIRET_IO_PHY_FILE_INDEX_NUM);
	ACE_OS::memcpy(m_pPhyFileIdx, &pPhyFileIdx[n], sizeof(SPhyFileIndex) * EN_DIRET_IO_PHY_FILE_INDEX_NUM);
	ACE_OS::free(pPhyFileIdx);

	return 0;
#endif
}

void VBFS::CRawVbfs::Close(void)
{
	if (m_pPhyFileIdx)
	{
		ACE_OS::free(m_pPhyFileIdx);
	}

	for (int i = 0; i < EN_MAX_FILE_TYPE; ++i)
	{
		if (m_arrLogicFileIdx[i].m_pPhyFileIdx)
		{
			ACE_OS::free(m_arrLogicFileIdx[i].m_pPhyFileIdx);
			m_arrLogicFileIdx[i].m_pPhyFileIdx = NULL;
		}
	}

	ACE_OS::close(m_storageHandle);
}

ACE_INT32 VBFS::CRawVbfs::Read(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID, char* pBuf, const ACE_UINT64 nBufSize, const ACE_UINT64 nBufOffset)
{
	ACE_ASSERT(nFileType > VBH_CLS::EN_INVALID_TABLE_TYPE && nFileType < EN_MAX_FILE_TYPE);
	ACE_ASSERT((nBufOffset + nBufSize) <= m_fsCfgParam.m_nFileSize);

	//nFileID是从0开始编号，m_nCurNum是从1开始编号
	if (DSC_LIKELY(nFileID < m_arrLogicFileIdx[nFileType].m_nCurNum))
	{
		const ACE_OFF_T nOffset = m_nFirstPhyFileOffset + m_fsCfgParam.m_nFileSize * m_arrLogicFileIdx[nFileType].m_pPhyFileIdx[nFileID] + nBufOffset;

		if (-1 == ACE_OS::pread(m_storageHandle, pBuf, nBufSize, nOffset))
		{
			int nLastError = ACE_OS::last_error();

			DSC_RUN_LOG_ERROR("read raw file error, file id:%d, last error:%d, last error reason:%s, offset:%llu.", nFileID, nLastError, ACE_OS::strerror(nLastError), nOffset);

			return -1;
		}

		return 0;
	}
	else
	{
		DSC_RUN_LOG_ERROR("read raw file error, file id:%d, current file number:%d", nFileID, m_arrLogicFileIdx[nFileType].m_nCurNum);

		return -1;
	}
}

ACE_INT32 VBFS::CRawVbfs::Write(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID, const char* pBuf, const ACE_UINT64 nBufSize, const ACE_UINT64 nBufOffset)
{
	ACE_ASSERT(nFileType > VBH_CLS::EN_INVALID_TABLE_TYPE && nFileType < EN_MAX_FILE_TYPE);
	ACE_ASSERT((nBufOffset + nBufSize) <= m_fsCfgParam.m_nFileSize);

	//nFileID是从0开始编号，m_nCurNum是从1开始编号
	if (nFileID < m_arrLogicFileIdx[nFileType].m_nCurNum)
	{
		const ACE_OFF_T nOffset = m_nFirstPhyFileOffset + m_fsCfgParam.m_nFileSize * m_arrLogicFileIdx[nFileType].m_pPhyFileIdx[nFileID] + nBufOffset;

		if (-1 == ACE_OS::pwrite(m_storageHandle, pBuf, nBufSize, nOffset))
		{
			int nLastError = ACE_OS::last_error();

			DSC_RUN_LOG_ERROR("write raw file error, file id:%d, last error:%d, last error reason:%s, offset:%llu.", nFileID, nLastError, ACE_OS::strerror(nLastError), nOffset);

			return -1;
		}

		return 0;
	}
	else
	{
		DSC_RUN_LOG_ERROR("Write raw file error, file id:%d, current file number:%d", nFileID, m_arrLogicFileIdx[nFileType].m_nCurNum);

		return -1;
	}
}

ACE_INT32 VBFS::CRawVbfs::AllocatFile(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID)
{
	ACE_ASSERT(nFileType > VBH_CLS::EN_INVALID_TABLE_TYPE && nFileType < EN_MAX_FILE_TYPE);

	if (m_nStartFreePhyFileIdx < m_fsCfgParam.m_nFileNum)
	{
		SLogicFileIndex& rLogicFileIdx = m_arrLogicFileIdx[nFileType];

		if (nFileID == rLogicFileIdx.m_nCurNum)
		{
			ResizeLogicFileIndex(rLogicFileIdx);
			rLogicFileIdx.m_pPhyFileIdx[rLogicFileIdx.m_nCurNum] = m_nStartFreePhyFileIdx;//赋上物理file-index
			++rLogicFileIdx.m_nCurNum;

			const ACE_UINT32 nRoundDownFileID = DSC_ROUND_DOWN(m_nStartFreePhyFileIdx, EN_DIRET_IO_PHY_FILE_INDEX_NUM);
			const ACE_UINT32 nIndex = m_nStartFreePhyFileIdx - nRoundDownFileID;

			if (nIndex)
			{
				m_pPhyFileIdx[nIndex].m_nLogicFileID = nFileID;
				m_pPhyFileIdx[nIndex].m_nLogicFileType = nFileType;
			}
			else
			{
				memset(m_pPhyFileIdx, 0, sizeof(SPhyFileIndex) * EN_DIRET_IO_PHY_FILE_INDEX_NUM);
				m_pPhyFileIdx[0].m_nLogicFileID = nFileID;
				m_pPhyFileIdx[0].m_nLogicFileType = nFileType;
			}

			++m_nStartFreePhyFileIdx;

			if (-1 == ACE_OS::pwrite(m_storageHandle, m_pPhyFileIdx, sizeof(SPhyFileIndex) * EN_DIRET_IO_PHY_FILE_INDEX_NUM, sizeof(SPhyFileIndex) * nRoundDownFileID))
			{
				int nLastError = ACE_OS::last_error();

				DSC_RUN_LOG_ERROR("write file id error, Logic file id:%d, , Logic file type:%d, last error:%d, last error reason:%s, file size:%d, offset:%d.", nFileID, nFileType, nLastError, ACE_OS::strerror(nLastError), sizeof(SPhyFileIndex) * EN_DIRET_IO_PHY_FILE_INDEX_NUM, sizeof(SPhyFileIndex) * nRoundDownFileID);

				return VBFS::EN_VBFS_IO_DISK_ERROR;
			}
		}
		else if (nFileID < rLogicFileIdx.m_nCurNum)
		{
			DSC_RUN_LOG_INFO("repeat alloc file req, Logic file id:%d, Logic file type:%d.", nFileID, nFileType);
		}
		else
		{
			DSC_RUN_LOG_ERROR("file id error, logic file id:%d, logic file type:%d, cur max file id:%d.", nFileID, nFileType, rLogicFileIdx.m_nCurNum);
			ACE_ASSERT(false);

			return VBFS::EN_VBFS_IO_LOGIC_ERROR;//避开上层以为是卷满
		}

		return VBFS::EN_VBFS_IO_OK;
	}
	else
	{
		DSC_RUN_LOG_ERROR("disk is full, start-free-phyfile-idx:%d, filenum:%d", m_nStartFreePhyFileIdx, m_fsCfgParam.m_nFileNum);

		return VBFS::EN_VBFS_IO_DISK_FULL;//避开上层以为是卷满
	}

}

void VBFS::CRawVbfs::ResizeLogicFileIndex(SLogicFileIndex& rLogicFileIdx)
{
	if (rLogicFileIdx.m_pPhyFileIdx)
	{
		if (rLogicFileIdx.m_nCurNum == rLogicFileIdx.m_nAllocNum)
		{
			rLogicFileIdx.m_nAllocNum += EN_STEP_ENLARGE_NUM;

			ACE_UINT32* pFileID = (ACE_UINT32* )::malloc(sizeof(ACE_UINT32) * rLogicFileIdx.m_nAllocNum);
			::memset(pFileID, 0, sizeof(ACE_UINT32) * rLogicFileIdx.m_nAllocNum);

			::memcpy(pFileID, rLogicFileIdx.m_pPhyFileIdx, sizeof(ACE_UINT32) * rLogicFileIdx.m_nCurNum);
			::free(rLogicFileIdx.m_pPhyFileIdx);
			rLogicFileIdx.m_pPhyFileIdx = pFileID;
		}
	}
	else
	{
		rLogicFileIdx.m_nCurNum = 0;
		rLogicFileIdx.m_nAllocNum = EN_STEP_ENLARGE_NUM;
		rLogicFileIdx.m_pPhyFileIdx = (ACE_UINT32*)::malloc(sizeof(ACE_UINT32) * EN_STEP_ENLARGE_NUM);
		::memset(rLogicFileIdx.m_pPhyFileIdx, 0, sizeof(ACE_UINT32) * EN_STEP_ENLARGE_NUM);
	}
}
