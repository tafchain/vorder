#ifndef VBH__UPDATE_H_43978348321832563832
#define VBH__UPDATE_H_43978348321832563832

#include "ace/Shared_Memory_MM.h"

#include "dsc/mem_mng/dsc_stl_type.h"
#include "dsc/codec/dsc_codec/dsc_codec.h"

#include "vbh_server_comm/vbh_server_comm_def_export.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_record_table.h"

//TODO: ���еı�Ҫ�޸�Ϊʹ�ø�ʽ�����߸�ʽ��������Ŀ¼������ʱ�����н����ö�̬��cache����Ϣ
class VBH_SERVER_COMM_DEF_EXPORT CVbhUpdateTable  final : public CVbhRecordTable
{
private:
	class CModifyMemLogItem
	{
	public:
		DSC_BIND_ATTR(m_nRecordID, m_itemOriginalValue)

	public:
		ACE_UINT64 m_nRecordID;
		DSC::CDscBlob m_itemOriginalValue; //���item����ǰ��ԭʼ����
		CPage* m_pPage = nullptr;

	public:
		CModifyMemLogItem* m_pNext = nullptr;
	};

	class CModifyMemLog
	{
	public:
		DSC_BIND_ATTR(m_queueUpdateLog)

	public:
		CDscSizeInfoUnboundQueue<CModifyMemLogItem> m_queueUpdateLog;
	};

public:
	// strBasePathΪĿ¼��,
	virtual ACE_INT32 Open(VBFS::CVbfs* pVbfs, const ACE_UINT32 nChannelID, const ACE_UINT32 nTableType) override;

	virtual void Close(void) override;

	template<typename RECORD_TYPE>
	ACE_INT32 Append(const ACE_UINT64 nRecordID, RECORD_TYPE& rRecord);

	template<typename RECORD_TYPE>
	ACE_INT32 Update(const ACE_UINT64 nRecordID, RECORD_TYPE& rRecord);

	//�ѱ�����浽��־�У���ApplyModify֮ǰ������
	ACE_INT32 SaveToLog(void);
	ACE_INT32 Persistence(void);
	void CommitteTransaction(void);
	void RollbackCache(void);
	ACE_INT32 RollbackTransaction(void);
	ACE_INT32 RecoverFromLog(void);//����ʱ�����쳣�ع�

private:
	CModifyMemLog m_modifyMemLog; //�����־,�������ڴ��е�����item�����־
	dsc_list_type(CPage*) m_lstDirtyPage; //update����Ӱ�쵽��ҳ�� //���е�page�Ծɴ�����m_mapPage��

	//��־�ļ���ر��� //��־�ļ����ݣ���ǰ��¼����(���ǰ)|�������item��ID�Լ����ǰ���ݵ��б�
	CDscString m_strLogFilePath; //��־�ļ�·��
	ACE_Shared_Memory_MM m_shmLog; //��־�ļ��Ĺ����ڴ����
	char* m_pLogBuf = nullptr; //��־�ļ���Ӧ�Ĺ����ڴ滺����
	ACE_UINT32 m_nLogFilesize = 0; //��־�ļ��ĳ���
};

#include "vbh_server_comm/vbh_const_length_storage/vbh_update_table.inl"

#endif
