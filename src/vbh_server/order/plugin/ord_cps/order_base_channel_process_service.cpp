#include "ace/OS_NS_fcntl.h"
#include "ace/OS_NS_unistd.h"
#include "ace/OS_NS_sys_stat.h"
#include "ace/OS_NS_sys_socket.h"

#include "vbh_comm/vbh_comm_func.h"
#include "vbh_comm/vbh_comm_macro_def.h"
#include "vbh_comm/vbh_comm_error_code.h"
#include "vbh_comm/vbh_comm_wrap_msg_def.h"
#include "vbh_server_comm/vbh_const_length_storage/vbh_cls_config_param.h"

#include "ord_cps/order_base_channel_process_service.h"


CBaseChannelProcessService::CBaseChannelProcessService(const CDscString& strIpAddr, const ACE_INT32 nPort, const ACE_UINT32 nChannelID)
	: m_strIpAddr(strIpAddr)
	, m_nPort(nPort)
	, m_nChannelID(nChannelID)
{
}

ACE_INT32 CBaseChannelProcessService::OnInit(void)
{
	if (CDscHtsServerService::OnInit())
	{
		DSC_RUN_LOG_ERROR("channel process service init failed!");

		return -1;
	}

	//1. 读取数据库配置参数
	ACE_INT32 nOrderID;
	if (VBH::GetVbhProfileInt("ORDER_ID", nOrderID))
	{
		DSC_RUN_LOG_ERROR("read ORDER_ID failed.");
		return -1;
	}
	if (nOrderID < 0)
	{
		DSC_RUN_LOG_ERROR("ORDER_ID[%d] value invalid", nOrderID);
		return -1;
	}
	m_nOrderID = (ACE_UINT16)nOrderID;

	ACE_INT32 nPeerCount;
	if (VBH::GetVbhProfileInt("PEER_COUNT", nPeerCount))
	{
		DSC_RUN_LOG_ERROR("read PEER_COUNT failed.");
		return -1;
	}
	if (nPeerCount < 0)
	{
		DSC_RUN_LOG_ERROR("PEER_COUNT[%d] value invalid", nPeerCount);
		return -1;
	}
	m_nPeerCount = (ACE_UINT32)nPeerCount;

	//2. 加载配置文件和日志文件
	//合成文件路径 stroage/channel_x/order/ //创建路径时，必须以 '/' 结尾
	CDscString strBasePath(CDscAppManager::Instance()->GetWorkRoot());
	strBasePath += DSC_FILE_PATH_SPLIT_CHAR;
	strBasePath += DEF_STORAGE_DIR_NAME;
	strBasePath += DSC_FILE_PATH_SPLIT_CHAR;
	strBasePath += "channel_";
	strBasePath += m_nChannelID;
	strBasePath += DSC_FILE_PATH_SPLIT_CHAR;
	strBasePath += "order";
	strBasePath += DSC_FILE_PATH_SPLIT_CHAR;

	ACE_stat stat;
	CDscString strCfgFilePath(strBasePath);
	CDscString strLogFilePath(strBasePath);

	strCfgFilePath += DEF_ORDER_CPS_CONFIG_FILE_NAME;
	strLogFilePath += DEF_ORDER_CPS_LOG_FILE_NAME;

	if (-1 == ACE_OS::stat(strBasePath.c_str(), &stat))//如果路径不存在，则创建路径，同时创建相应的文件
	{
		CCpsConfig cfg; //初始配置
		CCpsConfigLog cfgLog; //初始日志

		::memset(cfg.m_blockHash, 0, sizeof(cfg.m_blockHash)); //初始时，清空hash值

		if (DSC::DscRecurMkdir(strBasePath.c_str(), strBasePath.size()))
		{
			DSC_RUN_LOG_ERROR("make-dir:%s failed.", strBasePath.c_str());
			return -1;
		}

		if (VBH::CreateCfgFile(strCfgFilePath, cfg))
		{
			DSC_RUN_LOG_ERROR("create file:%s failed.", strCfgFilePath.c_str());
			return -1;
		}

		if (VBH::CreateCfgFile(strLogFilePath, cfgLog))
		{
			DSC_RUN_LOG_ERROR("create file:%s failed.", strLogFilePath.c_str());
			return -1;
		}
	}

	if (VBH::LoadMmapCfgFile(strCfgFilePath, m_shmCpsCfg, m_pConfig))
	{
		return -1;
	}

	if (VBH::LoadMmapCfgFile(strLogFilePath, m_shmCpsLog, m_pCfgLog))
	{
		return -1;
	}

	//3. 打开版本号定长存储设备
	//3.1 打开vbfs
	if (m_vbfs.Open(m_nChannelID))
	{
		DSC_RUN_LOG_ERROR("open vbfs failed, channel-id:%u", m_nChannelID);

		return -1;
	}

	//3.2 打开write-set-version-table
	if (m_wsVersionTable.Open(&m_vbfs, m_nChannelID, VBH_CLS::EN_WRITE_SET_VERSION_TABLE_TYPE))
	{
		DSC_RUN_LOG_ERROR("user version table open falie, channel-id:%u", m_nChannelID);

		return -1;
	}

	//4. 开启hts服务
	m_pAcceptor = DSC_THREAD_TYPE_NEW(CMcpAsynchAcceptor<CBaseChannelProcessService>) CMcpAsynchAcceptor<CBaseChannelProcessService>(*this);
	if (m_pAcceptor->Open(m_nPort, m_strIpAddr.c_str()))
	{
		DSC_THREAD_TYPE_DEALLOCATE(m_pAcceptor);
		m_pAcceptor = NULL;
		DSC_RUN_LOG_ERROR("acceptor failed, ip addr:%s, port:%d", m_strIpAddr.c_str(), m_nPort);

		return -1;
	}
	else
	{
		this->RegistHandler(m_pAcceptor, ACE_Event_Handler::ACCEPT_MASK);
	}




	return 0;
}

ACE_INT32 CBaseChannelProcessService::OnExit(void)
{
	if (m_pAcceptor)
	{
		this->UnRegistHandler(m_pAcceptor, ACE_Event_Handler::ALL_EVENTS_MASK | ACE_Event_Handler::DONT_CALL);
		m_pAcceptor->ReleaseServiceHandler();
	}

	m_wsVersionTable.Close();
	m_vbfs.Close();

	m_shmCpsCfg.close();
	m_shmCpsLog.close();

	return CDscHtsServerService::OnExit();
}
