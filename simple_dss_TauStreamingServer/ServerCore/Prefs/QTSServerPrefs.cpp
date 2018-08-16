/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSServerPrefs.cpp
Description: A object that stores for RTSP server preferences.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 



#include "QTSServerPrefs.h"
#include "QTSSDataConverter.h"
#include "QTSSRollingLog.h"
#include "MyAssert.h"
#include "OSMemory.h"
#include "defaultPaths.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

 
//rtsp_ports����������˿�Ĭ��ֵ,��554����
char* QTSServerPrefs::sAdditionalDefaultPorts[] =
{
    "7070",
	"8000",
	"8001",
    NULL
};

char* QTSServerPrefs::sRTP_Header_Players[] =
{
	"Real",
    NULL
};

char* QTSServerPrefs::sAdjust_Bandwidth_Players[] =
{
	"Real",
    NULL
};

/* ���������е�Ԥ��ֵ���� */
QTSSAttrInfoDict::AttrInfo  QTSServerPrefs::sAttributes[] =
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
    /* 0 */ { "rtsp_timeout",                           NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 1 */ { "real_rtsp_timeout",                      NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 2 */ { "rtp_timeout",                            NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 3 */ { "maximum_connections",                    NULL,                   qtssAttrDataTypeSInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 4 */ { "maximum_bandwidth",                      NULL,                   qtssAttrDataTypeSInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 5 */ { "movie_folder",                           NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 6 */ { "bind_ip_addr",                           NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 7 */ { "break_on_assert",                        NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 8 */ { "auto_restart",                           NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 9 */ { "total_bytes_update",                     NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 10 */ { "average_bandwidth_update",              NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 11 */ { "safe_play_duration",                    NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 12 */ { "module_folder",                         NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 13 */ { "error_logfile_name",                    NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 14 */ { "error_logfile_dir",                     NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 15 */ { "error_logfile_interval",                NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 16 */ { "error_logfile_size",                    NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 17 */ { "error_logfile_verbosity",               NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 18 */ { "screen_logging",                        NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 19 */ { "error_logging",                         NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 20 */ { "drop_all_video_delay",                  NULL,                   qtssAttrDataTypeSInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 21 */ { "start_thinning_delay",                  NULL,                   qtssAttrDataTypeSInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 22 */ { "large_window_size",                     NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 23 */ { "window_size_threshold",                 NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 24 */ { "min_tcp_buffer_size",                   NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 25 */ { "max_tcp_buffer_size",                   NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 26 */ { "tcp_seconds_to_buffer",                 NULL,                   qtssAttrDataTypeFloat32,    qtssAttrModeRead | qtssAttrModeWrite },
    /* 27 */ { "do_report_http_connection_ip_address",  NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 28 */ { "default_authorization_realm",           NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 29 */ { "run_user_name",                         NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 30 */ { "run_group_name",                        NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 31 */ { "append_source_addr_in_transport",       NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 32 */ { "rtsp_port",                             NULL,                   qtssAttrDataTypeUInt16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 33 */ { "max_retransmit_delay",                  NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 34 */ { "small_window_size",                     NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 35 */ { "ack_logging_enabled",                   NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 36 */ { "rtcp_poll_interval",                    NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 37 */ { "rtcp_rcv_buf_size",                     NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 38 */ { "send_interval",                         NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 39 */ { "thick_all_the_way_delay",               NULL,                   qtssAttrDataTypeSInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 40 */ { "alt_transport_src_ipaddr",              NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 41 */ { "max_send_ahead_time",                   NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 42 */ { "reliable_udp_slow_start",               NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 43 */ { "auto_delete_sdp_files",                 NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 44 */ { "authentication_scheme",                 NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 45 */ { "sdp_file_delete_interval_seconds",      NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 46 */ { "auto_start",                            NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 47 */ { "reliable_udp",                          NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 48 */ { "reliable_udp_dirs",                     NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 49 */ { "reliable_udp_printfs",                  NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 50 */ { "drop_all_packets_delay",                NULL,                   qtssAttrDataTypeSInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 51 */ { "thin_all_the_way_delay",                NULL,                   qtssAttrDataTypeSInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 52 */ { "always_thin_delay",                     NULL,                   qtssAttrDataTypeSInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 53 */ { "start_thicking_delay",                  NULL,                   qtssAttrDataTypeSInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 54 */ { "quality_check_interval",                NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 55 */ { "RTSP_error_message",                    NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 56 */ { "RTSP_debug_printfs",                    NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 57 */ { "enable_monitor_stats_file",             NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 58 */ { "monitor_stats_file_interval_seconds",   NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 59 */ { "monitor_stats_file_name",               NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 60 */ { "enable_packet_header_printfs",          NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 61 */ { "packet_header_printf_options",          NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
	/* 62 */ { "overbuffer_rate",						NULL,					qtssAttrDataTypeFloat32,	qtssAttrModeRead | qtssAttrModeWrite },
	/* 63 */ { "medium_window_size",					NULL,					qtssAttrDataTypeUInt32,		qtssAttrModeRead | qtssAttrModeWrite },
	/* 64 */ { "window_size_max_threshold",				NULL,					qtssAttrDataTypeUInt32,		qtssAttrModeRead | qtssAttrModeWrite },
    /* 65 */ { "RTSP_server_info",                      NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
	/* 66 */ { "run_num_threads",                       NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
	/* 67 */ { "pid_file",								NULL,					qtssAttrDataTypeCharArray,	qtssAttrModeRead | qtssAttrModeWrite },
    /* 68 */ { "force_logs_close_on_write",             NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 69 */ { "disable_thinning",                      NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModeWrite },
	/* 70 */ { "player_requires_rtp_header_info",		NULL,					qtssAttrDataTypeCharArray,	qtssAttrModeRead | qtssAttrModeWrite },
	/* 71 */ { "player_requires_bandwidth_adjustment",	NULL,					qtssAttrDataTypeCharArray,	qtssAttrModeRead | qtssAttrModeWrite }
    

};


QTSServerPrefs::PrefInfo QTSServerPrefs::sPrefInfo[] =
{
	{ kDontAllowMultipleValues, "0",        NULL                    },  //rtsp_timeout
	{ kDontAllowMultipleValues, "180",      NULL                    },  //real_rtsp_timeout
	{ kDontAllowMultipleValues,	"120",		NULL					},	//rtp_timeout
	{ kDontAllowMultipleValues, "1000",     NULL                    },  //maximum_connections
	{ kDontAllowMultipleValues, "102400",   NULL                    },  //maximum_bandwidth,Ĭ��100M����
	{ kDontAllowMultipleValues,	DEFAULTPATHS_MOVIES_DIR, NULL       },	//movie_folder
	{ kAllowMultipleValues,     "0",        NULL                    },  //bind_ip_addr
	{ kDontAllowMultipleValues, "false",    NULL                    },  //break_on_assert
	{ kDontAllowMultipleValues, "true",     NULL                    },  //auto_restart
	{ kDontAllowMultipleValues, "1",        NULL                    },  //total_bytes_update
	{ kDontAllowMultipleValues, "60",       NULL                    },  //average_bandwidth_update
	{ kDontAllowMultipleValues, "600",      NULL                    },  //safe_play_duration
	{ kDontAllowMultipleValues,	DEFAULTPATHS_SSM_DIR,	NULL		},	//module_folder
	{ kDontAllowMultipleValues, "Error",    NULL                    },  //error_logfile_name
	{ kDontAllowMultipleValues,	DEFAULTPATHS_LOG_DIR,	NULL		},	//error_logfile_dir
	{ kDontAllowMultipleValues, "7",        NULL                    },  //error_logfile_interval
	{ kDontAllowMultipleValues, "256000",   NULL                    },  //error_logfile_size
	{ kDontAllowMultipleValues, "2",        NULL                    },  //error_logfile_verbosity,2 means log fatal errors, warnings,and asserts,��󼶱�Ϊ2
	{ kDontAllowMultipleValues, "true",     NULL                    },  //screen_logging
	{ kDontAllowMultipleValues, "true",     NULL                    },  //error_logging
	{ kDontAllowMultipleValues, "1750",     NULL                    },  //drop_all_video_delay
	{ kDontAllowMultipleValues, "0",        NULL                    },  //start_thinning_delay
	{ kDontAllowMultipleValues, "64",       NULL                    },  //large_window_size
	{ kDontAllowMultipleValues, "200",      NULL                    },  //window_size_threshold
	{ kDontAllowMultipleValues, "8192",     NULL                    },  //min_tcp_buffer_size
	{ kDontAllowMultipleValues,	"200000",	NULL					},	//max_tcp_buffer_size
	{ kDontAllowMultipleValues, ".5",       NULL                    },  //tcp_seconds_to_buffer
	{ kDontAllowMultipleValues, "false",    NULL                    },  //do_report_http_connection_ip_address,��http Response�в�������������ַ
	{ kDontAllowMultipleValues, "Streaming Server", NULL            },  //default_authorization_realm
#ifndef __Win32__
	{ kDontAllowMultipleValues, "qtss",     NULL                    },  //run_user_name
	{ kDontAllowMultipleValues, "qtss",     NULL                    },  //run_group_name
#else
	{ kDontAllowMultipleValues, "",         NULL                    },  //run_user_name
	{ kDontAllowMultipleValues, "",         NULL                    },  //run_group_name
#endif
	{ kDontAllowMultipleValues, "false",    NULL                    },  //append_source_addr_in_transport,��transportͷ�в�������������ַ
	{ kAllowMultipleValues,     "554",      sAdditionalDefaultPorts },  //rtsp_ports,������������������㲥�Ķ˿�
	{ kDontAllowMultipleValues, "500",      NULL                    },  //max_retransmit_delay
	{ kDontAllowMultipleValues, "24",       NULL                    },  //small_window_size
	{ kDontAllowMultipleValues, "false",    NULL                    },  //ack_logging_enabled,����ӡRTCP Ack����Ӧ��Ϣ
	{ kDontAllowMultipleValues, "100",      NULL                    },  //rtcp_poll_interval
	{ kDontAllowMultipleValues, "768",      NULL                    },  //rtcp_rcv_buf_size
	{ kDontAllowMultipleValues, "50",       NULL                    },  //send_interval
	{ kDontAllowMultipleValues, "-2000",    NULL                    },  //thick_all_the_way_delay
	{ kDontAllowMultipleValues, "",         NULL                    },  //alt_transport_src_ipaddr,����Client������ķ�������ַ//If empty, the server uses its own IP addr in the source= param of the transport header. Otherwise, it uses this addr.
	{ kDontAllowMultipleValues, "25",       NULL                    },  //max_send_ahead_time
	{ kDontAllowMultipleValues, "true",     NULL                    },  //reliable_udp_slow_start,Ĭ��ʹ��������
	{ kDontAllowMultipleValues, "false",    NULL                    },  //auto_delete_sdp_files
	{ kDontAllowMultipleValues, "digest",   NULL                    },  //authentication_scheme
	{ kDontAllowMultipleValues, "10",       NULL                    },  //sdp_file_delete_interval_seconds
	{ kDontAllowMultipleValues, "false",    NULL                    },  //auto_start,Ĭ�Ͽ���������
	{ kDontAllowMultipleValues, "true",     NULL                    },  //reliable_udp,Ĭ�Ͽɿ�UDP��ʽ����
	{ kAllowMultipleValues,	DEFAULTPATHS_DIRECTORY_SEPARATOR,	NULL},	//reliable_udp_dirs (set all dirs)
	{ kDontAllowMultipleValues, "false",    NULL                    },  //reliable_udp_printfs
	{ kDontAllowMultipleValues, "2500",     NULL                    },  //drop_all_packets_delay
	{ kDontAllowMultipleValues, "1500",     NULL                    },  //thin_all_the_way_delay
	{ kDontAllowMultipleValues, "750",      NULL                    },  //always_thin_delay
	{ kDontAllowMultipleValues, "250",      NULL                    },  //start_thicking_delay
	{ kDontAllowMultipleValues, "1000",     NULL                    },  //quality_check_interval
	{ kDontAllowMultipleValues, "false",    NULL                    },  //RTSP_error_message
	{ kDontAllowMultipleValues, "false",    NULL                    },  //RTSP_debug_printfs
#if __MacOSX__
	{ kDontAllowMultipleValues, "false",     NULL                   },  //enable_monitor_stats_file
#else
	{ kDontAllowMultipleValues, "false",    NULL                    },  //enable_monitor_stats_file
#endif
	{ kDontAllowMultipleValues, "10",        NULL                   },  //monitor_stats_file_interval_seconds
	{ kDontAllowMultipleValues, "server_status",        NULL        },  //monitor_stats_file_name
	{ kDontAllowMultipleValues, "false",    NULL                    },  //enable_packet_header_printfs
	{ kDontAllowMultipleValues, "rtp;rr;sr;app;ack;",NULL           },  //packet_header_printf_options
	{ kDontAllowMultipleValues, "2.0",		NULL					},	//overbuffer_rate
	{ kDontAllowMultipleValues,	"48",		NULL					},	//medium_window_size
	{ kDontAllowMultipleValues,	"1000",		NULL					},	//window_size_max_threshold
	{ kDontAllowMultipleValues, "true",     NULL                    },  //RTSP_server_info,Ĭ����RTSP/Http����ʱ,��Client�����������Ϣ
	{ kDontAllowMultipleValues, "0",        NULL                    },  //run_num_threads,Ĭ��һ��CPU��,����һ�������߳�
	{ kDontAllowMultipleValues, DEFAULTPATHS_PID_DIR PLATFORM_SERVER_BIN_NAME ".pid",	NULL	},	//pid_file
	{ kDontAllowMultipleValues, "false",    NULL                    },  //force_logs_close_on_write,��־ÿ��д��󲢲��ر�
	{ kDontAllowMultipleValues, "false",    NULL                    },  //disable_thinning,Ĭ�Ͽ��Ա���
	{ kAllowMultipleValues,     "Nokia",    sRTP_Header_Players     },  //players_requires_rtp_header_info
	{ kAllowMultipleValues,     "Nokia",    sAdjust_Bandwidth_Players}  //players_requires_bandwidth_adjustment


};

/* Ϊ������Ԥ��ֵ�ֵ����������,�μ�QTSS.h����ϸ��˵�� */
void QTSServerPrefs::Initialize()
{
	for (UInt32 x = 0; x < qtssPrefsNumParams; x++)
		QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kPrefsDictIndex)->
		SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
		sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
}


QTSServerPrefs::QTSServerPrefs(XMLPrefsParser* inPrefsSource, Bool16 inWriteMissingPrefs)
:   QTSSPrefs(inPrefsSource, NULL, QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kPrefsDictIndex), false),
    fRTSPTimeoutInSecs(0),
    fRTSPTimeoutString(fRTSPTimeoutBuf, 0),
    fRealRTSPTimeoutInSecs(0),
    fRTPTimeoutInSecs(0),
    fMaximumConnections(0),
    fMaxBandwidthInKBits(0),   
    fDropAllPacketsTimeInMsec(0),
    fDropAllVideoPacketsTimeInMsec(0),
    fThinAllTheWayTimeInMsec(0),
    fAlwaysThinTimeInMsec(0),
    fStartThinningTimeInMsec(0),
    fStartThickingTimeInMsec(0),
    fThickAllTheWayTimeInMsec(0),
    fQualityCheckIntervalInMsec(0),
    /*�˴�ȱ��fMinTCPBufferSizeInBytes,fMaxTCPBufferSizeInBytes,fTCPSecondsToBuffer*/
	fDoReportHTTPConnectionAddress(false),
	fAppendSrcAddrInTransport(false),
	fBreakOnAssert(false),
	fAutoRestart(false),
	fTBUpdateTimeInSecs(0),
	fABUpdateTimeInSecs(0),
	fSafePlayDurationInSecs(0),
    fErrorLogEnabled(false),
	fScreenLoggingEnabled(true),
	fErrorLogBytes(0),
	fErrorRollIntervalInDays(0),
	fErrorLogVerbosity(0),//��ǰ������־�ļ���
    fMaxRetransDelayInMsec(0),
    fIsAckLoggingEnabled(false),
    fRTCPPollIntervalInMsec(0),
    fRTCPSocketRcvBufSizeInK(0), 
    fSendIntervalInMsec(0),
    fMaxSendAheadTimeInSecs(0),
    fIsSlowStartEnabled(false),
    fAutoStart(false),
    fReliableUDP(true),/* ����RUDP��ʽ���� */
    fReliableUDPPrintfs(false),
    fEnableRTSPErrMsg(false),
    fEnableRTSPDebugPrintfs(false),
    fEnableRTSPServerInfo(true),
    fNumThreads(0),
#if __MacOSX__
    fEnableMonitorStatsFile(false),
#else
    fEnableMonitorStatsFile(false),
#endif 
    fStatsFileIntervalSeconds(10),
	fOverbufferRate(0.0),
	fSmallWindowSizeInK(0),
	fMediumWindowSizeInK(0),
	fLargeWindowSizeInK(0),
	fWindowSizeThreshold(0),
	fWindowSizeMaxThreshold(0),
    fEnablePacketHeaderPrintfs(false),   
    fPacketHeaderPrintfOptions(kRTPALL | kRTCPSR | kRTCPRR | kRTCPAPP | kRTCPACK),
    fCloseLogsOnWrite(false),
    fDisableThinning(false),
	fauto_delete_sdp_files(false),  
	fsdp_file_delete_interval_seconds(10),/* ���sdp�ļ����10s */
	fAuthScheme(qtssAuthDigest) /* Ĭ��digest��֤���� */
{
    SetupAttributes();
    RereadServerPreferences(inWriteMissingPrefs);
}

//���ù��캯���иճ�ʼ�������ݳ�Ա������55�����Ե�ֵ,ע��ȱ��fPacketHeaderPrintfOptions��fAuthScheme
void QTSServerPrefs::SetupAttributes()
{
    this->SetVal(qtssPrefsRTSPTimeout,      &fRTSPTimeoutInSecs,        sizeof(fRTSPTimeoutInSecs));
    this->SetVal(qtssPrefsRealRTSPTimeout,  &fRealRTSPTimeoutInSecs,    sizeof(fRealRTSPTimeoutInSecs));
    this->SetVal(qtssPrefsRTPTimeout,       &fRTPTimeoutInSecs,         sizeof(fRTPTimeoutInSecs));
    this->SetVal(qtssPrefsMaximumConnections,&fMaximumConnections,      sizeof(fMaximumConnections));
    this->SetVal(qtssPrefsMaximumBandwidth, &fMaxBandwidthInKBits,      sizeof(fMaxBandwidthInKBits));
   
	this->SetVal(qtssPrefsDropAllPacketsDelayInMsec,    &fDropAllPacketsTimeInMsec,     sizeof(fDropAllPacketsTimeInMsec));
	this->SetVal(qtssPrefsDropVideoAllPacketsDelayInMsec,   &fDropAllVideoPacketsTimeInMsec,    sizeof(fDropAllVideoPacketsTimeInMsec));
	this->SetVal(qtssPrefsThinAllTheWayDelayInMsec,     &fThinAllTheWayTimeInMsec,      sizeof(fThinAllTheWayTimeInMsec));
	this->SetVal(qtssPrefsAlwaysThinDelayInMsec,        &fAlwaysThinTimeInMsec,         sizeof(fAlwaysThinTimeInMsec));
	this->SetVal(qtssPrefsStartThinningDelayInMsec,     &fStartThinningTimeInMsec,      sizeof(fStartThinningTimeInMsec));
	this->SetVal(qtssPrefsStartThickingDelayInMsec,     &fStartThickingTimeInMsec,      sizeof(fStartThickingTimeInMsec));
	this->SetVal(qtssPrefsThickAllTheWayDelayInMsec,    &fThickAllTheWayTimeInMsec,     sizeof(fThickAllTheWayTimeInMsec));
	this->SetVal(qtssPrefsQualityCheckIntervalInMsec,   &fQualityCheckIntervalInMsec,   sizeof(fQualityCheckIntervalInMsec));
     
    this->SetVal(qtssPrefsMinTCPBufferSizeInBytes,  &fMinTCPBufferSizeInBytes,  sizeof(fMinTCPBufferSizeInBytes));
    this->SetVal(qtssPrefsMaxTCPBufferSizeInBytes,  &fMaxTCPBufferSizeInBytes,  sizeof(fMaxTCPBufferSizeInBytes));
    this->SetVal(qtssPrefsTCPSecondsToBuffer,   &fTCPSecondsToBuffer,           sizeof(fTCPSecondsToBuffer));

    this->SetVal(qtssPrefsDoReportHTTPConnectionAddress,    &fDoReportHTTPConnectionAddress,   sizeof(fDoReportHTTPConnectionAddress));
    this->SetVal(qtssPrefsSrcAddrInTransport,   &fAppendSrcAddrInTransport, sizeof(fAppendSrcAddrInTransport));
	this->SetVal(qtssPrefsBreakOnAssert,    &fBreakOnAssert,            sizeof(fBreakOnAssert));
	this->SetVal(qtssPrefsAutoRestart,      &fAutoRestart,              sizeof(fAutoRestart));
	this->SetVal(qtssPrefsTotalBytesUpdate, &fTBUpdateTimeInSecs,       sizeof(fTBUpdateTimeInSecs));
	this->SetVal(qtssPrefsAvgBandwidthUpdate,&fABUpdateTimeInSecs,      sizeof(fABUpdateTimeInSecs));
	this->SetVal(qtssPrefsSafePlayDuration, &fSafePlayDurationInSecs,   sizeof(fSafePlayDurationInSecs));

	this->SetVal(qtssPrefsErrorLogEnabled,  &fErrorLogEnabled,          sizeof(fErrorLogEnabled));
	this->SetVal(qtssPrefsScreenLogging,    &fScreenLoggingEnabled,     sizeof(fScreenLoggingEnabled));
	this->SetVal(qtssPrefsMaxErrorLogSize,  &fErrorLogBytes,            sizeof(fErrorLogBytes));
	this->SetVal(qtssPrefsErrorRollInterval, &fErrorRollIntervalInDays, sizeof(fErrorRollIntervalInDays));
	this->SetVal(qtssPrefsErrorLogVerbosity, &fErrorLogVerbosity,       sizeof(fErrorLogVerbosity));

    this->SetVal(qtssPrefsMaxRetransDelayInMsec,    &fMaxRetransDelayInMsec,    sizeof(fMaxRetransDelayInMsec));
    this->SetVal(qtssPrefsAckLoggingEnabled,        &fIsAckLoggingEnabled,      sizeof(fIsAckLoggingEnabled));
    this->SetVal(qtssPrefsRTCPPollIntervalInMsec,   &fRTCPPollIntervalInMsec,   sizeof(fRTCPPollIntervalInMsec));
    this->SetVal(qtssPrefsRTCPSockRcvBufSizeInK,    &fRTCPSocketRcvBufSizeInK,  sizeof(fRTCPSocketRcvBufSizeInK));
    this->SetVal(qtssPrefsSendInterval,             &fSendIntervalInMsec,       sizeof(fSendIntervalInMsec));
    this->SetVal(qtssPrefsMaxAdvanceSendTimeInSec,  &fMaxSendAheadTimeInSecs,   sizeof(fMaxSendAheadTimeInSecs));
	this->SetVal(qtssPrefsReliableUDPSlowStart,     &fIsSlowStartEnabled,       sizeof(fIsSlowStartEnabled));
	this->SetVal(qtssPrefsAutoStart,                &fAutoStart,                sizeof(fAutoStart));
	this->SetVal(qtssPrefsReliableUDP,              &fReliableUDP,              sizeof(fReliableUDP));
	this->SetVal(qtssPrefsReliableUDPPrintfs,       &fReliableUDPPrintfs,       sizeof(fReliableUDPPrintfs));

	this->SetVal(qtssPrefsOverbufferRate,			&fOverbufferRate,			sizeof(fOverbufferRate));

	this->SetVal(qtssPrefsSmallWindowSizeInK,   &fSmallWindowSizeInK,       sizeof(fSmallWindowSizeInK));
	this->SetVal(qtssPrefsMediumWindowSizeInK, 	&fMediumWindowSizeInK,   sizeof(fMediumWindowSizeInK));
	this->SetVal(qtssPrefsLargeWindowSizeInK,   &fLargeWindowSizeInK,       sizeof(fLargeWindowSizeInK));
	this->SetVal(qtssPrefsWindowSizeThreshold,  &fWindowSizeThreshold,      sizeof(fWindowSizeThreshold));
	this->SetVal(qtssPrefsWindowSizeMaxThreshold,	&fWindowSizeMaxThreshold,   sizeof(fWindowSizeMaxThreshold));
    
	this->SetVal(qtssPrefsEnableRTSPErrorMessage,       &fEnableRTSPErrMsg,             sizeof(fEnableRTSPErrMsg));
	this->SetVal(qtssPrefsEnableRTSPDebugPrintfs,       &fEnableRTSPDebugPrintfs,       sizeof(fEnableRTSPDebugPrintfs));
	this->SetVal(qtssPrefsEnableRTSPServerInfo,         &fEnableRTSPServerInfo,         sizeof(fEnableRTSPServerInfo));
	this->SetVal(qtssPrefsRunNumThreads,                &fNumThreads,                   sizeof(fNumThreads));
	this->SetVal(qtssPrefsEnableMonitorStatsFile,       &fEnableMonitorStatsFile,       sizeof(fEnableMonitorStatsFile));
	this->SetVal(qtssPrefsMonitorStatsFileIntervalSec,  &fStatsFileIntervalSeconds,     sizeof(fStatsFileIntervalSeconds));

	this->SetVal(qtssPrefsEnablePacketHeaderPrintfs,    &fEnablePacketHeaderPrintfs,    sizeof(fEnablePacketHeaderPrintfs));
	this->SetVal(qtssPrefsCloseLogsOnWrite,             &fCloseLogsOnWrite,             sizeof(fCloseLogsOnWrite));

	this->SetVal(qtssPrefsDisableThinning,              &fDisableThinning,              sizeof(fDisableThinning));
    this->SetVal(qtssPrefsAutoDeleteSDPFiles,       &fauto_delete_sdp_files,    sizeof(fauto_delete_sdp_files));
    this->SetVal(qtssPrefsDeleteSDPFilesInterval,   &fsdp_file_delete_interval_seconds,   sizeof(fsdp_file_delete_interval_seconds));
   
}

/* used in QTSServer::RereadPrefsService() */
/*���Ǳ�������Ҫ�ĺ���:�ȱ����������������б�,�����Ԥ��ֵ�ļ����ø�ָ��������,��Ԥ��ֵ����,����Ĭ��ֵ����,Ȼ��������֤��ʽ,
	RTP/RTCP��ͷ��ӡѡ��,��־д�ر�,��󽫵������Ԥ��ֵ����д��xmlԤ��ֵ�ļ� */
void QTSServerPrefs::RereadServerPreferences(Bool16 inWriteMissingPrefs)
{
    OSMutexLocker locker(&fPrefsMutex);

	//�õ�������Ԥ��ֵ�ֵ����
    QTSSDictionaryMap* theMap = QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kPrefsDictIndex);
    
	//�����������������б�,�����Ԥ��ֵ�ļ����ø�ָ��������
    for (UInt32 x = 0; x < theMap->GetNumAttrs(); x++)
    {
        // Look for a pref in the file that matches each pref in the dictionary
        char* thePrefTypeStr = NULL;
        char* thePrefName = NULL;
		char* thePrefValue = NULL;
        
        ContainerRef server = fPrefsSource->GetRefForServer();/* ��fRootTag�»�ȡ��Tag SERVER������,û�о������� */
        ContainerRef pref = fPrefsSource->GetPrefRefByName( server, theMap->GetAttrName(x) );/* �õ���Tag SERVERָ������������Tag */
        
		/* ���粻��,����ָ��Tag��ָ��index����Tag��value,�õ�ָ��Tag��NAME��TYPE����ֵ,��Tag��������˷��� */
        if (pref != NULL)
            thePrefValue = fPrefsSource->GetPrefValueByRef( pref, 0, &thePrefName,(char**)&thePrefTypeStr);
                                                                    
        /* ����õ�������ֵ�ǿյ�, */
        if ((thePrefValue == NULL) && (x < qtssPrefsNumParams)) // Only generate errors for server prefs
        {
            // ����û������ֵ,����Ĭ��ֵ,ʹ��Ĭ��ֵ,������־�м�¼
            // There is no pref, use the default and log an error
            if (::strlen(sPrefInfo[x].fDefaultValue) > 0)
            {
                // ������Ĭ��ֵ(���ַ�������),������־�м�¼;����û��Ĭ��ֵ,����Ļ�ϴ�ӡ����
                // Only log this as an error if there is a default (an empty string
                // doesn't count). If there is no default, we will constantly print
                // out an error message...
                QTSSModuleUtils::LogError(  QTSSModuleUtils::GetMisingPrefLogVerbosity(),
                                            qtssServerPrefMissing,
                                            0,
                                            sAttributes[x].fAttrName,
                                            sPrefInfo[x].fDefaultValue);
            }
            //��Ĭ��ֵ������Ӧ�ķ���������ֵ
            this->SetPrefValue(x, 0, sPrefInfo[x].fDefaultValue, sAttributes[x].fAttrDataType);
			//���绹�и��ӵ�Ĭ��ֵ,ҲҪ����������
            if (sPrefInfo[x].fAdditionalDefVals != NULL)
            {
                // Add additional default values if they exist
                for (UInt32 y = 0; sPrefInfo[x].fAdditionalDefVals[y] != NULL; y++)
                    this->SetPrefValue(x, y+1, sPrefInfo[x].fAdditionalDefVals[y], sAttributes[x].fAttrDataType);
            }
            
			//�����2�����Ϊtrue,����ָ������ֵ���������͵�Tag,����Tag��Ƕ������,�����ظ�Tag,���������Լ���(���)����ֵ,����д��xmlԤ�����ļ�
            if (inWriteMissingPrefs)
            {
                // Add this value into the file, because we need it.
                pref = fPrefsSource->AddPref( server, sAttributes[x].fAttrName, QTSSDataConverter::TypeToTypeString(sAttributes[x].fAttrDataType));
                // ��д��ָ��������ֵ
				fPrefsSource->AddPrefValue(pref, sPrefInfo[x].fDefaultValue);
                //����ж���ֵ,Ҳһ��д��
                if (sPrefInfo[x].fAdditionalDefVals != NULL)
                {
                    for (UInt32 a = 0; sPrefInfo[x].fAdditionalDefVals[a] != NULL; a++)
                        fPrefsSource->AddPrefValue(pref, sPrefInfo[x].fAdditionalDefVals[a]);
                }
            }
            continue; //������һ������
        }
        
        QTSS_AttrDataType theType = QTSSDataConverter::TypeStringToType(thePrefTypeStr);
        
		// ������Ԥ��ֵ,�����������Ͳ���,ʹ��Ĭ��ֵ,����¼����
		// The pref in the file has the wrong type, use the default and log an error
        if ((x < qtssPrefsNumParams) && (theType != sAttributes[x].fAttrDataType)) // Only generate errors for server prefs
        { 
            // ������Ĭ��ֵ(���ַ�������),������־�м�¼;����û��Ĭ��ֵ,����Ļ�ϴ�ӡ����
            if (::strlen(sPrefInfo[x].fDefaultValue) > 0)
            {
                // Only log this as an error if there is a default (an empty string
                // doesn't count). If there is no default, we will constantly print
                // out an error message...
                QTSSModuleUtils::LogError(  qtssWarningVerbosity,
                                            qtssServerPrefWrongType,
                                            0,
                                            sAttributes[x].fAttrName,
                                            sPrefInfo[x].fDefaultValue);
            }
            //��Ĭ��ֵ������Ӧ�ķ���������ֵ
            this->SetPrefValue(x, 0, sPrefInfo[x].fDefaultValue, sAttributes[x].fAttrDataType);
			//���绹�и��ӵ�Ĭ��ֵ,ҲҪ����������
            if (sPrefInfo[x].fAdditionalDefVals != NULL)
            {
                // Add additional default values if they exist
                for (UInt32 z = 0; sPrefInfo[x].fAdditionalDefVals[z] != NULL; z++)
                    this->SetPrefValue(x, z+1, sPrefInfo[x].fAdditionalDefVals[z], sAttributes[x].fAttrDataType);
            }
			//�����2�����Ϊtrue,��ɾ����Tag,�������µ�Tag,����������,����(���)����ֵ
			// Remove it out of the file and add in the default.
            if (inWriteMissingPrefs)
            {   
                fPrefsSource->RemovePref(pref);
                pref = fPrefsSource->AddPref( server, sAttributes[x].fAttrName, QTSSDataConverter::TypeToTypeString(sAttributes[x].fAttrDataType));
                fPrefsSource->AddPrefValue(pref, sPrefInfo[x].fDefaultValue);
                if (sPrefInfo[x].fAdditionalDefVals != NULL)
                {
                    for (UInt32 b = 0; sPrefInfo[x].fAdditionalDefVals[b] != NULL; b++)
                        fPrefsSource->AddPrefValue(pref, sPrefInfo[x].fAdditionalDefVals[b]);
                }
            }
            continue;//������һ������
        }
        
		/* ���õ�ֵ���Ա�־,��������ĺ���QTSSPrefs::SetPrefValuesFromFileWithRef()�еĵ��������� */
        UInt32 theNumValues = 0;
        if ((x < qtssPrefsNumParams) && (!sPrefInfo[x].fAllowMultipleValues))
            theNumValues = 1;
        
		//��Ԥ��ֵ�ļ����ø�ָ��������
        this->SetPrefValuesFromFileWithRef(pref, x, theNumValues);
    }
    
    
    // Do any special pref post-processing

    //�ȵõ���֤��ʽ��Ԥ��ֵ,�������������ݳ�ԱfAuthScheme
    this->UpdateAuthScheme();
    //��ȡ������RTP/RTCP��ͷ��ӡѡ���Ԥ��ֵ,�����������������ݳ�ԱfPacketHeaderPrintfOptions
    this->UpdatePrintfOptions();
	//�����ݳ�ԱfEnableRTSPErrMsg����QTSSModuleUtils::sEnableRTSPErrorMsg
    QTSSModuleUtils::SetEnableRTSPErrorMsg(fEnableRTSPErrMsg);
    //�����ݳ�ԱfCloseLogsOnWrite����QTSSRollingLog�еľ�̬����sCloseOnWrite
    QTSSRollingLog::SetCloseOnWrite(fCloseLogsOnWrite);
   
    // In case we made any changes, write out the prefs file,�������������Ԥ��ֵ����д��xmlԤ��ֵ�ļ�
    (void)fPrefsSource->WritePrefsFile();
}

//�ȵõ���֤��ʽ��Ԥ��ֵ,�������������ݳ�ԱfAuthScheme
void    QTSServerPrefs::UpdateAuthScheme()
{
    static StrPtrLen sNoAuthScheme("none");
    static StrPtrLen sBasicAuthScheme("basic");
    static StrPtrLen sDigestAuthScheme("digest");
    
    // Get the auth scheme attribute,�õ���֤��ʽ��Ԥ��ֵ
    StrPtrLen* theAuthScheme = this->GetValue(qtssPrefsAuthenticationScheme);
    
    if (theAuthScheme->Equal(sNoAuthScheme))
        fAuthScheme = qtssAuthNone;
    else if (theAuthScheme->Equal(sBasicAuthScheme))
        fAuthScheme = qtssAuthBasic;
    else if (theAuthScheme->Equal(sDigestAuthScheme))
        fAuthScheme = qtssAuthDigest;
}

//��ȡ������RTP/RTCP��ͷ��ӡѡ���Ԥ��ֵ,�����������������ݳ�ԱfPacketHeaderPrintfOptions
void QTSServerPrefs::UpdatePrintfOptions()
{
	//���Ȼ�÷�����RTP/RTCP��ͷ��ӡѡ���Ԥ��ֵ,����û��,��������
	StrPtrLen* theOptions = this->GetValue(qtssPrefsPacketHeaderPrintfOptions);
	if (theOptions == NULL || theOptions->Len == 0)
		return;

	//������Ԥ��ֵ,�����������ݳ�ԱfPacketHeaderPrintfOptions
	fPacketHeaderPrintfOptions = 0;
	if (theOptions->FindStringIgnoreCase("rtp"))
		fPacketHeaderPrintfOptions |= kRTPALL;
	if (theOptions->FindStringIgnoreCase("sr"))
		fPacketHeaderPrintfOptions |= kRTCPSR;
	if (theOptions->FindStringIgnoreCase("rr"))
		fPacketHeaderPrintfOptions |= kRTCPRR;
	if (theOptions->FindStringIgnoreCase("app"))
		fPacketHeaderPrintfOptions |= kRTCPAPP;
	if (theOptions->FindStringIgnoreCase("ack"))
		fPacketHeaderPrintfOptions |= kRTCPACK;

}

/* used in RTSPRequestInterface::RTSPRequestInterface() */
// ??�·��仺���,����ṩ�Ļ�����δ���?
//���Ȼ�ȡ��Ӱ�ļ���·��,�ٽ���������ָ���Ļ��沢����(�������ṩ�Ļ��泤�Ȳ���,���·���ָ�����ȵĻ���)
char*   QTSServerPrefs::GetMovieFolder(char* inBuffer, UInt32* ioLen)
{
    OSMutexLocker locker(&fPrefsMutex);

    // Get the movie folder attribute
    StrPtrLen* theMovieFolder = this->GetValue(qtssPrefsMovieFolder);

    // If the movie folder path fits inside the provided buffer, copy it there
    if (theMovieFolder->Len < *ioLen)
        ::memcpy(inBuffer, theMovieFolder->Ptr, theMovieFolder->Len);
    else
    {
        // Otherwise, allocate a buffer to store the path
        inBuffer = NEW char[theMovieFolder->Len + 2];
        ::memcpy(inBuffer, theMovieFolder->Ptr, theMovieFolder->Len);
    }
    inBuffer[theMovieFolder->Len] = 0;
    *ioLen = theMovieFolder->Len;
    return inBuffer;
}

/* �ж����ָ�����ַ����Ƿ�ͷ�����Ԥ��ֵ��ͬ,��ͬ����true,���򷵻�false */
Bool16 QTSServerPrefs::IsPathInsideReliableUDPDir(StrPtrLen* inPath)
{
    OSMutexLocker locker(&fPrefsMutex);

    QTSS_Error theErr = QTSS_NoErr;
    for (UInt32 x = 0; theErr == QTSS_NoErr; x++)
    {
        StrPtrLen theReliableUDPDir;
        theErr = this->GetValuePtr(qtssPrefsReliableUDPDirs, x, (void**)&theReliableUDPDir.Ptr, &theReliableUDPDir.Len, true);
        
        if (theErr != QTSS_NoErr)
            return false;
            
        if (inPath->NumEqualIgnoreCase(theReliableUDPDir.Ptr, theReliableUDPDir.Len))
            return true;
    }
    Assert(0);
    return false;
}

/* �����ȥ���Ƶõ�����ķ�����iP,����Transportͷ */
void QTSServerPrefs::GetTransportSrcAddr(StrPtrLen* ioBuf)
{
    OSMutexLocker locker(&fPrefsMutex);

    // �õ�����ķ�����iP,����Transportͷ
    StrPtrLen* theTransportAddr = this->GetValue(qtssPrefsAltTransportIPAddr);

    // If the movie folder path fits inside the provided buffer, copy it there
    if ((theTransportAddr->Len > 0) && (theTransportAddr->Len < ioBuf->Len))
    {
        ::memcpy(ioBuf->Ptr, theTransportAddr->Ptr, theTransportAddr->Len);
        ioBuf->Len = theTransportAddr->Len;
    }
    else
        ioBuf->Len = 0;
}

/* �Ȼ�ȡ����ID���ָ�������Եĳ���,�ٶ�ָ̬�����ȵ��ڴ�,���벢���ش������ָ���������ַ����Ļ����ַ */
char* QTSServerPrefs::GetStringPref(QTSS_AttributeID inAttrID)
{
    StrPtrLen theBuffer;
    (void)this->GetValue(inAttrID, 0, NULL, &theBuffer.Len);
    theBuffer.Ptr = NEW char[theBuffer.Len + 1];
    theBuffer.Ptr[0] = '\0';
    
    if (theBuffer.Len > 0)
    {
        QTSS_Error theErr = this->GetValue(inAttrID, 0, theBuffer.Ptr, &theBuffer.Len);
        if (theErr == QTSS_NoErr)
            theBuffer.Ptr[theBuffer.Len] = 0;
    }
    return theBuffer.Ptr;
}

/* ���������ÿ��д���Ƿ�ǿ����־�ļ��ر�,ͬʱ��QTSSRollingLogҲ��ͬ������ */
void QTSServerPrefs::SetCloseLogsOnWrite(Bool16 closeLogsOnWrite) 
{
	//��ָ��������������Ƿ�ÿ��д��ر���־,ͬʱ�������ݳ�ԱfCloseLogsOnWrite
    QTSSRollingLog::SetCloseOnWrite(closeLogsOnWrite);
    fCloseLogsOnWrite = closeLogsOnWrite; 
}

