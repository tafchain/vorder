#include "vbh_server_comm/vbh_const_length_storage/vbh_cls_config_param.h"

static const char* const arrClsTableName[VBH_CLS::EN_WRITE_SET_HISTORY_TABLE_TYPE + 1] =
{
	"invalid",
	"write_set_version",
	"block_chain",
	"block_index",
	"write_set_index",
	"write_set_history"
};

const char* VBH_CLS::GetClsTableName(const ACE_UINT32 nTableType)
{
	if (nTableType > sizeof(arrClsTableName) / sizeof(arrClsTableName[0]))
	{
		return nullptr;
	}

	return arrClsTableName[nTableType];
}
