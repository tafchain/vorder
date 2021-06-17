#ifndef VBFS_COMM_H_784238793428231468432165432
#define VBFS_COMM_H_784238793428231468432165432

#include "ace/Basic_Types.h"

namespace VBFS
{
	enum
	{
		EN_MAX_FILE_TYPE = 8,
		EN_DIRET_IO_PHY_FILE_INDEX_NUM = 64   //64*sizeof(SPhyFileIndex) ==512
	};

	enum
	{
		EN_VBFS_IO_OK = 0,
		EN_VBFS_IO_DISK_FULL = 1,
		EN_VBFS_IO_DISK_ERROR = -1,
		EN_VBFS_IO_LOGIC_ERROR = -2
	};

	//存储空间划分为“文件”分别管理
	class CVbfsConfigParam
	{
	public:
		ACE_UINT32 m_nFileNum;//物理文件个数
		ACE_UINT64 m_nFileSize;//物理文件大小，//格式化时，推荐大小以G为单位
	};

	//物理文件的索引 //记录某个物理文件用于 x 种类型的逻辑文件，逻辑文件ID为多少
	struct SPhyFileIndex
	{
		ACE_UINT32 m_nLogicFileType;//逻辑文件类型，//0保留字，表示未分配文件
		ACE_UINT32 m_nLogicFileID;//逻辑文件编号
	};
}

#endif
