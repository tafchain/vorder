#ifndef VBFS_FILE_H_589764685161649418955265
#define VBFS_FILE_H_589764685161649418955265

#include "dsc/container/bare_hash_map.h"

#include "vbh_server_comm/vbh_server_comm_def_export.h"
#include "vbh_server_comm/vbh_const_length_storage/vbfs/i_vbfs_impl.h"
#include "vbh_server_comm/vbh_unite_dqueue.h"

namespace VBFS
{
	//思路：格式化磁盘后，要创建0~15的文件夹
	class VBH_SERVER_COMM_DEF_EXPORT CFileVbfs final : public IVbfsImpl
	{
	private:
		enum
		{
			EN_FILE_HANDLE_HASH_MAP_BITS = 6, //page-cache的规模
			EN_MAX_OPEN_FILE_NUM = 1 << 6
		};

		class CFileHandleWraper
		{
		public:
			ACE_HANDLE m_handle = ACE_INVALID_HANDLE;

		public: //CBareHashMap
			ACE_UINT32 m_nKey = 0; //key值就是fileID值
			CFileHandleWraper* m_pPrev = nullptr;
			CFileHandleWraper* m_pNext = nullptr;

		public: //作为CVbhUniteDqueue的元素需要的成员变量
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
		// strDevPath 为存储用裸设备的完整路径
		ACE_INT32 Open(const CDscString& strPath, const VBFS::CVbfsConfigParam& param) override;
		void Close(void) override;

	public:
		ACE_INT32 AllocatFile(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID) override;
		ACE_INT32 Read(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID, char* pBuf, const ACE_UINT64 nBufSize, const ACE_UINT64 nBufOffset) override;
		ACE_INT32 Write(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID, const char* pBuf, const ACE_UINT64 nBufSize, const ACE_UINT64 nBufOffset) override;

	public:
		//获取已经分配的 物理文件个数
		ACE_UINT32 GetAlocPhyFileNum(void) override;

		//获取最多能分配的物理文件个数
		ACE_UINT32 GetMaxPhyFileNum(void) override;

		//获取指定类型 已经分配的逻辑文件个数 最后1个文件ID
		ACE_UINT32 GetAlocLogicFileNum(const ACE_UINT32 nFileType) override;

	private:
		ACE_HANDLE GetFileHandle(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID);

		void InsertFileHandle(CTypeFile& rTypeFile, const ACE_HANDLE hHandle, const ACE_UINT32 nFileID);

	private:
		CDscString m_strBasePath; //基础路径
		VBFS::CVbfsConfigParam m_fsCfgParam;
		CTypeFile m_typeFile[EN_MAX_FILE_TYPE];
		ACE_UINT32 m_nTotalFileNum = 0;
	};
}

#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs_file.inl"

#endif
