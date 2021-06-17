#ifndef I_VBFS_IMPL_H_8423183218DSUYYU2346678
#define I_VBFS_IMPL_H_8423183218DSUYYU2346678

#include "dsc/container/dsc_string.h"

#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs_comm.h"

#define DEF_RAW_VBFS_TYPE "raw"
#define DEF_FILE_SYSTEM_VBFS_TYPE "fs"

namespace VBFS
{
	class IVbfsImpl
	{
	public:
		virtual ~IVbfsImpl() {}

	public:
		// strDevPath 为存储用裸设备的完整路径
		virtual ACE_INT32 Open(const CDscString& strPath, const VBFS::CVbfsConfigParam& param) = 0;
		virtual void Close(void) = 0;

	public:
		//开辟1个逻辑文件 //nFileID为新开辟的逻辑文件ID
		virtual ACE_INT32 AllocatFile(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID) = 0;
		virtual ACE_INT32 Read(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID, char* pBuf, const size_t nBufSize, const size_t nBufOffset) = 0;
		virtual ACE_INT32 Write(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID, const char* pBuf, const size_t nBufSize, const size_t nBufOffset) = 0;

		//获取已经分配的 物理文件个数
		virtual ACE_UINT32 GetAlocPhyFileNum(void) = 0;

		//获取最多能分配的物理文件个数
		virtual ACE_UINT32 GetMaxPhyFileNum(void) = 0;

		//获取指定类型 已经分配的逻辑文件个数 最后1个文件ID
		virtual ACE_UINT32 GetAlocLogicFileNum(const ACE_UINT32 nFileType) = 0;
	};
}

#endif
