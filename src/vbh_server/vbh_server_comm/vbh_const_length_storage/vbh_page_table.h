#ifndef VBH_PAGE_TABLE_83429872341861237123321123713256
#define VBH_PAGE_TABLE_83429872341861237123321123713256

#include "dsc/container/dsc_string.h"

#include "vbh_server_comm/vbh_server_comm_def_export.h"
#include "vbh_server_comm/vbh_const_length_storage/vbfs/vbfs.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_cls_config_param.h"

class VBH_SERVER_COMM_DEF_EXPORT CVbhPageTable
{
public:
	CVbhPageTable(VBFS::CVbfs* pVbfs, const ACE_UINT32 nTableType, const ACE_UINT32 nPageSize, const ACE_UINT32 nFilePageNum);

public:
	//���ļ���ȡ1ҳ //nPageID��0��ʼ�������
	ACE_INT32 ReadPage(char* pPage, const ACE_UINT64 nPageID);
	//д��1ҳ���ļ� //nPageID��0��ʼ�������
	ACE_INT32 WritePage(const char* pPage, const ACE_UINT64 nPageID);
	ACE_INT32 WriteNewPage(const char* pPage, const ACE_UINT64 nPageID);

private:
	//����PageID��ȡFileID��FileID��0��ʼ���
	ACE_UINT64 GetFileID(const ACE_UINT64 nPageID) const;
	ACE_UINT64 GetOffset(const ACE_UINT64 nPageID) const;

private:
	VBFS::CVbfs* m_pVbfs; //
	const ACE_UINT32 m_nTableType;
	const ACE_UINT32 m_nPageSize; //1ҳ��С
	const ACE_UINT32 m_nFilePageNum; //1���ļ��е�ҳ��
};

#include "vbh_server_comm/vbh_const_length_storage/vbh_page_table.inl"

#endif
