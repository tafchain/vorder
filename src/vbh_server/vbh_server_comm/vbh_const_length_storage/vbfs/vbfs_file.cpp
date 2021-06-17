#include "ace/OS_NS_string.h"
#include "ace/OS_NS_dirent.h"
#include "ace/OS_NS_fcntl.h"

#include "dsc/dsc_log.h"

#include "vbh_server_comm/vbh_const_length_storage/vbh_cls_config_param.h"
#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs_file.h"

ACE_INT32 VBFS::CFileVbfs::Open(const CDscString& strPath, const VBFS::CVbfsConfigParam& param)
{
	ACE_ASSERT(!strPath.empty());

	m_strBasePath = strPath;
	m_strBasePath.Trim();
	if (m_strBasePath[m_strBasePath.size() - 1] != DSC_FILE_PATH_SPLIT_CHAR)
	{
		m_strBasePath += DSC_FILE_PATH_SPLIT_CHAR;
	}

	m_fsCfgParam = param;

	CDscString strTempPath;

	for (int i = 1; i < EN_MAX_FILE_TYPE; ++i)
	{
		strTempPath = m_strBasePath;
		strTempPath += i;

		ACE_DIR* dir = ACE_OS::opendir(strTempPath.c_str());//先打开文件
		if (NULL == dir)//判断是否打开成功
		{
			int nLastError = ACE_OS::last_error();

			DSC_RUN_LOG_ERROR("open dir error, dir path:%s, last error:%d, last error reason:%s.", strTempPath.c_str(), nLastError, ACE_OS::strerror(nLastError));

			return -1;
		}

		ACE_DIRENT* di = ACE_OS::readdir(dir);
		while (di)
		{
			if (strcmp(di->d_name, ".") && strcmp(di->d_name, ".."))
			{
				++m_typeFile[i].m_nMaxAllocatedFileID;
				++m_nTotalFileNum;
			}
			di = ACE_OS::readdir(dir);
		}

		ACE_OS::closedir(dir);
	}

	return 0;
}

void VBFS::CFileVbfs::Close(void)
{
	for (int i = 1; i < EN_MAX_FILE_TYPE; ++i)
	{
		CTypeFile& rTypeFile = m_typeFile[i];
		CFileHandleWraper* pFileHandle = rTypeFile.m_queueFileHandleWraper.PopFront();

		while (pFileHandle)
		{
			DSC::DscSafeCloseFile(pFileHandle->m_handle);
			DSC_THREAD_TYPE_DELETE(pFileHandle);
			pFileHandle = rTypeFile.m_queueFileHandleWraper.PopFront();
		}
	}
}

ACE_INT32 VBFS::CFileVbfs::Read(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID, char* pBuf, const ACE_UINT64 nBufSize, const ACE_UINT64 nBufOffset)
{
	ACE_ASSERT(nFileType > VBH_CLS::EN_INVALID_TABLE_TYPE && nFileType < EN_MAX_FILE_TYPE);
	ACE_ASSERT((nBufOffset + nBufSize) <= m_fsCfgParam.m_nFileSize);

	ACE_HANDLE h = GetFileHandle(nFileType, nFileID);

	if (h == ACE_INVALID_HANDLE)
	{
		DSC_RUN_LOG_ERROR("GetFileHandle failed, file-type:%u, file-id:%u", nFileType, nFileID);

		return -1;
	}
	else
	{
		if (-1 == ACE_OS::pread(h, pBuf, nBufSize, nBufOffset))
		{
			int nLastError = ACE_OS::last_error();

			DSC_RUN_LOG_ERROR("read file error, file id:%d, last error:%d, last error reason:%s, offset:%llu.", nFileID, nLastError, ACE_OS::strerror(nLastError), nBufOffset);

			return -1;
		}

		return 0;
	}
}

ACE_INT32 VBFS::CFileVbfs::Write(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID, const char* pBuf, const ACE_UINT64 nBufSize, const ACE_UINT64 nBufOffset)
{
	ACE_ASSERT(nFileType > VBH_CLS::EN_INVALID_TABLE_TYPE && nFileType < EN_MAX_FILE_TYPE);
	ACE_ASSERT((nBufOffset + nBufSize) <= m_fsCfgParam.m_nFileSize);

	ACE_HANDLE h = GetFileHandle(nFileType, nFileID);

	if (h == ACE_INVALID_HANDLE)
	{
		DSC_RUN_LOG_ERROR("GetFileHandle failed, file-type:%u, file-id:%u", nFileType, nFileID);

		return -1;
	}
	else
	{
		if (-1 == ACE_OS::pwrite(h, pBuf, nBufSize, nBufOffset))
		{
			int nLastError = ACE_OS::last_error();

			DSC_RUN_LOG_ERROR("read file error, file id:%d, last error:%d, last error reason:%s, offset:%llu.", nFileID, nLastError, ACE_OS::strerror(nLastError), nBufOffset);

			return -1;
		}

		return 0;
	}
}

ACE_INT32 VBFS::CFileVbfs::AllocatFile(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID)
{
	ACE_ASSERT(nFileType > VBH_CLS::EN_INVALID_TABLE_TYPE && nFileType < EN_MAX_FILE_TYPE);

	if (m_nTotalFileNum < m_fsCfgParam.m_nFileNum)
	{
		CDscString fileName(m_strBasePath);

		fileName += nFileType;
		fileName += DSC_FILE_PATH_SPLIT_CHAR;
		fileName += nFileID;

#if defined(WIN32) || defined(_IOS_PLATFORM_)
		int nMode = O_RDWR | O_BINARY | O_CREAT;
#else
		int nMode = O_RDWR | O_BINARY | O_DIRECT | O_SYNC | O_CREAT;
#endif

		ACE_HANDLE h = ACE_OS::open(fileName.c_str(), nMode);

		if (ACE_INVALID_HANDLE == h)
		{
			int nLastError = ACE_OS::last_error();

			DSC_RUN_LOG_ERROR("open file:%s failed, last error code:%d, last error reason:%s.", fileName.c_str(), nLastError, ACE_OS::strerror(nLastError));

			return -1;
		}

		if (-1 == ACE_OS::ftruncate(h, m_fsCfgParam.m_nFileSize))
		{
			int nLastError = ACE_OS::last_error();

			DSC_RUN_LOG_ERROR("open file:%s failed, last error code:%d, last error reason:%s.", fileName.c_str(), nLastError, ACE_OS::strerror(nLastError));
			ACE_OS::close(h);

			return -1;
		}

		InsertFileHandle(m_typeFile[nFileType], h, nFileID);

		++m_typeFile[nFileType].m_nMaxAllocatedFileID;
		m_nTotalFileNum++;

		return 0;
	}
	else
	{
		DSC_RUN_LOG_ERROR("allocat file overrun, path:%s, file-num:%d.", m_strBasePath.c_str(), m_fsCfgParam.m_nFileNum);

		return -1;
	}
}

ACE_UINT32 VBFS::CFileVbfs::GetAlocPhyFileNum(void)
{
	return m_nTotalFileNum;
}

ACE_UINT32 VBFS::CFileVbfs::GetMaxPhyFileNum(void)
{
	return m_fsCfgParam.m_nFileNum;
}

ACE_UINT32 VBFS::CFileVbfs::GetAlocLogicFileNum(const ACE_UINT32 nFileType)
{
	ACE_ASSERT(nFileType < EN_MAX_FILE_TYPE);

	return m_typeFile[nFileType].m_nMaxAllocatedFileID;
}

ACE_HANDLE VBFS::CFileVbfs::GetFileHandle(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID)
{
	CFileHandleWraper* pFileHandle = m_typeFile[nFileType].m_mapFileHandleWraper.Find(nFileID);

	if (pFileHandle)
	{
		return pFileHandle->m_handle;
	}
	else
	{
		CDscString fileName(m_strBasePath);

		fileName += nFileType;
		fileName += DSC_FILE_PATH_SPLIT_CHAR;
		fileName += nFileID;

#if defined(WIN32) || defined(_IOS_PLATFORM_)
		int nMode = O_RDWR | O_BINARY;
#else
		int nMode = O_RDWR | O_BINARY | O_DIRECT | O_SYNC;
#endif

		ACE_HANDLE hHandle = ACE_OS::open(fileName.c_str(), nMode);

		if (ACE_INVALID_HANDLE == hHandle)
		{
			int nLastError = ACE_OS::last_error();

			DSC_RUN_LOG_ERROR("open file:%s failed, last error code:%d, last error reason:%s.", fileName.c_str(), nLastError, ACE_OS::strerror(nLastError));
		}
		else
		{
			InsertFileHandle(m_typeFile[nFileType], hHandle, nFileID);
		}

		return hHandle;
	}
}

void VBFS::CFileVbfs::InsertFileHandle(CTypeFile& rTypeFile, const ACE_HANDLE hHandle, const ACE_UINT32 nFileID)
{
	CFileHandleWraper* pFileHandleWraper;

	if (rTypeFile.m_nOpenedFileNum < EN_MAX_OPEN_FILE_NUM)
	{
		pFileHandleWraper = DSC_THREAD_TYPE_NEW(CFileHandleWraper) CFileHandleWraper;
	}
	else
	{
		pFileHandleWraper = rTypeFile.m_queueFileHandleWraper.PopFront();
		rTypeFile.m_mapFileHandleWraper.Erase(pFileHandleWraper);

		DSC::DscSafeCloseFile(pFileHandleWraper->m_handle);
	}

	pFileHandleWraper->m_handle = hHandle;
	rTypeFile.m_mapFileHandleWraper.DirectInsert(nFileID, pFileHandleWraper);
	rTypeFile.m_queueFileHandleWraper.PushBack(pFileHandleWraper);
}
