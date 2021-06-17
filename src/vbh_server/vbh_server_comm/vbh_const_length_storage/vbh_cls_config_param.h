#ifndef VBH_CLS_CONFIG_PARAM_H_8979543513546846541352
#define VBH_CLS_CONFIG_PARAM_H_8979543513546846541352

#include "dsc/codec/dsc_codec/dsc_codec.h"
#include "dsc/container/dsc_string.h"

#include "vbh_comm/vbh_encrypt_lib.h"
#include "vbh_comm/comm_msg_def/vbh_comm_class_def.h"
#include "vbh_server_comm/vbh_server_comm_def_export.h"

#define DEF_CLS_FORMATE_FILE_NAME "cls.fmt"
#define DEF_CLS_LOG_FILE_NAME "cls.log"
#define DEF_CLS_CONFIG_FILE_NAME "cls.cfg"

namespace VBH_CLS
{
	//�����洢�������
	enum
	{
		EN_INVALID_TABLE_TYPE = 0, //��Ч���ļ�����
		EN_WRITE_SET_VERSION_TABLE_TYPE, //order��ʹ�õ�user-versiont table
		EN_BLOCK_CHAIN_TABLE_TYPE, //peer������洢��
		EN_BLOCK_INDEX_TABLE_TYPE, //peer������������
		EN_WRITE_SET_INDEX_TABLE_TYPE, //peer��user-index table, ��user��word-state
		EN_WRITE_SET_HISTORY_TABLE_TYPE //peer��user-history table
	};
	//�����洢�������
	VBH_SERVER_COMM_DEF_EXPORT const char* GetClsTableName(const ACE_UINT32 nTableType);

	//�����洢�������
	//TODO

	//------------------------------�������д�ŵ����ݽṹ----------------------------
	//version tableʹ�õ�item
	class CVersionTableItem
	{
	public:
		enum 
		{
			EN_SIZE = sizeof(ACE_UINT32)
		};

	public:
		DSC_BIND_ATTR(m_nVersion);

	public:
		ACE_UINT32 m_nVersion; //�汾��
	};

	//write-set-index-table �еĽڵ�
	class CIndexTableItem
	{
	public:
		enum
		{
			EN_SIZE = VBH::CWriteSetUrl::EN_SIZE + sizeof(ACE_UINT32) * 2 + sizeof(ACE_UINT64)
		};

	public:
		DSC_BIND_ATTR(m_wsLatestUrl, m_nLatestVesion, m_nSequenceNumber4Verify, m_nPreHistTableIdx);

	public:
		VBH::CWriteSetUrl m_wsLatestUrl; //��Ӧд��������url
		ACE_UINT32 m_nLatestVesion; //�������ݵİ汾�ţ� //Ϊ�˼��ٷ��ʣ�����Ҫշת�������вſ��Ի�ȡ
		ACE_UINT32 m_nSequenceNumber4Verify; //У������ˮ�� //��ע���û�����������ȡ����
		ACE_UINT64 m_nPreHistTableIdx; //��һ��������hist-db�е���� 
	};

	//write-set-history-table �еĽڵ�
	class CHistoryTableItem
	{
	public:
		enum
		{
			EN_SIZE = VBH::CWriteSetUrl::EN_SIZE + sizeof(ACE_UINT64)
		};

	public:
		DSC_BIND_ATTR(m_wsUrl, m_nPreHistDBIdx);

	public:
		VBH::CWriteSetUrl m_wsUrl; //д����url
		ACE_UINT64 m_nPreHistDBIdx; //��һ��������hist-db�е����
	};

	class CBcIndexTableItem
	{
	public:
		enum
		{
			EN_SIZE = sizeof(ACE_UINT32) * 3 + sizeof(ACE_UINT64) + VBH_BLOCK_DIGEST_LENGTH
		};

	public:
		DSC_BIND_ATTR(m_nBlockDataLength, m_nTotalDataLength, m_nFileID, m_nOffset, m_preBlockHash);

	public:
		ACE_UINT32 m_nBlockDataLength; //������������ݳ���
		ACE_UINT32 m_nTotalDataLength; //д����������ݶ��������ݳ���
		ACE_UINT32 m_nFileID; //���������ļ�ID
		ACE_UINT64 m_nOffset; //�����������ļ��е�ƫ���� //����ƫ����
		CDscArray<char, VBH_BLOCK_DIGEST_LENGTH> m_preBlockHash;//ǰ1�����hashֵ,�ڱ�������ʱ�ౣ��һ�ݣ�����У��
	};

	//----------------------�����ļ�ϵͳ�ĸ�ʽ���������ݽṹ--------------------------------
	class VBH_SERVER_COMM_DEF_EXPORT SClsConfig
	{
	public:
		DSC_BIND_ATTR(m_nRecordLen, m_nPageRecordNum, m_nPageLen, m_nPageFileBits, m_strRawDevice);

	public:
		ACE_UINT32 m_nRecordLen = 0; //1����¼����
		ACE_UINT32 m_nPageRecordNum = 0; //1ҳ�м�¼�ĸ���
		ACE_UINT32 m_nPageLen = 0; //1ҳ����
		ACE_UINT32 m_nPageFileBits = 0; //ʹ���ļ�ʱ��1���ļ��е�ҳ�� 2^m_nPageFileBits
		CDscString m_strRawDevice; //ʹ�����豸ʱ�� ���豸��·��
	};

	enum
	{
		EN_CLS_LOG_FILE_BASE_SIZE = 1024 * 1024  // 1MB
	};
}

#endif
