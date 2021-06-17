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

		//��פ�ڴ�� �߼��ļ�ID -> �����ļ�ID ӳ���
		struct SLogicFileIndex
		{
			ACE_UINT32* m_pPhyFileIdx = nullptr; //�����±�Ϊ�߼��ļ�Index������Ϊ�����ļ�Index
			ACE_UINT32 m_nAllocNum;
			ACE_UINT32 m_nCurNum;
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
		void ResizeLogicFileIndex(SLogicFileIndex& rLogicFileIdx);

	private:
		ACE_HANDLE m_storageHandle = ACE_INVALID_HANDLE;
		ACE_UINT32 m_nStartFreePhyFileIdx; //û��ʹ�õĵ�һ�������ļ�index
		ACE_UINT64 m_nFirstPhyFileOffset = 0; //��1�������ļ��ڴ����ϵ�ƫ��
		SLogicFileIndex m_arrLogicFileIdx[EN_MAX_FILE_TYPE]; //�߼��ļ�����������פ�ڴ�
		SPhyFileIndex* m_pPhyFileIdx = nullptr;

		VBFS::CVbfsConfigParam m_fsCfgParam;
	};
}

#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs_raw.inl"

#endif
