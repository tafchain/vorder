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

	//�洢�ռ仮��Ϊ���ļ����ֱ����
	class CVbfsConfigParam
	{
	public:
		ACE_UINT32 m_nFileNum;//�����ļ�����
		ACE_UINT64 m_nFileSize;//�����ļ���С��//��ʽ��ʱ���Ƽ���С��GΪ��λ
	};

	//�����ļ������� //��¼ĳ�������ļ����� x �����͵��߼��ļ����߼��ļ�IDΪ����
	struct SPhyFileIndex
	{
		ACE_UINT32 m_nLogicFileType;//�߼��ļ����ͣ�//0�����֣���ʾδ�����ļ�
		ACE_UINT32 m_nLogicFileID;//�߼��ļ����
	};
}

#endif
