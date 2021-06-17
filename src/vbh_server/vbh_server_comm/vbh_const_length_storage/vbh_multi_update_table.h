#ifndef VBH_MULTI_UPDATE_TABLE_H_87976435132165464321321
#define VBH_MULTI_UPDATE_TABLE_H_87976435132165464321321

#include "ace/Shared_Memory_MM.h"

#include "dsc/codec/dsc_codec/dsc_codec.h"

#include "vbh_server_comm/vbh_server_comm_def_export.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_record_table.h"

//TODO: ���еı�Ҫ�޸�Ϊʹ�ø�ʽ�����߸�ʽ��������Ŀ¼������ʱ�����н����ö�̬��cache����Ϣ
class VBH_SERVER_COMM_DEF_EXPORT CVbhMultiUpdateTable final : public CVbhRecordTable
{
protected:
	//��ʾ1�δ���ı��
	class CModifyPackage
	{
	public:
		enum
		{
			EN_MODIFY_PAGE_CACHE_HASH_MAP_BITS = 10
		};

		class CCodecPage //�����ڱ�����page
		{
		public:
			DSC_BIND_ATTR(m_bNewPage, m_nPageID, m_pageDatae);

		public:
			bool m_bNewPage; //����Ƿ�����ҳ
			ACE_UINT64 m_nPageID;
			DSC::CDscBlob m_pageDatae;
		};

	public:
		dsc_unordered_map_type(ACE_UINT64, CCodecPage) m_mapModifyPages; //�޸����漰����ҳ //����󿽱���ҳ, //���ڿ��ٲ���

	public: //ʹ��CDscDqueueʱ��������еĳ�Ա
		CModifyPackage* m_pNext = nullptr;
		CModifyPackage* m_pPrev = nullptr;
	};

public:
	// strBasePathΪĿ¼��, ·���������� '/'��β //strStoragePathΪ�洢���ݵ����豸����
	virtual ACE_INT32 Open(VBFS::CVbfs* pVbfs, const ACE_UINT32 nChannelID, const ACE_UINT32 nTableType) override;
	virtual void Close(void) override;

	//Լ���ϲ�ҵ��ID��1��ʼ���������룬�����пն�
	template<typename RECORD_TYPE>
	ACE_INT32 Append(const ACE_UINT64 nRecordID, RECORD_TYPE& rRecord);

	//Updateʱ:����ָ����Ŀ������ //Ŀǰ��ʵ��Լ����page cache�㹻�����������ڼ���ҳ���ᱻ����
	template<typename RECORD_TYPE>
	ACE_INT32 Update(const ACE_UINT64 nRecordID, RECORD_TYPE& rRecord);

	//�ع�δ����Ļ����� //�ع�δ���� PackModify ʱ�Ķ��Update����
	void RollbackUnpackCache(void);

	//����������еı���������仺���������Ա����������ύ������
	void PackModify(void);

	//�ѱ�����浽��־�У���ApplyModify֮ǰ������
	ACE_INT32 SaveToLog(void);
	ACE_INT32 Persistence(void);
	void CommitteTransaction(void);

	//������־��ָ������һ��
	ACE_INT32 RedoByLog(void);

protected:
	//��cache��ȡpage //���cache�в����ڣ����ļ�����
	virtual ACE_INT32 ReadPage(char* pPageContent, const ACE_UINT64 nPageID) override;

	//�ͷ�1��modify-package
	void FreeModifyPackage(CModifyPackage* pModifyPackage);

private:
	CDscDqueue<CModifyPackage> m_queueModifyPackage; //�޸�package����
	dsc_list_type(CPage*) m_lstCurDirtyPage; //��ǰupdate����Ӱ�쵽��ҳ�� //���е�page�Ծɴ�����m_mapPage��

	//��־�ļ���ر��� //��־�ļ����ݣ���ǰ��¼����(���ǰ)|�������item��ID�Լ����ǰ���ݵ��б�
	CDscString m_strLogFilePath; //��־�ļ�·��
	ACE_Shared_Memory_MM m_shmLog; //��־�ļ��Ĺ����ڴ����
	char* m_pLogBuf = nullptr; //��־�ļ���Ӧ�Ĺ����ڴ滺����
	ACE_UINT32 m_nLogFilesize = 0; //��־�ļ��ĳ���
};

#include "vbh_server_comm/vbh_const_length_storage/vbh_multi_update_table.inl"

#endif
