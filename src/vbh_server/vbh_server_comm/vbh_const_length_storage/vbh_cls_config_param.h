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
	//定长存储表的类型
	enum
	{
		EN_INVALID_TABLE_TYPE = 0, //无效的文件类型
		EN_WRITE_SET_VERSION_TABLE_TYPE, //order端使用的user-versiont table
		EN_BLOCK_CHAIN_TABLE_TYPE, //peer端区块存储表
		EN_BLOCK_INDEX_TABLE_TYPE, //peer端区块索引表
		EN_WRITE_SET_INDEX_TABLE_TYPE, //peer端user-index table, 即user的word-state
		EN_WRITE_SET_HISTORY_TABLE_TYPE //peer端user-history table
	};
	//定长存储表的名称
	VBH_SERVER_COMM_DEF_EXPORT const char* GetClsTableName(const ACE_UINT32 nTableType);

	//定长存储表的名称
	//TODO

	//------------------------------各个表中存放的数据结构----------------------------
	//version table使用的item
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
		ACE_UINT32 m_nVersion; //版本号
	};

	//write-set-index-table 中的节点
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
		VBH::CWriteSetUrl m_wsLatestUrl; //对应写集的最新url
		ACE_UINT32 m_nLatestVesion; //最新数据的版本号； //为了加速访问，否则要辗转到区块中才可以获取
		ACE_UINT32 m_nSequenceNumber4Verify; //校验用流水号 //从注册用户的事务上提取出来
		ACE_UINT64 m_nPreHistTableIdx; //上一条数据在hist-db中的序号 
	};

	//write-set-history-table 中的节点
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
		VBH::CWriteSetUrl m_wsUrl; //写集的url
		ACE_UINT64 m_nPreHistDBIdx; //上一条数据在hist-db中的序号
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
		ACE_UINT32 m_nBlockDataLength; //纯粹的区块数据长度
		ACE_UINT32 m_nTotalDataLength; //写入的区块数据对齐后的数据长度
		ACE_UINT32 m_nFileID; //存放区块的文件ID
		ACE_UINT64 m_nOffset; //区块数据在文件中的偏移量 //最终偏移量
		CDscArray<char, VBH_BLOCK_DIGEST_LENGTH> m_preBlockHash;//前1区块的hash值,在保存区块时多保存一份，方便校验
	};

	//----------------------基于文件系统的格式化工具数据结构--------------------------------
	class VBH_SERVER_COMM_DEF_EXPORT SClsConfig
	{
	public:
		DSC_BIND_ATTR(m_nRecordLen, m_nPageRecordNum, m_nPageLen, m_nPageFileBits, m_strRawDevice);

	public:
		ACE_UINT32 m_nRecordLen = 0; //1条记录长度
		ACE_UINT32 m_nPageRecordNum = 0; //1页中记录的个数
		ACE_UINT32 m_nPageLen = 0; //1页长度
		ACE_UINT32 m_nPageFileBits = 0; //使用文件时，1个文件中的页数 2^m_nPageFileBits
		CDscString m_strRawDevice; //使用裸设备时， 裸设备的路径
	};

	enum
	{
		EN_CLS_LOG_FILE_BASE_SIZE = 1024 * 1024  // 1MB
	};
}

#endif
