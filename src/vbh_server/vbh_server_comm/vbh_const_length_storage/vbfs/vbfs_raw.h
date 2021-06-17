#ifndef VBFS_RAW_H_578942379423654325643265
#define VBFS_RAW_H_578942379423654325643265

#include "vbh_server_comm/vbh_server_comm_def_export.h"
#include "vbh_server_comm/vbh_const_length_storage/vbfs/i_vbfs_impl.h"

namespace VBFS
{
	class VBH_SERVER_COMM_DEF_EXPORT CRawVbfs final : public IVbfsImpl
	{
	private:
		enum
		{
			EN_STEP_ENLARGE_NUM = 1024
		};

		//常驻内存的 逻辑文件ID -> 物理文件ID 映射表
		struct SLogicFileIndex
		{
			ACE_UINT32* m_pPhyFileIdx = nullptr; //数组下标为逻辑文件Index，内容为物理文件Index
			ACE_UINT32 m_nAllocNum;
			ACE_UINT32 m_nCurNum;
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
		void ResizeLogicFileIndex(SLogicFileIndex& rLogicFileIdx);

	private:
		ACE_HANDLE m_storageHandle = ACE_INVALID_HANDLE;
		ACE_UINT32 m_nStartFreePhyFileIdx; //没有使用的第一个物理文件index
		ACE_UINT64 m_nFirstPhyFileOffset = 0; //第1个物理文件在磁盘上的偏移
		SLogicFileIndex m_arrLogicFileIdx[EN_MAX_FILE_TYPE]; //逻辑文件索引表，仅常驻内存
		SPhyFileIndex* m_pPhyFileIdx = nullptr;

		VBFS::CVbfsConfigParam m_fsCfgParam;
	};
}

#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs_raw.inl"

#endif
