#ifndef VBH_APPEND_H_5872349721682135623456
#define VBH_APPEND_H_5872349721682135623456

#include "ace/Shared_Memory_MM.h"

#include "dsc/mem_mng/dsc_stl_type.h"

#include "vbh_server_comm/vbh_server_comm_def_export.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_record_table.h"

class VBH_SERVER_COMM_DEF_EXPORT CVbhAppendTable : public CVbhRecordTable
{
private:
	class CAppendTableConfig
	{
	public:
		ACE_UINT64 m_nLastRecordID = 1; //ϵͳ�е�һ������ID //δ�������Сֵ
	};

	class CAppendTableLog
	{
	public:
		ACE_UINT64 m_nLastRecordID;
	};

public:
	// strPathΪĿ¼��, ·���������� '/'��β
	virtual ACE_INT32 Open(VBFS::CVbfs* pVbfs, const ACE_UINT32 nChannelID, const ACE_UINT32 nTableType) override;
	virtual void Close(void) override;

	template<typename RECORD_TYPE>
	ACE_INT32 Append(ACE_UINT64& nRecordID, RECORD_TYPE& rRecord);

	//�������1��RrecodeID��Ӧ�ļ�¼
	void PopBack(const ACE_UINT64 nRecordID);

	//�ѱ�����浽��־�У���ApplyModify֮ǰ������
	ACE_INT32 SaveToLog(void);
	ACE_INT32 Persistence(void);
	void CommitteTransaction(void);
	void RollbackCache(void);
	ACE_INT32 RollbackTransaction(void);
	ACE_INT32 RecoverFromLog(void);//����ʱ�����쳣�ع�

private:
	ACE_UINT64 m_nLastRecordID; //ϵͳ�е�һ������ID //δ�������Сֵ
	CPage* m_pAppendPage = nullptr;
	dsc_deque_type(CPage*) m_queueAppendPage; //appendʱ����������ݣ������ݣ���Page //m_bNewPage=false��page��m_mapPage�У���������
	ACE_Shared_Memory_MM m_shmClsCtrlInfo;
	CAppendTableConfig* m_pCfg = nullptr;

	CDscString m_strLogFilePath; //��־�ļ�·��
	ACE_Shared_Memory_MM m_shmLog; //��־�ļ��Ĺ����ڴ����
	CAppendTableLog* m_pLog = nullptr;//��־�ļ���Ӧ�Ĺ����ڴ滺����
};

#include "vbh_server_comm/vbh_const_length_storage/vbh_append_table.inl"

#endif
