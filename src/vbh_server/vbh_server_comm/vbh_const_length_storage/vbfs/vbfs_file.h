#ifndef VBFS_FILE_H_589764685161649418955265
#define VBFS_FILE_H_589764685161649418955265

#include "dsc/container/bare_hash_map.h"

#include "vbh_server_comm/vbh_server_comm_def_export.h"
#include "vbh_server_comm/vbh_const_length_storage/vbfs/i_vbfs_impl.h"
#include "vbh_server_comm/vbh_unite_dqueue.h"

namespace VBFS
{
	//˼·����ʽ�����̺�Ҫ����0~15���ļ���
	class VBH_SERVER_COMM_DEF_EXPORT CFileVbfs final : public IVbfsImpl
	{
	private:
		enum
		{
			EN_FILE_HANDLE_HASH_MAP_BITS = 6, //page-cache�Ĺ�ģ
			EN_MAX_OPEN_FILE_NUM = 1 << 6
		};

		class CFileHandleWraper
		{
		public:
			ACE_HANDLE m_handle = ACE_INVALID_HANDLE;

		public: //CBareHashMap
			ACE_UINT32 m_nKey = 0; //keyֵ����fileIDֵ
			CFileHandleWraper* m_pPrev = nullptr;
			CFileHandleWraper* m_pNext = nullptr;

		public: //��ΪCVbhUniteDqueue��Ԫ����Ҫ�ĳ�Ա����
			CFileHandleWraper* m_pDqueuePrev = nullptr;
			CFileHandleWraper* m_pDqueueNext = nullptr;
		};

		using file_handle_wraper_map_type = CBareHashMap< ACE_UINT32, CFileHandleWraper, EN_FILE_HANDLE_HASH_MAP_BITS>; //handleID -> file handle
		using file_handle_wraper_queue_type = CVbhUniteDqueue<CFileHandleWraper>;

		class CTypeFile
		{
		public:
			file_handle_wraper_map_type m_mapFileHandleWraper;
			file_handle_wraper_queue_type m_queueFileHandleWraper;
			ACE_UINT32 m_nMaxAllocatedFileID = 0;
			ACE_UINT32 m_nOpenedFileNum = 0;
		};

	public:
		// strDevPath Ϊ�洢�����豸������·��
		ACE_INT32 Open(const CDscString& strPath, const VBFS::CVbfsConfigParam& param) override;
		void Close(void) override;

	public:
		ACE_INT32 AllocatFile(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID) override;
		ACE_INT32 Read(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID, char* pBuf, const ACE_UINT64 nBufSize, const ACE_UINT64 nBufOffset) override;
		ACE_INT32 Write(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID, const char* pBuf, const ACE_UINT64 nBufSize, const ACE_UINT64 nBufOffset) override;

	public:
		//��ȡ�Ѿ������ �����ļ�����
		ACE_UINT32 GetAlocPhyFileNum(void) override;

		//��ȡ����ܷ���������ļ�����
		ACE_UINT32 GetMaxPhyFileNum(void) override;

		//��ȡָ������ �Ѿ�������߼��ļ����� ���1���ļ�ID
		ACE_UINT32 GetAlocLogicFileNum(const ACE_UINT32 nFileType) override;

	private:
		ACE_HANDLE GetFileHandle(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID);

		void InsertFileHandle(CTypeFile& rTypeFile, const ACE_HANDLE hHandle, const ACE_UINT32 nFileID);

	private:
		CDscString m_strBasePath; //����·��
		VBFS::CVbfsConfigParam m_fsCfgParam;
		CTypeFile m_typeFile[EN_MAX_FILE_TYPE];
		ACE_UINT32 m_nTotalFileNum = 0;
	};
}

#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs_file.inl"

#endif
