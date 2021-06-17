#ifndef VBFS_H_432978423764327FDWYIFHJ3427423187
#define VBFS_H_432978423764327FDWYIFHJ3427423187

//#include "dsc/mem_mng/dsc_stl_type.h"

#include "vbh_server_comm/vbh_server_comm_def_export.h"
#include "vbh_server_comm/vbh_const_length_storage/vbfs/i_vbfs_impl.h"

namespace VBFS
{
	class VBH_SERVER_COMM_DEF_EXPORT CVbfs
	{
	private:
		using IVbfsImplPtr = IVbfsImpl*;

	public:
		// strDevPath 为存储用裸设备的完整路径
		ACE_INT32 Open(const ACE_UINT32 nChannelID);
		void Close(void);

	public:
		ACE_INT32 AllocatFile(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID);
		ACE_INT32 Read(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID, char* pBuf, const ACE_UINT64 nBufSize, const ACE_UINT64 nBufOffset);
		ACE_INT32 Write(const ACE_UINT32 nFileType, const ACE_UINT32 nFileID, const char* pBuf, const ACE_UINT64 nBufSize, const ACE_UINT64 nBufOffset);

	private:
		IVbfsImplPtr m_pLastVbfs = nullptr; //最后1个raw-vbfs
		IVbfsImplPtr* m_pVbfsPtr = nullptr;
		ACE_UINT32 m_nAllocNum = 0;
		ACE_UINT32 m_nCurNum = 0;
		ACE_INT32 m_nLastRawVbfsDeviceIndex = 0; //保存最后1个raw-vbfs的index
		ACE_UINT32 m_nChannelID = 0;
		CDscString m_strDeviceType; //设备类型
	};
}

#endif
