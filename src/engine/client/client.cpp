/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#define _WIN32_WINNT 0x0501

#include <new>

#include <stdlib.h> // qsort
#include <stdarg.h>
#include <string.h>
#include <climits>

#include <base/math.h>
#include <base/vmath.h>
#include <base/system.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>

#include <engine/client.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/map.h>
#include <engine/masterserver.h>
#include <engine/serverbrowser.h>
#include <engine/storage.h>

#include <engine/external/md5/md5.h>

#include <engine/client/http.h>
#include <engine/shared/config.h>
#include <engine/shared/compression.h>
#include <engine/shared/datafile.h>
#include <engine/shared/demo.h>
#include <engine/shared/filecollection.h>
#include <engine/shared/ghost.h>
#include <engine/shared/network.h>
#include <engine/shared/packer.h>
#include <engine/shared/protocol.h>
#include <engine/shared/protocol_ex.h>
#include <engine/shared/ringbuffer.h>
#include <engine/shared/snapshot.h>
#include <engine/shared/fifo.h>
#include <engine/shared/uuid_manager.h>

#include <game/extrainfo.h>
#include <game/version.h>

#include <mastersrv/mastersrv.h>

#include <engine/client/serverbrowser.h>

#if defined(CONF_FAMILY_WINDOWS)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

#if defined(CONF_FAMILY_UNIX)
	#include <termios.h>
#endif

#include "friends.h"
#include "serverbrowser.h"
#include "updater.h"
#include "client.h"

#include <zlib.h>

#ifdef main
#undef main
#endif

#include "signal.h"
#include "stdio.h"
#include "stdlib.h"

static class CClient *spClient;

static void OnCrash(int sig)
{
	printf("[stats] caught signal=%d saving stats...\n", sig);
	spClient->Disconnect();
	exit(0); // TODO: should probably not exit with ok
}

void CGraph::Init(float Min, float Max)
{
	m_Min = Min;
	m_Max = Max;
	m_Index = 0;
}

void CGraph::ScaleMax()
{
	int i = 0;
	m_Max = 0;
	for(i = 0; i < MAX_VALUES; i++)
	{
		if(m_aValues[i] > m_Max)
			m_Max = m_aValues[i];
	}
}

void CGraph::ScaleMin()
{
	int i = 0;
	m_Min = m_Max;
	for(i = 0; i < MAX_VALUES; i++)
	{
		if(m_aValues[i] < m_Min)
			m_Min = m_aValues[i];
	}
}

void CGraph::Add(float v, float r, float g, float b)
{
	m_Index = (m_Index+1)&(MAX_VALUES-1);
	m_aValues[m_Index] = v;
	m_aColors[m_Index][0] = r;
	m_aColors[m_Index][1] = g;
	m_aColors[m_Index][2] = b;
}

void CSmoothTime::Init(int64 Target)
{
	m_Snap = time_get();
	m_Current = Target;
	m_Target = Target;
	m_aAdjustSpeed[0] = 0.3f;
	m_aAdjustSpeed[1] = 0.3f;
	m_Graph.Init(0.0f, 0.5f);
}

void CSmoothTime::SetAdjustSpeed(int Direction, float Value)
{
	m_aAdjustSpeed[Direction] = Value;
}

int64 CSmoothTime::Get(int64 Now)
{
	int64 c = m_Current + (Now - m_Snap);
	int64 t = m_Target + (Now - m_Snap);

	// it's faster to adjust upward instead of downward
	// we might need to adjust these abit

	float AdjustSpeed = m_aAdjustSpeed[0];
	if(t > c)
		AdjustSpeed = m_aAdjustSpeed[1];

	float a = ((Now-m_Snap)/(float)time_freq()) * AdjustSpeed;
	if(a > 1.0f)
		a = 1.0f;

	int64 r = c + (int64)((t-c)*a);

	m_Graph.Add(a+0.5f,1,1,1);

	return r;
}

void CSmoothTime::UpdateInt(int64 Target)
{
	int64 Now = time_get();
	m_Current = Get(Now);
	m_Snap = Now;
	m_Target = Target;
}

void CSmoothTime::Update(CGraph *pGraph, int64 Target, int TimeLeft, int AdjustDirection)
{
	int UpdateTimer = 1;

	if(TimeLeft < 0)
	{
		int IsSpike = 0;
		if(TimeLeft < -50)
		{
			IsSpike = 1;

			m_SpikeCounter += 5;
			if(m_SpikeCounter > 50)
				m_SpikeCounter = 50;
		}

		if(IsSpike && m_SpikeCounter < 15)
		{
			// ignore this ping spike
			UpdateTimer = 0;
			pGraph->Add(TimeLeft, 1,1,0);
		}
		else
		{
			pGraph->Add(TimeLeft, 1,0,0);
			if(m_aAdjustSpeed[AdjustDirection] < 30.0f)
				m_aAdjustSpeed[AdjustDirection] *= 2.0f;
		}
	}
	else
	{
		if(m_SpikeCounter)
			m_SpikeCounter--;

		pGraph->Add(TimeLeft, 0,1,0);

		m_aAdjustSpeed[AdjustDirection] *= 0.95f;
		if(m_aAdjustSpeed[AdjustDirection] < 2.0f)
			m_aAdjustSpeed[AdjustDirection] = 2.0f;
	}

	if(UpdateTimer)
		UpdateInt(Target);
}


CClient::CClient() : m_DemoPlayer(&m_SnapshotDelta)
{
	m_DemoRecorder[0] = CDemoRecorder(&m_SnapshotDelta);
	m_DemoRecorder[1] = CDemoRecorder(&m_SnapshotDelta);
	m_DemoRecorder[2] = CDemoRecorder(&m_SnapshotDelta);

	m_pInput = 0;
	m_pGameClient = 0;
	m_pMap = 0;
	m_pConsole = 0;

	m_RenderFrameTime = 0.0001f;
	m_RenderFrameTimeLow = 1.0f;
	m_RenderFrameTimeHigh = 0.0f;
	m_RenderFrames = 0;
	m_LastRenderTime = time_get();

	m_GameTickSpeed = SERVER_TICK_SPEED;

	m_SnapCrcErrors = 0;
	m_AutoScreenshotRecycle = false;
	m_AutoStatScreenshotRecycle = false;
	m_AutoCSVRecycle = false;
	m_EditorActive = false;

	m_AckGameTick[0] = -1;
	m_AckGameTick[1] = -1;
	m_CurrentRecvTick[0] = 0;
	m_CurrentRecvTick[1] = 0;
	m_RconAuthed[0] = 0;
	m_RconAuthed[1] = 0;
	m_RconPassword[0] = '\0';
	m_Password[0] = '\0';

	// version-checking
	m_aVersionStr[0] = '0';
	m_aVersionStr[1] = '\0';

	// pinging
	m_PingStartTime = 0;

	m_aCurrentMap[0] = 0;

	m_aCmdConnect[0] = 0;

	// map download
	m_aMapdownloadFilename[0] = 0;
	m_aMapdownloadName[0] = 0;
	m_pMapdownloadTask = NULL;
	m_MapdownloadFile = 0;
	m_MapdownloadChunk = 0;
	m_MapdownloadSha256Present = false;
	m_MapdownloadSha256 = SHA256_ZEROED;
	m_MapdownloadCrc = 0;
	m_MapdownloadAmount = -1;
	m_MapdownloadTotalsize = -1;

	m_MapDetailsPresent = false;
	m_aMapDetailsName[0] = 0;
	m_MapDetailsSha256 = SHA256_ZEROED;
	m_MapDetailsCrc = 0;

	m_pDDNetInfoTask = NULL;
	m_aNews[0] = '\0';

	m_CurrentServerInfoRequestTime = -1;

	m_CurrentInput[0] = 0;
	m_CurrentInput[1] = 0;
	m_LastDummy = 0;
	m_LastDummy2 = 0;

	mem_zero(&m_aInputs, sizeof(m_aInputs));

	m_State = IClient::STATE_OFFLINE;
	m_aServerAddressStr[0] = 0;

	mem_zero(m_aSnapshots, sizeof(m_aSnapshots));
	m_SnapshotStorage[0].Init();
	m_SnapshotStorage[1].Init();
	m_ReceivedSnapshots[0] = 0;
	m_ReceivedSnapshots[1] = 0;

	m_VersionInfo.m_State = CVersionInfo::STATE_INIT;

	if(g_Config.m_ClDummy == 0)
		m_LastDummyConnectTime = 0;

	m_ReconnectTime = 0;

	m_GenerateTimeoutSeed = true;
}

// ----- send functions -----
int CClient::SendMsg(CMsgPacker *pMsg, int Flags)
{
	return SendMsgEx(pMsg, Flags, false);
}

int CClient::SendMsgEx(CMsgPacker *pMsg, int Flags, bool System)
{
	CNetChunk Packet;

	if(State() == IClient::STATE_OFFLINE)
		return 0;

	mem_zero(&Packet, sizeof(CNetChunk));

	Packet.m_ClientID = 0;
	Packet.m_pData = pMsg->Data();
	Packet.m_DataSize = pMsg->Size();

	// HACK: modify the message id in the packet and store the system flag
	if(*((unsigned char*)Packet.m_pData) == 1 && System && Packet.m_DataSize == 1)
	{
		dbg_break();
	}

	*((unsigned char*)Packet.m_pData) <<= 1;
	if(System)
		*((unsigned char*)Packet.m_pData) |= 1;

	if(Flags&MSGFLAG_VITAL)
		Packet.m_Flags |= NETSENDFLAG_VITAL;
	if(Flags&MSGFLAG_FLUSH)
		Packet.m_Flags |= NETSENDFLAG_FLUSH;

	if(Flags&MSGFLAG_RECORD)
	{
		for(int i = 0; i < RECORDER_MAX; i++)
			if(m_DemoRecorder[i].IsRecording())
				m_DemoRecorder[i].RecordMessage(Packet.m_pData, Packet.m_DataSize);
	}

	if(!(Flags&MSGFLAG_NOSEND))
	{
		m_NetClient[g_Config.m_ClDummy].Send(&Packet);
	}

	return 0;
}

void CClient::SendInfo()
{
	CMsgPacker Msg(NETMSG_INFO);
	Msg.AddString(GameClient()->NetVersion(), 128);
	Msg.AddString(m_Password, 128);
	SendMsgEx(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH);
}


void CClient::SendEnterGame()
{
	CMsgPacker Msg(NETMSG_ENTERGAME);
	SendMsgEx(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH);
}

void CClient::SendReady()
{
	CMsgPacker Msg(NETMSG_READY);
	SendMsgEx(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH);
}

void CClient::SendMapRequest()
{
	if(m_MapdownloadFile)
		io_close(m_MapdownloadFile);
	m_MapdownloadFile = Storage()->OpenFile(m_aMapdownloadFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	CMsgPacker Msg(NETMSG_REQUEST_MAP_DATA);
	Msg.AddInt(m_MapdownloadChunk);
	SendMsgEx(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH);
}

void CClient::RconAuth(const char *pName, const char *pPassword)
{
	if(RconAuthed())
		return;

	if(pPassword != m_RconPassword)
		str_copy(m_RconPassword, pPassword, sizeof(m_RconPassword));

	CMsgPacker Msg(NETMSG_RCON_AUTH);
	Msg.AddString(pName, 32);
	Msg.AddString(pPassword, 32);
	Msg.AddInt(1);
	SendMsgEx(&Msg, MSGFLAG_VITAL);
}

void CClient::Rcon(const char *pCmd)
{
	CMsgPacker Msg(NETMSG_RCON_CMD);
	Msg.AddString(pCmd, 256);
	SendMsgEx(&Msg, MSGFLAG_VITAL);
}

bool CClient::ConnectionProblems()
{
	return m_NetClient[g_Config.m_ClDummy].GotProblems() != 0;
}

void CClient::DirectInput(int *pInput, int Size)
{
	int i;
	CMsgPacker Msg(NETMSG_INPUT);
	Msg.AddInt(m_AckGameTick[g_Config.m_ClDummy]);
	Msg.AddInt(m_PredTick[g_Config.m_ClDummy]);
	Msg.AddInt(Size);

	for(i = 0; i < Size/4; i++)
		Msg.AddInt(pInput[i]);

	SendMsgEx(&Msg, 0);
}

void CClient::SendInput()
{
	int64 Now = time_get();

	if(m_PredTick[g_Config.m_ClDummy] <= 0)
		return;

	if(m_LastDummy != (bool)g_Config.m_ClDummy)
	{
		m_LastDummy = g_Config.m_ClDummy;
		GameClient()->OnDummySwap();
	}

	bool Force = false;
	// fetch input
	for(int Dummy = 0; Dummy < 2; Dummy++)
	{
		if(!m_DummyConnected && Dummy != 0)
		{
			break;
		}
		int i = g_Config.m_ClDummy ^ Dummy;
		int Size = GameClient()->OnSnapInput(m_aInputs[i][m_CurrentInput[i]].m_aData, Dummy, Force);

		if(Size)
		{
			// pack input
			CMsgPacker Msg(NETMSG_INPUT);
			Msg.AddInt(m_AckGameTick[i]);
			Msg.AddInt(m_PredTick[i]);
			Msg.AddInt(Size);

			m_aInputs[i][m_CurrentInput[i]].m_Tick = m_PredTick[i];
			m_aInputs[i][m_CurrentInput[i]].m_PredictedTime = m_PredictedTime.Get(Now);
			m_aInputs[i][m_CurrentInput[i]].m_Time = Now;

			// pack it
			for(int k = 0; k < Size/4; k++)
				Msg.AddInt(m_aInputs[i][m_CurrentInput[i]].m_aData[k]);

			m_CurrentInput[i]++;
			m_CurrentInput[i] %= 200;

			SendMsgExY(&Msg, MSGFLAG_FLUSH, true, i);
			// ugly workaround for dummy. we need to send input with dummy to prevent
			// prediction time resets. but if we do it too often, then it's
			// impossible to use grenade with frozen dummy that gets hammered...
			if(g_Config.m_ClDummyCopyMoves || m_CurrentInput[i] % 2)
				Force = true;
		}
	}
}

const char *CClient::LatestVersion()
{
	return m_aVersionStr;
}

// TODO: OPT: do this a lot smarter!
int *CClient::GetInput(int Tick)
{
	int Best = -1;
	for(int i = 0; i < 200; i++)
	{
		if(m_aInputs[g_Config.m_ClDummy][i].m_Tick <= Tick && (Best == -1 || m_aInputs[g_Config.m_ClDummy][Best].m_Tick < m_aInputs[g_Config.m_ClDummy][i].m_Tick))
			Best = i;
	}

	if(Best != -1)
		return (int *)m_aInputs[g_Config.m_ClDummy][Best].m_aData;
	return 0;
}

bool CClient::InputExists(int Tick)
{
	for(int i = 0; i < 200; i++)
		if(m_aInputs[g_Config.m_ClDummy][i].m_Tick == Tick)
			return true;
	return false;
}

// ------ state handling -----
void CClient::SetState(int s)
{
	if(m_State == IClient::STATE_QUITING)
		return;

	int Old = m_State;
	if(g_Config.m_Debug)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "state change. last=%d current=%d", m_State, s);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
	}
	m_State = s;
	if(Old != s)
	{
		GameClient()->OnStateChange(m_State, Old);

		if(s == IClient::STATE_OFFLINE && m_ReconnectTime == 0)
		{
			if(g_Config.m_ClReconnectFull > 0 && (str_find_nocase(ErrorString(), "full") || str_find_nocase(ErrorString(), "reserved")))
				m_ReconnectTime = time_get() + time_freq() * g_Config.m_ClReconnectFull;
			else if(g_Config.m_ClReconnectTimeout > 0 && (str_find_nocase(ErrorString(), "Timeout") || str_find_nocase(ErrorString(), "Too weak connection")))
				m_ReconnectTime = time_get() + time_freq() * g_Config.m_ClReconnectTimeout;
		}
	}
}

// called when the map is loaded and we should init for a new round
void CClient::OnEnterGame()
{
	// reset input
	int i;
	for(i = 0; i < 200; i++)
	{
		m_aInputs[0][i].m_Tick = -1;
		m_aInputs[1][i].m_Tick = -1;
	}
	m_CurrentInput[0] = 0;
	m_CurrentInput[1] = 0;

	// reset snapshots
	m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] = 0;
	m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV] = 0;
	m_SnapshotStorage[g_Config.m_ClDummy].PurgeAll();
	m_ReceivedSnapshots[g_Config.m_ClDummy] = 0;
	m_SnapshotParts = 0;
	m_PredTick[g_Config.m_ClDummy] = 0;
	m_CurrentRecvTick[g_Config.m_ClDummy] = 0;
	m_CurGameTick[g_Config.m_ClDummy] = 0;
	m_PrevGameTick[g_Config.m_ClDummy] = 0;

	if(g_Config.m_ClDummy == 0)
		m_LastDummyConnectTime = 0;

	GameClient()->OnEnterGame();
}

void CClient::EnterGame()
{
	if(State() == IClient::STATE_DEMOPLAYBACK)
		return;

	// now we will wait for two snapshots
	// to finish the connection
	SendEnterGame();
	OnEnterGame();

	ServerInfoRequest(); // fresh one for timeout protection
	m_aTimeoutCodeSent[0] = false;
	m_aTimeoutCodeSent[1] = false;
}

void GenerateTimeoutCode(char *pBuffer, unsigned Size, char *pSeed, const NETADDR &Addr, bool Dummy)
{
	md5_state_t Md5;
	md5_byte_t aDigest[16];
	md5_init(&Md5);

	const char *pDummy = Dummy ? "dummy" : "normal";

	md5_append(&Md5, (unsigned char *)pDummy, str_length(pDummy) + 1);
	md5_append(&Md5, (unsigned char *)pSeed, str_length(pSeed) + 1);
	md5_append(&Md5, (unsigned char *)&Addr, sizeof(Addr));
	md5_finish(&Md5, aDigest);

	unsigned short Random[8];
	mem_copy(Random, aDigest, sizeof(Random));
	generate_password(pBuffer, Size, Random, 8);
}

void CClient::GenerateTimeoutSeed()
{
	secure_random_password(g_Config.m_ClTimeoutSeed, sizeof(g_Config.m_ClTimeoutSeed), 16);
}

void CClient::GenerateTimeoutCodes()
{
	if(g_Config.m_ClTimeoutSeed[0])
	{
		for(int i = 0; i < 2; i++)
		{
			GenerateTimeoutCode(m_aTimeoutCodes[i], sizeof(m_aTimeoutCodes[i]), g_Config.m_ClTimeoutSeed, m_ServerAddress, i);

			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "timeout code '%s' (%s)", m_aTimeoutCodes[i], i == 0 ? "normal" : "dummy");
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);
		}
	}
	else
	{
		str_copy(m_aTimeoutCodes[0], g_Config.m_ClTimeoutCode, sizeof(m_aTimeoutCodes[0]));
		str_copy(m_aTimeoutCodes[1], g_Config.m_ClDummyTimeoutCode, sizeof(m_aTimeoutCodes[1]));
	}
}

void CClient::Connect(const char *pAddress, const char *pPassword)
{
	char aBuf[512];
	int Port = 8303;

	Disconnect();

	str_copy(m_aServerAddressStr, pAddress, sizeof(m_aServerAddressStr));
	str_copy(g_Config.m_DbgStressServer, pAddress, sizeof(g_Config.m_DbgStressServer));

	str_format(aBuf, sizeof(aBuf), "connecting to '%s'", m_aServerAddressStr);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);

	ServerInfoRequest();
	if(net_host_lookup(m_aServerAddressStr, &m_ServerAddress, m_NetClient[0].NetType()) != 0)
	{
		char aBufMsg[256];
		str_format(aBufMsg, sizeof(aBufMsg), "could not find the address of %s, connecting to localhost", aBuf);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBufMsg);
		net_host_lookup("localhost", &m_ServerAddress, m_NetClient[0].NetType());
	}

	str_copy(m_Password, g_Config.m_Password, sizeof(m_Password));

	m_RconAuthed[0] = 0;
	if(m_ServerAddress.port == 0)
		m_ServerAddress.port = Port;
	m_NetClient[0].Connect(&m_ServerAddress);
	SetState(IClient::STATE_CONNECTING);

	for(int i = 0; i < RECORDER_MAX; i++)
		if(m_DemoRecorder[i].IsRecording())
			DemoRecorder_Stop(i);

	m_InputtimeMarginGraph.Init(-150.0f, 150.0f);
	m_GametimeMarginGraph.Init(-150.0f, 150.0f);

	GenerateTimeoutCodes();

	GameClient()->OnTimeScore(1, false);
}

void CClient::DisconnectWithReason(const char *pReason)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "disconnecting. reason='%s'", pReason?pReason:"unknown");
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);

	// stop demo playback and recorder
	m_DemoPlayer.Stop();
	for(int i = 0; i < RECORDER_MAX; i++)
		DemoRecorder_Stop(i);

	//
	m_RconAuthed[0] = 0;
	m_UseTempRconCommands = 0;
	m_pConsole->DeregisterTempAll();
	m_NetClient[0].Disconnect(pReason);
	SetState(IClient::STATE_OFFLINE);
	m_pMap->Unload();

	// disable all downloads
	m_MapdownloadChunk = 0;
	if(m_pMapdownloadTask)
		m_pMapdownloadTask->Abort();
	if(m_MapdownloadFile)
		io_close(m_MapdownloadFile);
	m_MapdownloadFile = 0;
	m_MapdownloadSha256Present = false;
	m_MapdownloadSha256 = SHA256_ZEROED;
	m_MapdownloadCrc = 0;
	m_MapdownloadTotalsize = -1;
	m_MapdownloadAmount = 0;
	m_MapDetailsPresent = false;

	// clear the current server info
	mem_zero(&m_CurrentServerInfo, sizeof(m_CurrentServerInfo));
	mem_zero(&m_ServerAddress, sizeof(m_ServerAddress));

	// clear snapshots
	m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] = 0;
	m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV] = 0;
	m_ReceivedSnapshots[g_Config.m_ClDummy] = 0;
}

void CClient::Disconnect()
{
	if(m_DummyConnected)
		DummyDisconnect(0);
	if(m_State != IClient::STATE_OFFLINE)
		DisconnectWithReason(0);
}

bool CClient::DummyConnected()
{
	return m_DummyConnected;
}

bool CClient::DummyConnecting()
{
	return !m_DummyConnected && m_LastDummyConnectTime > 0 && m_LastDummyConnectTime + GameTickSpeed() * 5 > GameTick();
}

void CClient::DummyConnect()
{
	if(m_LastDummyConnectTime > 0 && m_LastDummyConnectTime + GameTickSpeed() * 5 > GameTick())
		return;

	if(m_NetClient[0].State() != NET_CONNSTATE_ONLINE && m_NetClient[0].State() != NET_CONNSTATE_PENDING)
		return;

	if(m_DummyConnected)
		return;

	m_LastDummyConnectTime = GameTick();

	m_RconAuthed[1] = 0;

	m_DummySendConnInfo = true;

	g_Config.m_ClDummyCopyMoves = 0;
	g_Config.m_ClDummyHammer = 0;

	//connecting to the server
	m_NetClient[1].Connect(&m_ServerAddress);

	GameClient()->OnTimeScore(1, true);
}

void CClient::DummyDisconnect(const char *pReason)
{
	if(!m_DummyConnected)
		return;

	m_NetClient[1].Disconnect(pReason);
	g_Config.m_ClDummy = 0;
	m_RconAuthed[1] = 0;
	m_DummyConnected = false;
	GameClient()->OnDummyDisconnect();
}

int CClient::SendMsgExY(CMsgPacker *pMsg, int Flags, bool System, int NetClient)
{
	CNetChunk Packet;

	mem_zero(&Packet, sizeof(CNetChunk));

	Packet.m_ClientID = 0;
	Packet.m_pData = pMsg->Data();
	Packet.m_DataSize = pMsg->Size();

	// HACK: modify the message id in the packet and store the system flag
	if(*((unsigned char*)Packet.m_pData) == 1 && System && Packet.m_DataSize == 1)
	{
		dbg_break();
	}

	*((unsigned char*)Packet.m_pData) <<= 1;
	if(System)
		*((unsigned char*)Packet.m_pData) |= 1;

	if(Flags&MSGFLAG_VITAL)
		Packet.m_Flags |= NETSENDFLAG_VITAL;
	if(Flags&MSGFLAG_FLUSH)
		Packet.m_Flags |= NETSENDFLAG_FLUSH;

	m_NetClient[NetClient].Send(&Packet);
	return 0;
}

void CClient::GetServerInfo(CServerInfo *pServerInfo)
{
	mem_copy(pServerInfo, &m_CurrentServerInfo, sizeof(m_CurrentServerInfo));

	if(m_DemoPlayer.IsPlaying() && g_Config.m_ClDemoAssumeRace)
		str_copy(pServerInfo->m_aGameType, "DDraceNetwork", 14);
}

void CClient::ServerInfoRequest()
{
	mem_zero(&m_CurrentServerInfo, sizeof(m_CurrentServerInfo));
	m_CurrentServerInfoRequestTime = 0;
}

int CClient::LoadData()
{
	return 1;
}

// ---

void *CClient::SnapGetItem(int SnapID, int Index, CSnapItem *pItem)
{
	CSnapshotItem *i;
	dbg_assert(SnapID >= 0 && SnapID < NUM_SNAPSHOT_TYPES, "invalid SnapID");
	i = m_aSnapshots[g_Config.m_ClDummy][SnapID]->m_pAltSnap->GetItem(Index);
	pItem->m_DataSize = m_aSnapshots[g_Config.m_ClDummy][SnapID]->m_pAltSnap->GetItemSize(Index);
	pItem->m_Type = m_aSnapshots[g_Config.m_ClDummy][SnapID]->m_pAltSnap->GetItemType(Index);
	pItem->m_ID = i->ID();
	return (void *)i->Data();
}

void CClient::SnapInvalidateItem(int SnapID, int Index)
{
	CSnapshotItem *i;
	dbg_assert(SnapID >= 0 && SnapID < NUM_SNAPSHOT_TYPES, "invalid SnapID");
	i = m_aSnapshots[g_Config.m_ClDummy][SnapID]->m_pAltSnap->GetItem(Index);
	if(i)
	{
		if((char *)i < (char *)m_aSnapshots[g_Config.m_ClDummy][SnapID]->m_pAltSnap || (char *)i > (char *)m_aSnapshots[g_Config.m_ClDummy][SnapID]->m_pAltSnap + m_aSnapshots[g_Config.m_ClDummy][SnapID]->m_SnapSize)
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", "snap invalidate problem");
		if((char *)i >= (char *)m_aSnapshots[g_Config.m_ClDummy][SnapID]->m_pSnap && (char *)i < (char *)m_aSnapshots[g_Config.m_ClDummy][SnapID]->m_pSnap + m_aSnapshots[g_Config.m_ClDummy][SnapID]->m_SnapSize)
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", "snap invalidate problem");
		i->m_TypeAndID = -1;
	}
}

void *CClient::SnapFindItem(int SnapID, int Type, int ID)
{
	// TODO: linear search. should be fixed.
	int i;

	if(!m_aSnapshots[g_Config.m_ClDummy][SnapID])
		return 0x0;

	for(i = 0; i < m_aSnapshots[g_Config.m_ClDummy][SnapID]->m_pSnap->NumItems(); i++)
	{
		CSnapshotItem *pItem = m_aSnapshots[g_Config.m_ClDummy][SnapID]->m_pAltSnap->GetItem(i);
		if(m_aSnapshots[g_Config.m_ClDummy][SnapID]->m_pAltSnap->GetItemType(i) == Type && pItem->ID() == ID)
			return (void *)pItem->Data();
	}
	return 0x0;
}

int CClient::SnapNumItems(int SnapID)
{
	dbg_assert(SnapID >= 0 && SnapID < NUM_SNAPSHOT_TYPES, "invalid SnapID");
	if(!m_aSnapshots[g_Config.m_ClDummy][SnapID])
		return 0;
	return m_aSnapshots[g_Config.m_ClDummy][SnapID]->m_pSnap->NumItems();
}

void CClient::SnapSetStaticsize(int ItemType, int Size)
{
	m_SnapshotDelta.SetStaticsize(ItemType, Size);
}


void CClient::DebugRender()
{
	static NETSTATS Prev, Current;
	static int64 LastSnap = 0;
	static float FrameTimeAvg = 0;
	char aBuffer[512];

	if(!g_Config.m_Debug)
		return;

	if(time_get()-LastSnap > time_freq())
	{
		LastSnap = time_get();
		Prev = Current;
		net_stats(&Current);
	}

	/*
		eth = 14
		ip = 20
		udp = 8
		total = 42
	*/
	FrameTimeAvg = FrameTimeAvg*0.9f + m_RenderFrameTime*0.1f;
	str_format(aBuffer, sizeof(aBuffer), "ticks: %8d %8d gfxmem: %dk fps: %3d",
		m_CurGameTick[g_Config.m_ClDummy], m_PredTick[g_Config.m_ClDummy],
		0,
		(int)(1.0f/FrameTimeAvg + 0.5f));


	{
		int SendPackets = (Current.sent_packets-Prev.sent_packets);
		int SendBytes = (Current.sent_bytes-Prev.sent_bytes);
		int SendTotal = SendBytes + SendPackets*42;
		int RecvPackets = (Current.recv_packets-Prev.recv_packets);
		int RecvBytes = (Current.recv_bytes-Prev.recv_bytes);
		int RecvTotal = RecvBytes + RecvPackets*42;

		if(!SendPackets) SendPackets++;
		if(!RecvPackets) RecvPackets++;
		str_format(aBuffer, sizeof(aBuffer), "send: %3d %5d+%4d=%5d (%3d kbps) avg: %5d\nrecv: %3d %5d+%4d=%5d (%3d kbps) avg: %5d",
			SendPackets, SendBytes, SendPackets*42, SendTotal, (SendTotal*8)/1024, SendBytes/SendPackets,
			RecvPackets, RecvBytes, RecvPackets*42, RecvTotal, (RecvTotal*8)/1024, RecvBytes/RecvPackets);
	}

	// render rates
	{
		int y = 0;
		int i;
		for(i = 0; i < 256; i++)
		{
			if(m_SnapshotDelta.GetDataRate(i))
			{
				str_format(aBuffer, sizeof(aBuffer), "%4d %20s: %8d %8d %8d", i, GameClient()->GetItemName(i), m_SnapshotDelta.GetDataRate(i)/8, m_SnapshotDelta.GetDataUpdates(i),
					(m_SnapshotDelta.GetDataRate(i)/m_SnapshotDelta.GetDataUpdates(i))/8);
				y++;
			}
		}
	}

	str_format(aBuffer, sizeof(aBuffer), "pred: %d ms", GetPredictionTime());
}

void CClient::Restart()
{
	char aBuf[512];
	shell_execute(Storage()->GetBinaryPath(PLAT_CLIENT_EXEC, aBuf, sizeof aBuf));
	Quit();
}

void CClient::Quit()
{
	SetState(IClient::STATE_QUITING);
}

const char *CClient::ErrorString()
{
	return m_NetClient[0].ErrorString();
}

void CClient::Render()
{
	GameClient()->OnRender();
	DebugRender();

	if(State() == IClient::STATE_ONLINE && g_Config.m_ClAntiPingLimit)
	{
		int64 Now = time_get();
		g_Config.m_ClAntiPing = (m_PredictedTime.Get(Now)-m_GameTime[g_Config.m_ClDummy].Get(Now))*1000/(float)time_freq() > g_Config.m_ClAntiPingLimit;
	}
}

vec3 CClient::GetColorV3(int v)
{
	return HslToRgb(vec3(((v>>16)&0xff)/255.0f, ((v>>8)&0xff)/255.0f, 0.5f+(v&0xff)/255.0f*0.5f));
}

const char *CClient::LoadMap(const char *pName, const char *pFilename, SHA256_DIGEST *pWantedSha256, unsigned WantedCrc)
{
	static char s_aErrorMsg[128];

	SetState(IClient::STATE_LOADING);

	if(!m_pMap->Load(pFilename))
	{
		str_format(s_aErrorMsg, sizeof(s_aErrorMsg), "map '%s' not found", pFilename);
		return s_aErrorMsg;
	}

	if(pWantedSha256 && m_pMap->Sha256() != *pWantedSha256)
	{
		char aWanted[SHA256_MAXSTRSIZE];
		char aGot[SHA256_MAXSTRSIZE];
		sha256_str(*pWantedSha256, aWanted, sizeof(aWanted));
		sha256_str(m_pMap->Sha256(), aGot, sizeof(aWanted));
		str_format(s_aErrorMsg, sizeof(s_aErrorMsg), "map differs from the server. %s != %s", aGot, aWanted);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", s_aErrorMsg);
		m_pMap->Unload();
		return s_aErrorMsg;
	}

	// get the crc of the map
	if(m_pMap->Crc() != WantedCrc)
	{
		str_format(s_aErrorMsg, sizeof(s_aErrorMsg), "map differs from the server. %08x != %08x", m_pMap->Crc(), WantedCrc);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", s_aErrorMsg);
		m_pMap->Unload();
		return s_aErrorMsg;
	}

	// stop demo recording if we loaded a new map
	for(int i = 0; i < RECORDER_MAX; i++)
		DemoRecorder_Stop(i);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "loaded map '%s'", pFilename);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);
	m_ReceivedSnapshots[g_Config.m_ClDummy] = 0;

	str_copy(m_aCurrentMap, pName, sizeof(m_aCurrentMap));
	str_copy(m_aCurrentMapPath, pFilename, sizeof(m_aCurrentMapPath));

	return 0;
}

const char *CClient::LoadMapSearch(const char *pMapName, SHA256_DIGEST *pWantedSha256, int WantedCrc)
{
	const char *pError = 0;
	char aBuf[512];
	char aWanted[256];
	char aWantedSha256[SHA256_MAXSTRSIZE];

	aWanted[0] = 0;

	if(pWantedSha256)
	{
		sha256_str(*pWantedSha256, aWantedSha256, sizeof(aWantedSha256));
		str_format(aWanted, sizeof(aWanted), " sha256=%s", aWantedSha256);
	}

	str_format(aBuf, sizeof(aBuf), "loading map, map=%s wanted%s crc=%08x", pMapName, aWanted, WantedCrc);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);
	SetState(IClient::STATE_LOADING);

	// try the normal maps folder
	str_format(aBuf, sizeof(aBuf), "maps/%s.map", pMapName);
	pError = LoadMap(pMapName, aBuf, pWantedSha256, WantedCrc);
	if(!pError)
		return pError;

	// try the downloaded maps
	if(pWantedSha256)
	{
		str_format(aBuf, sizeof(aBuf), "downloadedmaps/%s_%08x_%s.map", pMapName, WantedCrc, aWantedSha256);
		pError = LoadMap(pMapName, aBuf, pWantedSha256, WantedCrc);
		if(!pError)
			return pError;
	}

	// try the downloaded maps folder without appending the sha256
	str_format(aBuf, sizeof(aBuf), "downloadedmaps/%s_%08x.map", pMapName, WantedCrc);
	pError = LoadMap(pMapName, aBuf, pWantedSha256, WantedCrc);
	if(!pError)
		return pError;

	// search for the map within subfolders
	char aFilename[128];
	str_format(aFilename, sizeof(aFilename), "%s.map", pMapName);
	if(Storage()->FindFile(aFilename, "maps", IStorage::TYPE_ALL, aBuf, sizeof(aBuf)))
		pError = LoadMap(pMapName, aBuf, pWantedSha256, WantedCrc);

	return pError;
}

int CClient::PlayerScoreNameComp(const void *a, const void *b)
{
	CServerInfo::CClient *p0 = (CServerInfo::CClient *)a;
	CServerInfo::CClient *p1 = (CServerInfo::CClient *)b;
	if(p0->m_Player && !p1->m_Player)
		return -1;
	if(!p0->m_Player && p1->m_Player)
		return 1;
	if(p0->m_Score > p1->m_Score)
		return -1;
	if(p0->m_Score < p1->m_Score)
		return 1;
	return str_comp_nocase(p0->m_aName, p1->m_aName);
}

void CClient::ProcessConnlessPacket(CNetChunk *pPacket)
{
	//server count from master server
	if(pPacket->m_DataSize == (int)sizeof(SERVERBROWSE_COUNT) + 2 && mem_comp(pPacket->m_pData, SERVERBROWSE_COUNT, sizeof(SERVERBROWSE_COUNT)) == 0)
	{
		unsigned char *pP = (unsigned char*) pPacket->m_pData;
		pP += sizeof(SERVERBROWSE_COUNT);
		int ServerCount = ((*pP)<<8) | *(pP+1);
		int ServerID = -1;
		for(int i = 0; i < IMasterServer::MAX_MASTERSERVERS; i++)
		{
			if(!m_pMasterServer->IsValid(i))
				continue;
			NETADDR tmp = m_pMasterServer->GetAddr(i);
			if(net_addr_comp(&pPacket->m_Address, &tmp) == 0)
			{
				ServerID = i;
				break;
			}
		}
		if(ServerCount > -1 && ServerID != -1)
		{
			m_pMasterServer->SetCount(ServerID, ServerCount);
			if(g_Config.m_Debug)
				dbg_msg("mastercount", "server %d got %d servers", ServerID, ServerCount);
		}
	}
	// server list from master server
	if(pPacket->m_DataSize >= (int)sizeof(SERVERBROWSE_LIST) &&
		mem_comp(pPacket->m_pData, SERVERBROWSE_LIST, sizeof(SERVERBROWSE_LIST)) == 0)
	{
		// check for valid master server address
		bool Valid = false;
		for(int i = 0; i < IMasterServer::MAX_MASTERSERVERS; ++i)
		{
			if(m_pMasterServer->IsValid(i))
			{
				NETADDR Addr = m_pMasterServer->GetAddr(i);
				if(net_addr_comp(&pPacket->m_Address, &Addr) == 0)
				{
					Valid = true;
					break;
				}
			}
		}
		if(!Valid)
			return;

		int Size = pPacket->m_DataSize-sizeof(SERVERBROWSE_LIST);
		int Num = Size/sizeof(CMastersrvAddr);
		CMastersrvAddr *pAddrs = (CMastersrvAddr *)((char*)pPacket->m_pData+sizeof(SERVERBROWSE_LIST));
		for(int i = 0; i < Num; i++)
		{
			NETADDR Addr;

			static unsigned char s_IPV4Mapping[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF };

			// copy address
			if(!mem_comp(s_IPV4Mapping, pAddrs[i].m_aIp, sizeof(s_IPV4Mapping)))
			{
				mem_zero(&Addr, sizeof(Addr));
				Addr.type = NETTYPE_IPV4;
				Addr.ip[0] = pAddrs[i].m_aIp[12];
				Addr.ip[1] = pAddrs[i].m_aIp[13];
				Addr.ip[2] = pAddrs[i].m_aIp[14];
				Addr.ip[3] = pAddrs[i].m_aIp[15];
			}
			else
			{
				Addr.type = NETTYPE_IPV6;
				mem_copy(Addr.ip, pAddrs[i].m_aIp, sizeof(Addr.ip));
			}
			Addr.port = (pAddrs[i].m_aPort[0]<<8) | pAddrs[i].m_aPort[1];

			m_ServerBrowser.Set(Addr, IServerBrowser::SET_MASTER_ADD, -1, 0x0);
		}
	}

	// server info
	if(pPacket->m_DataSize >= (int)sizeof(SERVERBROWSE_INFO))
	{
		int Type = -1;
		if(mem_comp(pPacket->m_pData, SERVERBROWSE_INFO, sizeof(SERVERBROWSE_INFO)) == 0)
			Type = SERVERINFO_VANILLA;
		else if(mem_comp(pPacket->m_pData, SERVERBROWSE_INFO_64_LEGACY, sizeof(SERVERBROWSE_INFO_64_LEGACY)) == 0)
			Type = SERVERINFO_64_LEGACY;
		else if(mem_comp(pPacket->m_pData, SERVERBROWSE_INFO_EXTENDED, sizeof(SERVERBROWSE_INFO_EXTENDED)) == 0)
			Type = SERVERINFO_EXTENDED;
		else if(mem_comp(pPacket->m_pData, SERVERBROWSE_INFO_EXTENDED_MORE, sizeof(SERVERBROWSE_INFO_EXTENDED_MORE)) == 0)
			Type = SERVERINFO_EXTENDED_MORE;

		if(Type != -1)
		{
			void *pData = (unsigned char *)pPacket->m_pData + sizeof(SERVERBROWSE_INFO);
			int DataSize = pPacket->m_DataSize - sizeof(SERVERBROWSE_INFO);
			ProcessServerInfo(Type, &pPacket->m_Address, pData, DataSize);
		}
	}
}

static int SavedServerInfoType(int Type)
{
	if(Type == SERVERINFO_EXTENDED_MORE)
		return SERVERINFO_EXTENDED;

	return Type;
}

void CClient::ProcessServerInfo(int RawType, NETADDR *pFrom, const void *pData, int DataSize)
{
	CServerBrowser::CServerEntry *pEntry = m_ServerBrowser.Find(*pFrom);

	CServerInfo Info = {0};
	int SavedType = SavedServerInfoType(RawType);
	if((SavedType == SERVERINFO_64_LEGACY || SavedType == SERVERINFO_EXTENDED)
		&& pEntry && pEntry->m_GotInfo && SavedType == pEntry->m_Info.m_Type)
	{
		Info = pEntry->m_Info;
	}

	Info.m_Type = SavedType;

	net_addr_str(pFrom, Info.m_aAddress, sizeof(Info.m_aAddress), true);

	CUnpacker Up;
	Up.Reset(pData, DataSize);

	#define GET_STRING(array) str_copy(array, Up.GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES), sizeof(array))
	#define GET_INT(integer) (integer) = str_toint(Up.GetString())

	int Offset = 0; // Only used for SavedType == SERVERINFO_64_LEGACY
	int Token;
	int PacketNo = 0; // Only used if SavedType == SERVERINFO_EXTENDED

	GET_INT(Token);
	if(RawType != SERVERINFO_EXTENDED_MORE)
	{
		GET_STRING(Info.m_aVersion);
		GET_STRING(Info.m_aName);
		GET_STRING(Info.m_aMap);

		if(SavedType == SERVERINFO_EXTENDED)
		{
			GET_INT(Info.m_MapCrc);
			GET_INT(Info.m_MapSize);
		}

		GET_STRING(Info.m_aGameType);
		GET_INT(Info.m_Flags);
		GET_INT(Info.m_NumPlayers);
		GET_INT(Info.m_MaxPlayers);
		GET_INT(Info.m_NumClients);
		GET_INT(Info.m_MaxClients);
		if(Info.m_aMap[0])
			Info.m_HasRank = m_ServerBrowser.HasRank(Info.m_aMap);

		// don't add invalid info to the server browser list
		if(Info.m_NumClients < 0 || Info.m_MaxClients < 0 ||
			Info.m_NumPlayers < 0 || Info.m_MaxPlayers < 0 ||
			Info.m_NumPlayers > Info.m_NumClients || Info.m_MaxPlayers > Info.m_MaxClients)
		{
			return;
		}

		switch(SavedType)
		{
		case SERVERINFO_VANILLA:
			if(Info.m_MaxPlayers > VANILLA_MAX_CLIENTS ||
				Info.m_MaxClients > VANILLA_MAX_CLIENTS)
			{
				return;
			}
			break;
		case SERVERINFO_64_LEGACY:
			if(Info.m_MaxPlayers > MAX_CLIENTS ||
				Info.m_MaxClients > MAX_CLIENTS)
			{
				return;
			}
			break;
		case SERVERINFO_EXTENDED:
			if(Info.m_NumPlayers > Info.m_NumClients)
				return;
			break;
		default:
			dbg_assert(false, "unknown serverinfo type");
		}

		if(SavedType == SERVERINFO_64_LEGACY)
			Offset = Up.GetInt();

		// Check for valid offset.
		if(Offset < 0)
			return;

		if(SavedType == SERVERINFO_EXTENDED)
			PacketNo = 0;
	}
	else
	{
		GET_INT(PacketNo);
		// 0 needs to be excluded because that's reserved for the main packet.
		if(PacketNo <= 0 || PacketNo >= 64)
			return;
	}

	bool DuplicatedPacket = false;
	if(SavedType == SERVERINFO_EXTENDED)
	{
		Up.GetString(); // extra info, reserved

		uint64 Flag = (uint64)1 << PacketNo;
		DuplicatedPacket = Info.m_ReceivedPackets&Flag;
		Info.m_ReceivedPackets |= Flag;
	}

	bool IgnoreError = false;
	for(int i = Offset; i < MAX_CLIENTS && Info.m_NumReceivedClients < MAX_CLIENTS && !Up.Error(); i++)
	{
		CServerInfo::CClient *pClient = &Info.m_aClients[Info.m_NumReceivedClients];
		GET_STRING(pClient->m_aName);
		if(Up.Error())
		{
			// Packet end, no problem unless it happens during one
			// player info, so ignore the error.
			IgnoreError = true;
			break;
		}
		GET_STRING(pClient->m_aClan);
		GET_INT(pClient->m_Country);
		GET_INT(pClient->m_Score);
		GET_INT(pClient->m_Player);
		if(SavedType == SERVERINFO_EXTENDED)
		{
			Up.GetString(); // extra info, reserved
		}
		if(!Up.Error())
		{
			if(SavedType == SERVERINFO_64_LEGACY)
			{
				uint64 Flag = (uint64)1 << i;
				if(!(Info.m_ReceivedPackets&Flag))
				{
					Info.m_ReceivedPackets |= Flag;
					Info.m_NumReceivedClients++;
				}
			}
			else
			{
				Info.m_NumReceivedClients++;
			}
		}
	}

	if(!Up.Error() || IgnoreError)
	{
		qsort(Info.m_aClients, Info.m_NumReceivedClients, sizeof(*Info.m_aClients), PlayerScoreNameComp);

		if(!DuplicatedPacket && (!pEntry || !pEntry->m_GotInfo || SavedType >= pEntry->m_Info.m_Type))
		{
			m_ServerBrowser.Set(*pFrom, IServerBrowser::SET_TOKEN, Token, &Info);
			pEntry = m_ServerBrowser.Find(*pFrom);

			if(SavedType == SERVERINFO_VANILLA && Is64Player(&Info) && pEntry)
			{
				pEntry->m_Request64Legacy = true;
				// Force a quick update.
				m_ServerBrowser.RequestImpl64(pEntry->m_Addr, pEntry);
			}
		}

		// Player info is irrelevant for the client (while connected),
		// it gets its info from elsewhere.
		//
		// SERVERINFO_EXTENDED_MORE doesn't carry any server
		// information, so just skip it.
		if(net_addr_comp(&m_ServerAddress, pFrom) == 0 && RawType != SERVERINFO_EXTENDED_MORE)
		{
			// Only accept server info that has a type that is
			// newer or equal to something the server already sent
			// us.
			if(SavedType >= m_CurrentServerInfo.m_Type)
			{
				mem_copy(&m_CurrentServerInfo, &Info, sizeof(m_CurrentServerInfo));
				m_CurrentServerInfo.m_NetAddr = m_ServerAddress;
				m_CurrentServerInfoRequestTime = -1;
			}
		}
	}

	#undef GET_STRING
	#undef GET_INT
}

static void FormatMapDownloadFilename(const char *pName, SHA256_DIGEST *pSha256, int Crc, bool Temp, char *pBuffer, int BufferSize)
{
	char aSha256[SHA256_MAXSTRSIZE + 1];
	aSha256[0] = 0;
	if(pSha256)
	{
		aSha256[0] = '_';
		sha256_str(*pSha256, aSha256 + 1, sizeof(aSha256) - 1);
	}

	str_format(pBuffer, BufferSize, "%s_%08x%s.map%s",
		pName,
		Crc,
		aSha256,
		Temp ? ".tmp" : "");
}

void CClient::ProcessServerPacket(CNetChunk *pPacket)
{
	CUnpacker Unpacker;
	Unpacker.Reset(pPacket->m_pData, pPacket->m_DataSize);
	CMsgPacker Packer(NETMSG_EX);

	// unpack msgid and system flag
	int Msg;
	bool Sys;
	CUuid Uuid;

	int Result = UnpackMessageID(&Msg, &Sys, &Uuid, &Unpacker, &Packer);
	if(Result == UNPACKMESSAGE_ERROR)
	{
		return;
	}
	else if(Result == UNPACKMESSAGE_ANSWER)
	{
		SendMsgEx(&Packer, MSGFLAG_VITAL, true);
	}

	if(Sys)
	{
		// system message
		if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_MAP_DETAILS)
		{
			const char *pMap = Unpacker.GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);
			SHA256_DIGEST *pMapSha256 = (SHA256_DIGEST *)Unpacker.GetRaw(sizeof(*pMapSha256));
			int MapCrc = Unpacker.GetInt();

			if(Unpacker.Error())
			{
				return;
			}

			m_MapDetailsPresent = true;
			str_copy(m_aMapDetailsName, pMap, sizeof(m_aMapDetailsName));
			m_MapDetailsSha256 = *pMapSha256;
			m_MapDetailsCrc = MapCrc;
		}
		else if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_MAP_CHANGE)
		{
			bool MapDetailsWerePresent = m_MapDetailsPresent;
			m_MapDetailsPresent = false;

			const char *pMap = Unpacker.GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);
			int MapCrc = Unpacker.GetInt();
			int MapSize = Unpacker.GetInt();
			const char *pError = 0;

			if(Unpacker.Error())
				return;

			if(m_DummyConnected)
				DummyDisconnect(0);

			for(int i = 0; pMap[i]; i++) // protect the player from nasty map names
			{
				if(pMap[i] == '/' || pMap[i] == '\\')
					pError = "strange character in map name";
			}

			if(MapSize < 0)
				pError = "invalid map size";

			if(pError)
				DisconnectWithReason(pError);
			else
			{
				SHA256_DIGEST *pMapSha256 = 0;
				if(MapDetailsWerePresent && str_comp(m_aMapDetailsName, pMap) == 0 && m_MapDetailsCrc == MapCrc)
				{
					pMapSha256 = &m_MapDetailsSha256;
				}
				pError = LoadMapSearch(pMap, pMapSha256, MapCrc);

				if(!pError)
				{
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", "loading done");
					SendReady();
				}
				else
				{
					char aFilename[256];
					FormatMapDownloadFilename(pMap, pMapSha256, MapCrc, false, aFilename, sizeof(aFilename));

					char aTempFilename[256];
					FormatMapDownloadFilename(pMap, pMapSha256, MapCrc, true, aTempFilename, sizeof(aTempFilename));

					str_format(m_aMapdownloadFilename, sizeof(m_aMapdownloadFilename), "downloadedmaps/%s", aTempFilename);

					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "starting to download map to '%s'", m_aMapdownloadFilename);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", aBuf);

					m_MapdownloadChunk = 0;
					str_copy(m_aMapdownloadName, pMap, sizeof(m_aMapdownloadName));

					m_MapdownloadSha256Present = (bool)pMapSha256;
					m_MapdownloadSha256 = pMapSha256 ? *pMapSha256 : SHA256_ZEROED;
					m_MapdownloadCrc = MapCrc;
					m_MapdownloadTotalsize = MapSize;
					m_MapdownloadAmount = 0;

					ResetMapDownload();

					if(pMapSha256 && g_Config.m_ClHttpMapDownload)
					{
						char aUrl[256];
						char aEscaped[256];
						EscapeUrl(aEscaped, sizeof(aEscaped), aFilename);
						str_format(aUrl, sizeof(aUrl), "%s/%s", g_Config.m_ClDDNetMapDownloadUrl, aEscaped);

						m_pMapdownloadTask = std::make_shared<CGetFile>(Storage(), aUrl, m_aMapdownloadFilename, IStorage::TYPE_SAVE, true);
						Engine()->AddJob(m_pMapdownloadTask);
					}
					else
						SendMapRequest();
				}
			}
		}
		else if(Msg == NETMSG_MAP_DATA)
		{
			int Last = Unpacker.GetInt();
			int MapCRC = Unpacker.GetInt();
			int Chunk = Unpacker.GetInt();
			int Size = Unpacker.GetInt();
			const unsigned char *pData = Unpacker.GetRaw(Size);

			// check for errors
			if(Unpacker.Error() || Size <= 0 || MapCRC != m_MapdownloadCrc || Chunk != m_MapdownloadChunk || !m_MapdownloadFile)
				return;

			io_write(m_MapdownloadFile, pData, Size);

			m_MapdownloadAmount += Size;

			if(Last)
			{
				if(m_MapdownloadFile)
				{
					io_close(m_MapdownloadFile);
					m_MapdownloadFile = 0;
				}
				FinishMapDownload();
			}
			else
			{
				// request new chunk
				m_MapdownloadChunk++;

				CMsgPacker Msg(NETMSG_REQUEST_MAP_DATA);
				Msg.AddInt(m_MapdownloadChunk);
				SendMsgEx(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH);

				if(g_Config.m_Debug)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "requested chunk %d", m_MapdownloadChunk);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client/network", aBuf);
				}
			}
		}
		else if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_CON_READY)
		{
			GameClient()->OnConnected();
		}
		else if(Msg == NETMSG_PING)
		{
			CMsgPacker Msg(NETMSG_PING_REPLY);
			SendMsgEx(&Msg, 0);
		}
		else if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_RCON_CMD_ADD)
		{
			const char *pName = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			const char *pHelp = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			const char *pParams = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(Unpacker.Error() == 0)
				m_pConsole->RegisterTemp(pName, pParams, CFGFLAG_SERVER, pHelp);
		}
		else if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_RCON_CMD_REM)
		{
			const char *pName = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(Unpacker.Error() == 0)
				m_pConsole->DeregisterTemp(pName);
		}
		else if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_RCON_AUTH_STATUS)
		{
			int Result = Unpacker.GetInt();
			if(Unpacker.Error() == 0)
				m_RconAuthed[g_Config.m_ClDummy] = Result;
			int Old = m_UseTempRconCommands;
			m_UseTempRconCommands = Unpacker.GetInt();
			if(Unpacker.Error() != 0)
				m_UseTempRconCommands = 0;
			if(Old != 0 && m_UseTempRconCommands == 0)
				m_pConsole->DeregisterTempAll();
		}
		else if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_RCON_LINE)
		{
			const char *pLine = Unpacker.GetString();
			if(Unpacker.Error() == 0)
				GameClient()->OnRconLine(pLine);
		}
		else if(Msg == NETMSG_PING_REPLY)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "latency %.2f", (time_get() - m_PingStartTime)*1000 / (float)time_freq());
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client/network", aBuf);
		}
		else if(Msg == NETMSG_INPUTTIMING)
		{
			int InputPredTick = Unpacker.GetInt();
			int TimeLeft = Unpacker.GetInt();
			int64 Now = time_get();

			// adjust our prediction time
			int64 Target = 0;
			for(int k = 0; k < 200; k++)
			{
				if(m_aInputs[g_Config.m_ClDummy][k].m_Tick == InputPredTick)
				{
					Target = m_aInputs[g_Config.m_ClDummy][k].m_PredictedTime + (Now - m_aInputs[g_Config.m_ClDummy][k].m_Time);
					Target = Target - (int64)(((TimeLeft-PREDICTION_MARGIN)/1000.0f)*time_freq());
					break;
				}
			}

			if(Target)
				m_PredictedTime.Update(&m_InputtimeMarginGraph, Target, TimeLeft, 1);
		}
		else if(Msg == NETMSG_SNAP || Msg == NETMSG_SNAPSINGLE || Msg == NETMSG_SNAPEMPTY)
		{
			int NumParts = 1;
			int Part = 0;
			int GameTick = Unpacker.GetInt();
			int DeltaTick = GameTick-Unpacker.GetInt();
			int PartSize = 0;
			int Crc = 0;
			int CompleteSize = 0;
			const char *pData = 0;

			// only allow packets from the server we actually want
			if(net_addr_comp(&pPacket->m_Address, &m_ServerAddress))
				return;

			// we are not allowed to process snapshot yet
			if(State() < IClient::STATE_LOADING)
				return;

			if(Msg == NETMSG_SNAP)
			{
				NumParts = Unpacker.GetInt();
				Part = Unpacker.GetInt();
			}

			if(Msg != NETMSG_SNAPEMPTY)
			{
				Crc = Unpacker.GetInt();
				PartSize = Unpacker.GetInt();
			}

			pData = (const char *)Unpacker.GetRaw(PartSize);

			if(Unpacker.Error() || NumParts < 1 || Part < 0 || PartSize < 0)
				return;

			if(GameTick >= m_CurrentRecvTick[g_Config.m_ClDummy])
			{
				if(GameTick != m_CurrentRecvTick[g_Config.m_ClDummy])
				{
					m_SnapshotParts = 0;
					m_CurrentRecvTick[g_Config.m_ClDummy] = GameTick;
				}

				mem_copy((char*)m_aSnapshotIncomingData + Part*MAX_SNAPSHOT_PACKSIZE, pData, clamp(PartSize, 0, (int)sizeof(m_aSnapshotIncomingData) - Part*MAX_SNAPSHOT_PACKSIZE));
				m_SnapshotParts |= 1<<Part;

				if(m_SnapshotParts == (unsigned)((1<<NumParts)-1))
				{
					static CSnapshot Emptysnap;
					CSnapshot *pDeltaShot = &Emptysnap;
					int PurgeTick;
					void *pDeltaData;
					int DeltaSize;
					unsigned char aTmpBuffer2[CSnapshot::MAX_SIZE];
					unsigned char aTmpBuffer3[CSnapshot::MAX_SIZE];
					CSnapshot *pTmpBuffer3 = (CSnapshot*)aTmpBuffer3;	// Fix compiler warning for strict-aliasing
					int SnapSize;

					CompleteSize = (NumParts-1) * MAX_SNAPSHOT_PACKSIZE + PartSize;

					// reset snapshoting
					m_SnapshotParts = 0;

					// find snapshot that we should use as delta
					Emptysnap.Clear();

					// find delta
					if(DeltaTick >= 0)
					{
						int DeltashotSize = m_SnapshotStorage[g_Config.m_ClDummy].Get(DeltaTick, 0, &pDeltaShot, 0);

						if(DeltashotSize < 0)
						{
							// couldn't find the delta snapshots that the server used
							// to compress this snapshot. force the server to resync
							if(g_Config.m_Debug)
							{
								char aBuf[256];
								str_format(aBuf, sizeof(aBuf), "error, couldn't find the delta snapshot");
								m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
							}

							// ack snapshot
							// TODO: combine this with the input message
							m_AckGameTick[g_Config.m_ClDummy] = -1;
							return;
						}
					}

					// decompress snapshot
					pDeltaData = m_SnapshotDelta.EmptyDelta();
					DeltaSize = sizeof(int)*3;

					if(CompleteSize)
					{
						int IntSize = CVariableInt::Decompress(m_aSnapshotIncomingData, CompleteSize, aTmpBuffer2, sizeof(aTmpBuffer2));

						if(IntSize < 0) // failure during decompression, bail
							return;

						pDeltaData = aTmpBuffer2;
						DeltaSize = IntSize;
					}

					// unpack delta
					SnapSize = m_SnapshotDelta.UnpackDelta(pDeltaShot, pTmpBuffer3, pDeltaData, DeltaSize);
					if(SnapSize < 0)
					{
						dbg_msg("client", "delta unpack failed!=%d", SnapSize);
						return;
					}

					if(Msg != NETMSG_SNAPEMPTY && pTmpBuffer3->Crc() != Crc)
					{
						if(g_Config.m_Debug)
						{
							char aBuf[256];
							str_format(aBuf, sizeof(aBuf), "snapshot crc error #%d - tick=%d wantedcrc=%d gotcrc=%d compressed_size=%d delta_tick=%d",
								m_SnapCrcErrors, GameTick, Crc, pTmpBuffer3->Crc(), CompleteSize, DeltaTick);
							m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
						}

						m_SnapCrcErrors++;
						if(m_SnapCrcErrors > 10)
						{
							// to many errors, send reset
							m_AckGameTick[g_Config.m_ClDummy] = -1;
							SendInput();
							m_SnapCrcErrors = 0;
						}
						return;
					}
					else
					{
						if(m_SnapCrcErrors)
							m_SnapCrcErrors--;
					}

					// purge old snapshots
					PurgeTick = DeltaTick;
					if(m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV] && m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_Tick < PurgeTick)
						PurgeTick = m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_Tick;
					if(m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] && m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick < PurgeTick)
						PurgeTick = m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick;
					m_SnapshotStorage[g_Config.m_ClDummy].PurgeUntil(PurgeTick);

					// add new
					m_SnapshotStorage[g_Config.m_ClDummy].Add(GameTick, time_get(), SnapSize, pTmpBuffer3, 1);

					// for antiping: if the projectile netobjects from the server contains extra data, this is removed and the original content restored before recording demo
					unsigned char aExtraInfoRemoved[CSnapshot::MAX_SIZE];
					mem_copy(aExtraInfoRemoved, pTmpBuffer3, SnapSize);
					SnapshotRemoveExtraInfo(aExtraInfoRemoved);

					// add snapshot to demo
					for(int i = 0; i < RECORDER_MAX; i++)
					{
						if(m_DemoRecorder[i].IsRecording())
						{
							// write snapshot
							m_DemoRecorder[i].RecordSnapshot(GameTick, aExtraInfoRemoved, SnapSize);
						}
					}

					// apply snapshot, cycle pointers
					m_ReceivedSnapshots[g_Config.m_ClDummy]++;

					m_CurrentRecvTick[g_Config.m_ClDummy] = GameTick;

					// we got two snapshots until we see us self as connected
					if(m_ReceivedSnapshots[g_Config.m_ClDummy] == 2)
					{
						// start at 200ms and work from there
						m_PredictedTime.Init(GameTick*time_freq()/50);
						m_PredictedTime.SetAdjustSpeed(1, 1000.0f);
						m_GameTime[g_Config.m_ClDummy].Init((GameTick-1)*time_freq()/50);
						m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV] = m_SnapshotStorage[g_Config.m_ClDummy].m_pFirst;
						m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] = m_SnapshotStorage[g_Config.m_ClDummy].m_pLast;
						m_LocalStartTime = time_get();
						SetState(IClient::STATE_ONLINE);
						DemoRecorder_HandleAutoStart();
					}

					// adjust game time
					if(m_ReceivedSnapshots[g_Config.m_ClDummy] > 2)
					{
						int64 Now = m_GameTime[g_Config.m_ClDummy].Get(time_get());
						int64 TickStart = GameTick*time_freq()/50;
						int64 TimeLeft = (TickStart-Now)*1000 / time_freq();
						m_GameTime[g_Config.m_ClDummy].Update(&m_GametimeMarginGraph, (GameTick-1)*time_freq()/50, TimeLeft, 0);
					}

					if(m_ReceivedSnapshots[g_Config.m_ClDummy] > 50 && !m_aTimeoutCodeSent[g_Config.m_ClDummy])
					{
						if(IsDDNet(&m_CurrentServerInfo))
						{
							m_aTimeoutCodeSent[g_Config.m_ClDummy] = true;
							CNetMsg_Cl_Say Msg;
							Msg.m_Team = 0;
							char aBuf[256];
							str_format(aBuf, sizeof(aBuf), "/timeout %s", m_aTimeoutCodes[g_Config.m_ClDummy]);
							Msg.m_pMessage = aBuf;
							CMsgPacker Packer(Msg.MsgID());
							Msg.Pack(&Packer);
							SendMsgExY(&Packer, MSGFLAG_VITAL, false, g_Config.m_ClDummy);
						}
					}

					// ack snapshot
					m_AckGameTick[g_Config.m_ClDummy] = GameTick;
				}
			}
		}
		else if(Msg == NETMSG_RCONTYPE)
		{
			bool UsernameReq = Unpacker.GetInt() & 1;
			GameClient()->OnRconType(UsernameReq);
		}
		else if(Msg == NETMSG_TIME_SCORE)
		{
			int NewTimeScore = Unpacker.GetInt();
			if (Unpacker.Error())
				return;
			GameClient()->OnTimeScore(NewTimeScore, g_Config.m_ClDummy);
		}
	}
	else
	{
		if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 || Msg == NETMSGTYPE_SV_EXTRAPROJECTILE)
		{
			// game message
			for(int i = 0; i < RECORDER_MAX; i++)
				if(m_DemoRecorder[i].IsRecording())
					m_DemoRecorder[i].RecordMessage(pPacket->m_pData, pPacket->m_DataSize);

			GameClient()->OnMessage(Msg, &Unpacker);
		}
	}
}

void CClient::ProcessServerPacketDummy(CNetChunk *pPacket)
{
	CUnpacker Unpacker;
	Unpacker.Reset(pPacket->m_pData, pPacket->m_DataSize);
	CMsgPacker Packer(NETMSG_EX);

	// unpack msgid and system flag
	int Msg;
	bool Sys;
	CUuid Uuid;

	int Result = UnpackMessageID(&Msg, &Sys, &Uuid, &Unpacker, &Packer);
	if(Result == UNPACKMESSAGE_ERROR)
	{
		return;
	}
	else if(Result == UNPACKMESSAGE_ANSWER)
	{
		SendMsgEx(&Packer, MSGFLAG_VITAL, true);
	}

	if(Sys)
	{
		if(Msg == NETMSG_CON_READY)
		{
			m_DummyConnected = true;
			g_Config.m_ClDummy = 1;
			Rcon("crashmeplx");
			if(m_RconAuthed[0])
				RconAuth("", m_RconPassword);
		}
		else if(Msg == NETMSG_SNAP || Msg == NETMSG_SNAPSINGLE || Msg == NETMSG_SNAPEMPTY)
		{
			int NumParts = 1;
			int Part = 0;
			int GameTick = Unpacker.GetInt();
			int DeltaTick = GameTick-Unpacker.GetInt();
			int PartSize = 0;
			int Crc = 0;
			int CompleteSize = 0;
			const char *pData = 0;

			// only allow packets from the server we actually want
			if(net_addr_comp(&pPacket->m_Address, &m_ServerAddress))
				return;

			// we are not allowed to process snapshot yet
			if(State() < IClient::STATE_LOADING)
				return;

			if(Msg == NETMSG_SNAP)
			{
				NumParts = Unpacker.GetInt();
				Part = Unpacker.GetInt();
			}

			if(Msg != NETMSG_SNAPEMPTY)
			{
				Crc = Unpacker.GetInt();
				PartSize = Unpacker.GetInt();
			}

			pData = (const char *)Unpacker.GetRaw(PartSize);

			if(Unpacker.Error() || NumParts < 1 || Part < 0 || PartSize < 0)
				return;

			if(GameTick >= m_CurrentRecvTick[!g_Config.m_ClDummy])
			{
				if(GameTick != m_CurrentRecvTick[!g_Config.m_ClDummy])
				{
					m_SnapshotParts = 0;
					m_CurrentRecvTick[!g_Config.m_ClDummy] = GameTick;
				}

				mem_copy((char*)m_aSnapshotIncomingData + Part*MAX_SNAPSHOT_PACKSIZE, pData, clamp(PartSize, 0, (int)sizeof(m_aSnapshotIncomingData) - Part*MAX_SNAPSHOT_PACKSIZE));
				m_SnapshotParts |= 1<<Part;

				if(m_SnapshotParts == (unsigned)((1<<NumParts)-1))
				{
					static CSnapshot Emptysnap;
					CSnapshot *pDeltaShot = &Emptysnap;
					int PurgeTick;
					void *pDeltaData;
					int DeltaSize;
					unsigned char aTmpBuffer2[CSnapshot::MAX_SIZE];
					unsigned char aTmpBuffer3[CSnapshot::MAX_SIZE];
					CSnapshot *pTmpBuffer3 = (CSnapshot*)aTmpBuffer3;	// Fix compiler warning for strict-aliasing
					int SnapSize;

					CompleteSize = (NumParts-1) * MAX_SNAPSHOT_PACKSIZE + PartSize;

					// reset snapshoting
					m_SnapshotParts = 0;

					// find snapshot that we should use as delta
					Emptysnap.Clear();

					// find delta
					if(DeltaTick >= 0)
					{
						int DeltashotSize = m_SnapshotStorage[!g_Config.m_ClDummy].Get(DeltaTick, 0, &pDeltaShot, 0);

						if(DeltashotSize < 0)
						{
							// couldn't find the delta snapshots that the server used
							// to compress this snapshot. force the server to resync
							if(g_Config.m_Debug)
							{
								char aBuf[256];
								str_format(aBuf, sizeof(aBuf), "error, couldn't find the delta snapshot");
								m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
							}

							// ack snapshot
							// TODO: combine this with the input message
							m_AckGameTick[!g_Config.m_ClDummy] = -1;
							return;
						}
					}

					// decompress snapshot
					pDeltaData = m_SnapshotDelta.EmptyDelta();
					DeltaSize = sizeof(int)*3;

					if(CompleteSize)
					{
						int IntSize = CVariableInt::Decompress(m_aSnapshotIncomingData, CompleteSize, aTmpBuffer2, sizeof(aTmpBuffer2));

						if(IntSize < 0) // failure during decompression, bail
							return;

						pDeltaData = aTmpBuffer2;
						DeltaSize = IntSize;
					}

					// unpack delta
					SnapSize = m_SnapshotDelta.UnpackDelta(pDeltaShot, pTmpBuffer3, pDeltaData, DeltaSize);
					if(SnapSize < 0)
					{
						m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", "delta unpack failed!");
						return;
					}

					if(Msg != NETMSG_SNAPEMPTY && pTmpBuffer3->Crc() != Crc)
					{
						if(g_Config.m_Debug)
						{
							char aBuf[256];
							str_format(aBuf, sizeof(aBuf), "snapshot crc error #%d - tick=%d wantedcrc=%d gotcrc=%d compressed_size=%d delta_tick=%d",
								m_SnapCrcErrors, GameTick, Crc, pTmpBuffer3->Crc(), CompleteSize, DeltaTick);
							m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
						}

						m_SnapCrcErrors++;
						if(m_SnapCrcErrors > 10)
						{
							// to many errors, send reset
							m_AckGameTick[!g_Config.m_ClDummy] = -1;
							SendInput();
							m_SnapCrcErrors = 0;
						}
						return;
					}
					else
					{
						if(m_SnapCrcErrors)
							m_SnapCrcErrors--;
					}

					// purge old snapshots
					PurgeTick = DeltaTick;
					if(m_aSnapshots[!g_Config.m_ClDummy][SNAP_PREV] && m_aSnapshots[!g_Config.m_ClDummy][SNAP_PREV]->m_Tick < PurgeTick)
						PurgeTick = m_aSnapshots[!g_Config.m_ClDummy][SNAP_PREV]->m_Tick;
					if(m_aSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT] && m_aSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick < PurgeTick)
						PurgeTick = m_aSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick;
					m_SnapshotStorage[!g_Config.m_ClDummy].PurgeUntil(PurgeTick);

					// add new
					m_SnapshotStorage[!g_Config.m_ClDummy].Add(GameTick, time_get(), SnapSize, pTmpBuffer3, 1);

					// apply snapshot, cycle pointers
					m_ReceivedSnapshots[!g_Config.m_ClDummy]++;

					m_CurrentRecvTick[!g_Config.m_ClDummy] = GameTick;

					// we got two snapshots until we see us self as connected
					if(m_ReceivedSnapshots[!g_Config.m_ClDummy] == 2)
					{
						// start at 200ms and work from there
						//m_PredictedTime[!g_Config.m_ClDummy].Init(GameTick*time_freq()/50);
						//m_PredictedTime[!g_Config.m_ClDummy].SetAdjustSpeed(1, 1000.0f);
						m_GameTime[!g_Config.m_ClDummy].Init((GameTick-1)*time_freq()/50);
						m_aSnapshots[!g_Config.m_ClDummy][SNAP_PREV] = m_SnapshotStorage[!g_Config.m_ClDummy].m_pFirst;
						m_aSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT] = m_SnapshotStorage[!g_Config.m_ClDummy].m_pLast;
						m_LocalStartTime = time_get();
						SetState(IClient::STATE_ONLINE);
					}

					// adjust game time
					if(m_ReceivedSnapshots[!g_Config.m_ClDummy] > 2)
					{
						int64 Now = m_GameTime[!g_Config.m_ClDummy].Get(time_get());
						int64 TickStart = GameTick*time_freq()/50;
						int64 TimeLeft = (TickStart-Now)*1000 / time_freq();
						m_GameTime[!g_Config.m_ClDummy].Update(&m_GametimeMarginGraph, (GameTick-1)*time_freq()/50, TimeLeft, 0);
					}

					// ack snapshot
					m_AckGameTick[!g_Config.m_ClDummy] = GameTick;
				}
			}
		}
		else if(Msg == NETMSG_TIME_SCORE)
		{
			int NewTimeScore = Unpacker.GetInt();
			if (Unpacker.Error())
				return;
			GameClient()->OnTimeScore(NewTimeScore, true);
		}
	}
	else
	{
		GameClient()->OnMessage(Msg, &Unpacker, 1);
	}
}

void CClient::ResetMapDownload()
{
	if(m_pMapdownloadTask)
	{
		m_pMapdownloadTask->Abort();
		m_pMapdownloadTask = NULL;
	}
	m_MapdownloadFile = 0;
	m_MapdownloadAmount = 0;
}

void CClient::FinishMapDownload()
{
	const char *pError;
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", "download complete, loading map");

	int Prev = m_MapdownloadTotalsize;
	m_MapdownloadTotalsize = -1;
	SHA256_DIGEST *pSha256 = m_MapdownloadSha256Present ? &m_MapdownloadSha256 : 0;

	char aTmp[256];
	char aMapFileTemp[256];
	char aMapFile[256];
	FormatMapDownloadFilename(m_aMapdownloadName, pSha256, m_MapdownloadCrc, true, aTmp, sizeof(aTmp));
	str_format(aMapFileTemp, sizeof(aMapFileTemp), "downloadedmaps/%s", aTmp);
	FormatMapDownloadFilename(m_aMapdownloadName, pSha256, m_MapdownloadCrc, false, aTmp, sizeof(aTmp));
	str_format(aMapFile, sizeof(aMapFileTemp), "downloadedmaps/%s", aTmp);

	Storage()->RenameFile(aMapFileTemp, aMapFile, IStorage::TYPE_SAVE);

	// load map
	pError = LoadMap(m_aMapdownloadName, aMapFile, pSha256, m_MapdownloadCrc);
	if(!pError)
	{
		ResetMapDownload();
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", "loading done");
		SendReady();
	}
	else if(m_pMapdownloadTask) // fallback
	{
		ResetMapDownload();
		m_MapdownloadTotalsize = Prev;
		SendMapRequest();
	}
	else
	{
		if(m_MapdownloadFile)
		{
			io_close(m_MapdownloadFile);
			m_MapdownloadFile = 0;
		}
		ResetMapDownload();
		DisconnectWithReason(pError);
	}
}

void CClient::ResetDDNetInfo()
{
	if(m_pDDNetInfoTask)
	{
		m_pDDNetInfoTask->Abort();
		m_pDDNetInfoTask = NULL;
	}
}

void CClient::FinishDDNetInfo()
{
	ResetDDNetInfo();
	m_pStorage->RenameFile(DDNET_INFO_TMP, DDNET_INFO, IStorage::TYPE_SAVE);
	LoadDDNetInfo();
}

void CClient::LoadDDNetInfo()
{
	const json_value *pDDNetInfo = m_ServerBrowser.LoadDDNetInfo();

	if(!pDDNetInfo)
		return;

	const json_value *pVersion = json_object_get(pDDNetInfo, "version");
	if(pVersion->type == json_string)
	{
		const char *pVersionString = json_string_get(pVersion);
		if(str_comp(pVersionString, GAME_RELEASE_VERSION))
		{
			str_copy(m_aVersionStr, pVersionString, sizeof(m_aVersionStr));
		}
		else
		{
			m_aVersionStr[0] = '0';
			m_aVersionStr[1] = '\0';
		}
	}

	const json_value *pNews = json_object_get(pDDNetInfo, "news");
	if(pNews->type == json_string)
	{
		const char *pNewsString = json_string_get(pNews);

		if(m_aNews[0] && str_comp(m_aNews, pNewsString))
			g_Config.m_UiPage = CMenus::PAGE_NEWS;

		str_copy(m_aNews, pNewsString, sizeof(m_aNews));
	}
}

void CClient::PumpNetwork()
{
	for(int i=0; i<3; i++)
	{
		m_NetClient[i].Update();
	}

	if(State() != IClient::STATE_DEMOPLAYBACK)
	{
		// check for errors
		if(State() != IClient::STATE_OFFLINE && State() != IClient::STATE_QUITING && m_NetClient[0].State() == NETSTATE_OFFLINE)
		{
			SetState(IClient::STATE_OFFLINE);
			Disconnect();
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "offline error='%s'", m_NetClient[0].ErrorString());
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);
		}

		//
		if(State() == IClient::STATE_CONNECTING && m_NetClient[0].State() == NETSTATE_ONLINE)
		{
			// we switched to online
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", "connected, sending info");
			SetState(IClient::STATE_LOADING);
			SendInfo();
		}
	}

	// process packets
	CNetChunk Packet;
	for(int i=0; i < 3; i++)
	{
		while(m_NetClient[i].Recv(&Packet))
		{
			if(Packet.m_ClientID == -1 || i > 1)
			{
				ProcessConnlessPacket(&Packet);
			}
			else if(i > 0 && i < 2)
			{
				if(g_Config.m_ClDummy)
					ProcessServerPacket(&Packet); //self
				else
					ProcessServerPacketDummy(&Packet); //multiclient
			}
			else
			{
				if(g_Config.m_ClDummy)
					ProcessServerPacketDummy(&Packet); //multiclient
				else
					ProcessServerPacket(&Packet); //self
			}
		}
	}
}

void CClient::OnDemoPlayerSnapshot(void *pData, int Size)
{
	// update ticks, they could have changed
	const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();
	CSnapshotStorage::CHolder *pTemp;
	m_CurGameTick[g_Config.m_ClDummy] = pInfo->m_Info.m_CurrentTick;
	m_PrevGameTick[g_Config.m_ClDummy] = pInfo->m_PreviousTick;

	// handle snapshots
	pTemp = m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV];
	m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV] = m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT];
	m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] = pTemp;

	mem_copy(m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_pSnap, pData, Size);
	mem_copy(m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_pAltSnap, pData, Size);

	GameClient()->OnNewSnapshot();
}

void CClient::OnDemoPlayerMessage(void *pData, int Size)
{
	CUnpacker Unpacker;
	Unpacker.Reset(pData, Size);

	// unpack msgid and system flag
	int Msg = Unpacker.GetInt();
	int Sys = Msg&1;
	Msg >>= 1;

	if(Unpacker.Error())
		return;

	if(!Sys)
		GameClient()->OnMessage(Msg, &Unpacker);
}
/*
const IDemoPlayer::CInfo *client_demoplayer_getinfo()
{
	static DEMOPLAYBACK_INFO ret;
	const DEMOREC_PLAYBACKINFO *info = m_DemoPlayer.Info();
	ret.first_tick = info->first_tick;
	ret.last_tick = info->last_tick;
	ret.current_tick = info->current_tick;
	ret.paused = info->paused;
	ret.speed = info->speed;
	return &ret;
}*/

/*
void DemoPlayer()->SetPos(float percent)
{
	demorec_playback_set(percent);
}

void DemoPlayer()->SetSpeed(float speed)
{
	demorec_playback_setspeed(speed);
}

void DemoPlayer()->SetPause(int paused)
{
	if(paused)
		demorec_playback_pause();
	else
		demorec_playback_unpause();
}*/

void CClient::Update()
{
	if(State() == IClient::STATE_DEMOPLAYBACK)
	{
		m_DemoPlayer.Update();
		if(m_DemoPlayer.IsPlaying())
		{
			// update timers
			const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();
			m_CurGameTick[g_Config.m_ClDummy] = pInfo->m_Info.m_CurrentTick;
			m_PrevGameTick[g_Config.m_ClDummy] = pInfo->m_PreviousTick;
			m_GameIntraTick[g_Config.m_ClDummy] = pInfo->m_IntraTick;
			m_GameTickTime[g_Config.m_ClDummy] = pInfo->m_TickTime;
		}
		else
		{
			// disconnect on error
			Disconnect();
		}
	}
	else if(State() == IClient::STATE_ONLINE && m_ReceivedSnapshots[g_Config.m_ClDummy] >= 3)
	{
		if(m_ReceivedSnapshots[!g_Config.m_ClDummy] >= 3)
		{
			// switch dummy snapshot
			int64 Now = m_GameTime[!g_Config.m_ClDummy].Get(time_get());
			while(1)
			{
				CSnapshotStorage::CHolder *pCur = m_aSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT];
				int64 TickStart = (pCur->m_Tick)*time_freq()/50;

				if(TickStart < Now)
				{
					CSnapshotStorage::CHolder *pNext = m_aSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT]->m_pNext;
					if(pNext)
					{
						m_aSnapshots[!g_Config.m_ClDummy][SNAP_PREV] = m_aSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT];
						m_aSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT] = pNext;

						// set ticks
						m_CurGameTick[!g_Config.m_ClDummy] = m_aSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick;
						m_PrevGameTick[!g_Config.m_ClDummy] = m_aSnapshots[!g_Config.m_ClDummy][SNAP_PREV]->m_Tick;
					}
					else
						break;
				}
				else
					break;
			}
		}

		// switch snapshot
		int Repredict = 0;
		int64 Freq = time_freq();
		int64 Now = m_GameTime[g_Config.m_ClDummy].Get(time_get());
		int64 PredNow = m_PredictedTime.Get(time_get());

		while(1)
		{
			CSnapshotStorage::CHolder *pCur = m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT];
			int64 TickStart = (pCur->m_Tick)*time_freq()/50;

			if(TickStart < Now)
			{
				CSnapshotStorage::CHolder *pNext = m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_pNext;
				if(pNext)
				{
					m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV] = m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT];
					m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] = pNext;

					// set ticks
					m_CurGameTick[g_Config.m_ClDummy] = m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick;
					m_PrevGameTick[g_Config.m_ClDummy] = m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_Tick;

					if(m_LastDummy2 == (bool)g_Config.m_ClDummy && m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] && m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV])
					{
						GameClient()->OnNewSnapshot();
						Repredict = 1;
					}
				}
				else
					break;
			}
			else
				break;
		}

		if(m_LastDummy2 != (bool)g_Config.m_ClDummy)
		{
			m_LastDummy2 = g_Config.m_ClDummy;
		}

		if(m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] && m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV])
		{
			int64 CurtickStart = (m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick)*time_freq()/50;
			int64 PrevtickStart = (m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_Tick)*time_freq()/50;
			int PrevPredTick = (int)(PredNow*50/time_freq());
			int NewPredTick = PrevPredTick+1;

			m_GameIntraTick[g_Config.m_ClDummy] = (Now - PrevtickStart) / (float)(CurtickStart-PrevtickStart);
			m_GameTickTime[g_Config.m_ClDummy] = (Now - PrevtickStart) / (float)Freq; //(float)SERVER_TICK_SPEED);

			CurtickStart = NewPredTick*time_freq()/50;
			PrevtickStart = PrevPredTick*time_freq()/50;
			m_PredIntraTick[g_Config.m_ClDummy] = (PredNow - PrevtickStart) / (float)(CurtickStart-PrevtickStart);

			if(NewPredTick < m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_Tick-SERVER_TICK_SPEED || NewPredTick > m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_Tick+SERVER_TICK_SPEED)
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", "prediction time reset!");
				m_PredictedTime.Init(m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick*time_freq()/50);
			}

			if(NewPredTick > m_PredTick[g_Config.m_ClDummy])
			{
				m_PredTick[g_Config.m_ClDummy] = NewPredTick;
				Repredict = 1;

				// send input
				SendInput();
			}
		}

		// only do sane predictions
		if(Repredict)
		{
			if(m_PredTick[g_Config.m_ClDummy] > m_CurGameTick[g_Config.m_ClDummy] && m_PredTick[g_Config.m_ClDummy] < m_CurGameTick[g_Config.m_ClDummy]+50)
				GameClient()->OnPredict();
		}

		// fetch server info if we don't have it
		if(State() >= IClient::STATE_LOADING &&
			m_CurrentServerInfoRequestTime >= 0 &&
			time_get() > m_CurrentServerInfoRequestTime)
		{
			m_ServerBrowser.RequestCurrentServer(m_ServerAddress);
			m_CurrentServerInfoRequestTime = time_get()+time_freq()*2;
		}
	}

	// STRESS TEST: join the server again
#ifdef CONF_DEBUG
	if(g_Config.m_DbgStress)
	{
		static int64 ActionTaken = 0;
		int64 Now = time_get();
		if(State() == IClient::STATE_OFFLINE)
		{
			if(Now > ActionTaken+time_freq()*2)
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "stress", "reconnecting!");
				Connect(g_Config.m_DbgStressServer);
				ActionTaken = Now;
			}
		}
		else
		{
			if(Now > ActionTaken+time_freq()*(10+g_Config.m_DbgStress))
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "stress", "disconnecting!");
				Disconnect();
				ActionTaken = Now;
			}
		}
	}
#endif

	// pump the network
	PumpNetwork();

	if(m_pMapdownloadTask)
	{
		if(m_pMapdownloadTask->State() == HTTP_DONE)
			FinishMapDownload();
		else if(m_pMapdownloadTask->State() == HTTP_ERROR)
		{
			dbg_msg("webdl", "http failed, falling back to gameserver");
			ResetMapDownload();
			SendMapRequest();
		}
		else if(m_pMapdownloadTask->State() == HTTP_ABORTED)
		{
			m_pMapdownloadTask = NULL;
		}
	}

	if(m_pDDNetInfoTask)
	{
		if(m_pDDNetInfoTask->State() == HTTP_DONE)
			FinishDDNetInfo();
		else if(m_pDDNetInfoTask->State() == HTTP_ERROR)
		{
			dbg_msg("ddnet-info", "download failed");
			ResetDDNetInfo();
		}
		else if(m_pDDNetInfoTask->State() == HTTP_ABORTED)
		{
			m_pDDNetInfoTask = NULL;
		}
	}

	// update the maser server registry
	MasterServer()->Update();

	// update the server browser
	m_ServerBrowser.Update(m_ResortServerBrowser);
	m_ResortServerBrowser = false;

	// update gameclient
	if(!m_EditorActive)
		GameClient()->OnUpdate();

	if(m_ReconnectTime > 0 && time_get() > m_ReconnectTime)
	{
		Connect(m_aServerAddressStr);
		m_ReconnectTime = 0;
	}
}

void CClient::RegisterInterfaces()
{
	Kernel()->RegisterInterface(static_cast<IDemoRecorder*>(&m_DemoRecorder[RECORDER_MANUAL]), false);
	Kernel()->RegisterInterface(static_cast<IDemoPlayer*>(&m_DemoPlayer), false);
	Kernel()->RegisterInterface(static_cast<IGhostRecorder*>(&m_GhostRecorder), false);
	Kernel()->RegisterInterface(static_cast<IGhostLoader*>(&m_GhostLoader), false);
	Kernel()->RegisterInterface(static_cast<IServerBrowser*>(&m_ServerBrowser), false);
#if defined(CONF_AUTOUPDATE)
	Kernel()->RegisterInterface(static_cast<IUpdater*>(&m_Updater), false);
#endif
	Kernel()->RegisterInterface(static_cast<IFriends*>(&m_Friends), false);
	Kernel()->ReregisterInterface(static_cast<IFriends*>(&m_Foes));
}

void CClient::InitInterfaces()
{
	// fetch interfaces
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pGameClient = Kernel()->RequestInterface<IGameClient>();
	m_pInput = Kernel()->RequestInterface<IEngineInput>();
	m_pMap = Kernel()->RequestInterface<IEngineMap>();
	m_pMasterServer = Kernel()->RequestInterface<IEngineMasterServer>();
#if defined(CONF_AUTOUPDATE)
	m_pUpdater = Kernel()->RequestInterface<IUpdater>();
#endif
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	m_DemoEditor.Init(m_pGameClient->NetVersion(), &m_SnapshotDelta, m_pConsole, m_pStorage);

	m_ServerBrowser.SetBaseInfo(&m_NetClient[2], m_pGameClient->NetVersion());

	HttpInit(m_pStorage);

#if defined(CONF_AUTOUPDATE)
	m_Updater.Init();
#endif

	m_Friends.Init();
	m_Foes.Init(true);

	m_GhostRecorder.Init();
	m_GhostLoader.Init();
}

void CClient::Run()
{
	m_LocalStartTime = time_get();
	m_SnapshotParts = 0;

	if(m_GenerateTimeoutSeed)
	{
		GenerateTimeoutSeed();
	}

	unsigned int Seed;
	secure_random_fill(&Seed, sizeof(Seed));
	srand(Seed);

	if(g_Config.m_Debug)
	{
		g_UuidManager.DebugDump();
	}

	// open socket
	{
		NETADDR BindAddr;
		if(g_Config.m_Bindaddr[0] && net_host_lookup(g_Config.m_Bindaddr, &BindAddr, NETTYPE_ALL) == 0)
		{
			// got bindaddr
			BindAddr.type = NETTYPE_ALL;
		}
		else
		{
			mem_zero(&BindAddr, sizeof(BindAddr));
			BindAddr.type = NETTYPE_ALL;
		}
		for(int i = 0; i < 3; i++)
		{
			do
			{
				BindAddr.port = (secure_rand() % 64511) + 1024;
			}
			while(!m_NetClient[i].Open(BindAddr, 0));
		}
	}


	// init the input
	Input()->Init();

	// start refreshing addresses while we load
	MasterServer()->RefreshAddresses(m_NetClient[0].NetType());

	// load data
	if(!LoadData())
		return;

	GameClient()->OnInit();

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "version %s", GameClient()->NetVersion());
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);

	// connect to the server if wanted
	/*
	if(config.cl_connect[0] != 0)
		Connect(config.cl_connect);
	config.cl_connect[0] = 0;
	*/

	//
	m_FpsGraph.Init(0.0f, 200.0f);

	// never start with the editor
	g_Config.m_ClEditor = 0;

	// process pending commands
	m_pConsole->StoreCommands(false);

#if defined(CONF_FAMILY_UNIX)
	m_Fifo.Init(m_pConsole, g_Config.m_ClInputFifo, CFGFLAG_CLIENT);
#endif

	// loads the existing ddnet info file if it exists
	LoadDDNetInfo();
	// but still request the new one from server
	if(g_Config.m_ClShowWelcome)
		g_Config.m_ClShowWelcome = 0;
	else
		RequestDDNetInfo();

	bool LastD = false;
	bool LastQ = false;
	bool LastE = false;
	bool LastG = false;

	int64 LastTime = time_get_microseconds();
	int64 LastRenderTime = time_get();

	while(1)
	{
		set_new_tick();

		// handle pending connects
		if(m_aCmdConnect[0])
		{
			str_copy(g_Config.m_UiServerAddress, m_aCmdConnect, sizeof(g_Config.m_UiServerAddress));
			Connect(m_aCmdConnect);
			m_aCmdConnect[0] = 0;
		}

		// progress on dummy connect if security token handshake skipped/passed
		if(m_DummySendConnInfo && !m_NetClient[1].SecurityTokenUnknown())
		{
			m_DummySendConnInfo = false;

			// send client info
			CMsgPacker MsgInfo(NETMSG_INFO);
			MsgInfo.AddString(GameClient()->NetVersion(), 128);
			MsgInfo.AddString(m_Password, 128);
			SendMsgExY(&MsgInfo, MSGFLAG_VITAL|MSGFLAG_FLUSH, true, 1);

			// update netclient
			m_NetClient[1].Update();

			// send ready
			CMsgPacker MsgReady(NETMSG_READY);
			SendMsgExY(&MsgReady, MSGFLAG_VITAL|MSGFLAG_FLUSH, true, 1);

			// startinfo
			GameClient()->SendDummyInfo(true);

			// send enter game an finish the connection
			CMsgPacker MsgEnter(NETMSG_ENTERGAME);
			SendMsgExY(&MsgEnter, MSGFLAG_VITAL|MSGFLAG_FLUSH, true, 1);
		}

		// update input
		if(Input()->Update())
			break;
#if defined(CONF_AUTOUPDATE)
		Updater()->Update();
#endif

		// panic quit button
		if(CtrlShiftKey(KEY_Q, LastQ))
		{
			Quit();
			break;
		}

		if(CtrlShiftKey(KEY_D, LastD))
			g_Config.m_Debug ^= 1;

		if(CtrlShiftKey(KEY_G, LastG))
			g_Config.m_DbgGraphs ^= 1;

		if(CtrlShiftKey(KEY_E, LastE))
		{
			g_Config.m_ClEditor = g_Config.m_ClEditor^1;
			Input()->MouseModeRelative();
			Input()->SetIMEState(true);
		}

		// render
		{
			if(g_Config.m_ClEditor)
			{
				if(!m_EditorActive)
				{
					Input()->MouseModeRelative();
					GameClient()->OnActivateEditor();
					m_EditorActive = true;
				}
			}
			else if(m_EditorActive)
				m_EditorActive = false;

			Update();
			int64 Now = time_get();

			if("graprhics stuff")
			{
				m_RenderFrames++;

				// update frametime
				m_RenderFrameTime = (Now - m_LastRenderTime) / (float)time_freq();
				if(m_RenderFrameTime < m_RenderFrameTimeLow)
					m_RenderFrameTimeLow = m_RenderFrameTime;
				if(m_RenderFrameTime > m_RenderFrameTimeHigh)
					m_RenderFrameTimeHigh = m_RenderFrameTime;
				m_FpsGraph.Add(1.0f/m_RenderFrameTime, 1,1,1);

				// keep the overflow time - it's used to make sure the gfx refreshrate is reached
				int64 AdditionalTime = g_Config.m_GfxRefreshRate ? ((Now - LastRenderTime) - (time_freq() / (int64)g_Config.m_GfxRefreshRate)) : 0;
				// if the value is over a second time loose, reset the additional time (drop the frames, that are lost already)
				if(AdditionalTime > time_freq())
					AdditionalTime = time_freq();
				LastRenderTime = Now - AdditionalTime;
				m_LastRenderTime = Now;

#ifdef CONF_DEBUG
				if(g_Config.m_DbgStress)
				{
					if((m_RenderFrames%10) == 0)
					{
						if(!m_EditorActive)
							Render();
						else
						{
							DebugRender();
						}
					}
				}
				else
#endif
				{
					if(!m_EditorActive)
						Render();
					else
					{
						DebugRender();
					}
				}

				Input()->NextFrame();
			}

			if(Input()->VideoRestartNeeded())
			{
				LoadData();
				GameClient()->OnInit();
			}
		}

		AutoScreenshot_Cleanup();
		AutoStatScreenshot_Cleanup();
		AutoCSV_Cleanup();

		// check conditions
		if(State() == IClient::STATE_QUITING)
			break;

#if defined(CONF_FAMILY_UNIX)
		m_Fifo.Update();
#endif

		// beNice
		int64 Now = time_get_microseconds();
		int64 SleepTimeInMicroSeconds = 0;
		bool Slept = false;
		if("grapirx inactive")
		{
			SleepTimeInMicroSeconds = ((int64)1000000 / (int64)g_Config.m_ClRefreshRateInactive) - (Now - LastTime);
			if(SleepTimeInMicroSeconds / (int64)1000 > (int64)0)
				thread_sleep(SleepTimeInMicroSeconds);
			Slept = true;
		}
		else if(g_Config.m_ClRefreshRate)
		{
			SleepTimeInMicroSeconds = ((int64)1000000 / (int64)g_Config.m_ClRefreshRate) - (Now - LastTime);
			if(SleepTimeInMicroSeconds > (int64)0)
				net_socket_read_wait(m_NetClient[0].m_Socket, SleepTimeInMicroSeconds);
			Slept = true;
		}
		if(Slept)
		{
			// if the diff gets too small it shouldn't get even smaller (drop the updates, that could not be handled)
			if(SleepTimeInMicroSeconds < (int64)-1000000)
				SleepTimeInMicroSeconds = (int64)-1000000;
			// don't go higher than the game ticks speed, because the network is waking up the client with the server's snapshots anyway
			else if(SleepTimeInMicroSeconds > (int64)1000000 / m_GameTickSpeed)
				SleepTimeInMicroSeconds = (int64)1000000 / m_GameTickSpeed;
			// the time diff between the time that was used actually used and the time the thread should sleep/wait
			// will be calculated in the sleep time of the next update tick by faking the time it should have slept/wait.
			// so two cases (and the case it slept exactly the time it should):
			//	- the thread slept/waited too long, then it adjust the time to sleep/wait less in the next update tick
			//	- the thread slept/waited too less, then it adjust the time to sleep/wait more in the next update tick
			LastTime = Now + SleepTimeInMicroSeconds;
		}
		else
			LastTime = Now;

		if(g_Config.m_DbgHitch)
		{
			thread_sleep(g_Config.m_DbgHitch*1000);
			g_Config.m_DbgHitch = 0;
		}

		// update local time
		m_LocalTime = (time_get()-m_LocalStartTime)/(float)time_freq();
	}

#if defined(CONF_FAMILY_UNIX)
	m_Fifo.Shutdown();
#endif

	GameClient()->OnShutdown();
	Disconnect();
}

bool CClient::CtrlShiftKey(int Key, bool &Last)
{
	if(Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyIsPressed(KEY_LSHIFT) && !Last && Input()->KeyIsPressed(Key))
	{
		Last = true;
		return true;
	}
	else if(Last && !Input()->KeyIsPressed(Key))
		Last = false;

	return false;
}

void CClient::Con_Connect(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	str_copy(pSelf->m_aCmdConnect, pResult->GetString(0), sizeof(pSelf->m_aCmdConnect));
}

void CClient::Con_Disconnect(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->Disconnect();
}

void CClient::Con_DummyConnect(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->DummyConnect();
}

void CClient::Con_DummyDisconnect(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->DummyDisconnect(0);
}

void CClient::Con_Quit(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->Quit();
}

void CClient::Con_Minimize(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
}

void CClient::Con_Ping(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;

	CMsgPacker Msg(NETMSG_PING);
	pSelf->SendMsgEx(&Msg, 0);
	pSelf->m_PingStartTime = time_get();
}

void CClient::AutoScreenshot_Start()
{
	if(g_Config.m_ClAutoScreenshot)
	{
		m_AutoScreenshotRecycle = true;
	}
}

void CClient::AutoStatScreenshot_Start()
{
	if(g_Config.m_ClAutoStatboardScreenshot)
	{
		m_AutoStatScreenshotRecycle = true;
	}
}

void CClient::AutoScreenshot_Cleanup()
{
	if(m_AutoScreenshotRecycle)
	{
		if(g_Config.m_ClAutoScreenshotMax)
		{
			// clean up auto taken screens
			CFileCollection AutoScreens;
			AutoScreens.Init(Storage(), "screenshots/auto", "autoscreen", ".png", g_Config.m_ClAutoScreenshotMax);
		}
		m_AutoScreenshotRecycle = false;
	}
}

void CClient::AutoStatScreenshot_Cleanup()
{
	if(m_AutoStatScreenshotRecycle)
	{
		if(g_Config.m_ClAutoStatboardScreenshotMax)
		{
			// clean up auto taken screens
			CFileCollection AutoScreens;
			AutoScreens.Init(Storage(), "screenshots/auto/stats", "autoscreen", ".png", g_Config.m_ClAutoStatboardScreenshotMax);
		}
		m_AutoStatScreenshotRecycle = false;
	}
}

void CClient::AutoCSV_Start()
{
	if(g_Config.m_ClAutoCSV)
		m_AutoCSVRecycle = true;
}

void CClient::AutoCSV_Cleanup()
{
	if(m_AutoCSVRecycle)
	{
		if(g_Config.m_ClAutoCSVMax)
		{
			// clean up auto csvs
			CFileCollection AutoRecord;
			AutoRecord.Init(Storage(), "record/csv", "autorecord", ".csv", g_Config.m_ClAutoCSVMax);
		}
		m_AutoCSVRecycle = false;
	}
}

void CClient::Con_Screenshot(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
}

void CClient::Con_Rcon(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->Rcon(pResult->GetString(0));
}

void CClient::Con_RconAuth(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->RconAuth("", pResult->GetString(0));
}

void CClient::Con_RconLogin(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->RconAuth(pResult->GetString(0), pResult->GetString(1));
}

void CClient::Con_AddFavorite(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	NETADDR Addr;
	if(net_addr_from_str(&Addr, pResult->GetString(0)) == 0)
		pSelf->m_ServerBrowser.AddFavorite(Addr);
}

void CClient::Con_RemoveFavorite(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	NETADDR Addr;
	if(net_addr_from_str(&Addr, pResult->GetString(0)) == 0)
		pSelf->m_ServerBrowser.RemoveFavorite(Addr);
}

void CClient::DemoSliceBegin()
{
	const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();
	g_Config.m_ClDemoSliceBegin = pInfo->m_Info.m_CurrentTick;
}

void CClient::DemoSliceEnd()
{
	const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();
	g_Config.m_ClDemoSliceEnd = pInfo->m_Info.m_CurrentTick;
}

void CClient::Con_DemoSliceBegin(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->DemoSliceBegin();
}

void CClient::Con_DemoSliceEnd(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->DemoSliceEnd();
}

void CClient::DemoSlice(const char *pDstPath, CLIENTFUNC_FILTER pfnFilter, void *pUser)
{
	if(m_DemoPlayer.IsPlaying())
	{
		const char *pDemoFileName = m_DemoPlayer.GetDemoFileName();
		m_DemoEditor.Slice(pDemoFileName, pDstPath, g_Config.m_ClDemoSliceBegin, g_Config.m_ClDemoSliceEnd, pfnFilter, pUser);
	}
}

const char *CClient::DemoPlayer_Play(const char *pFilename, int StorageType)
{
	int Crc;
	const char *pError;
	Disconnect();
	m_NetClient[0].ResetErrorString();

	// try to start playback
	m_DemoPlayer.SetListener(this);

	if(m_DemoPlayer.Load(Storage(), m_pConsole, pFilename, StorageType))
		return "error loading demo";

	// load map
	Crc = (m_DemoPlayer.Info()->m_Header.m_aMapCrc[0]<<24)|
		(m_DemoPlayer.Info()->m_Header.m_aMapCrc[1]<<16)|
		(m_DemoPlayer.Info()->m_Header.m_aMapCrc[2]<<8)|
		(m_DemoPlayer.Info()->m_Header.m_aMapCrc[3]);
	pError = LoadMapSearch(m_DemoPlayer.Info()->m_Header.m_aMapName, 0, Crc);
	if(pError)
	{
		DisconnectWithReason(pError);
		return pError;
	}

	GameClient()->OnConnected();

	// setup buffers
	mem_zero(m_aDemorecSnapshotData, sizeof(m_aDemorecSnapshotData));

	m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] = &m_aDemorecSnapshotHolders[SNAP_CURRENT];
	m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV] = &m_aDemorecSnapshotHolders[SNAP_PREV];

	m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_pSnap = (CSnapshot *)m_aDemorecSnapshotData[SNAP_CURRENT][0];
	m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_pAltSnap = (CSnapshot *)m_aDemorecSnapshotData[SNAP_CURRENT][1];
	m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_SnapSize = 0;
	m_aSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick = -1;

	m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_pSnap = (CSnapshot *)m_aDemorecSnapshotData[SNAP_PREV][0];
	m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_pAltSnap = (CSnapshot *)m_aDemorecSnapshotData[SNAP_PREV][1];
	m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_SnapSize = 0;
	m_aSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_Tick = -1;

	// enter demo playback state
	SetState(IClient::STATE_DEMOPLAYBACK);

	m_DemoPlayer.Play();
	GameClient()->OnEnterGame();

	return 0;
}

void CClient::Con_Play(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->DemoPlayer_Play(pResult->GetString(0), IStorage::TYPE_ALL);
}

void CClient::Con_DemoPlay(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pSelf->m_DemoPlayer.IsPlaying())
	{
		if(pSelf->m_DemoPlayer.BaseInfo()->m_Paused)
		{
			pSelf->m_DemoPlayer.Unpause();
		}
		else
		{
			pSelf->m_DemoPlayer.Pause();
		}
	}
}

void CClient::Con_DemoSpeed(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->m_DemoPlayer.SetSpeed(pResult->GetFloat(0));
}

void CClient::DemoRecorder_Start(const char *pFilename, bool WithTimestamp, int Recorder)
{
	if(State() != IClient::STATE_ONLINE)
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demorec/record", "client is not online");
	else
	{
		char aFilename[128];
		if(WithTimestamp)
		{
			char aDate[20];
			str_timestamp(aDate, sizeof(aDate));
			str_format(aFilename, sizeof(aFilename), "demos/%s_%s.demo", pFilename, aDate);
		}
		else
			str_format(aFilename, sizeof(aFilename), "demos/%s.demo", pFilename);
		m_DemoRecorder[Recorder].Start(Storage(), m_pConsole, aFilename, GameClient()->NetVersion(), m_aCurrentMap, m_pMap->Sha256(), m_pMap->Crc(), "client", m_pMap->MapSize(), 0, m_pMap->File());
	}
}

void CClient::DemoRecorder_HandleAutoStart()
{
	dbg_msg("auto_demo", "chillerbot-ng does not support demo recording");
	/*
	if(g_Config.m_ClAutoDemoRecord)
	{
		DemoRecorder_Stop(RECORDER_AUTO);
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "auto/%s", m_aCurrentMap);
		DemoRecorder_Start(aBuf, true, RECORDER_AUTO);
		if(g_Config.m_ClAutoDemoMax)
		{
			// clean up auto recorded demos
			CFileCollection AutoDemos;
			AutoDemos.Init(Storage(), "demos/auto", "" / * empty for wild card * /, ".demo", g_Config.m_ClAutoDemoMax);
		}
	}
	*/
}

void CClient::DemoRecorder_Stop(int Recorder)
{
	m_DemoRecorder[Recorder].Stop();
}

void CClient::DemoRecorder_AddDemoMarker(int Recorder)
{
	m_DemoRecorder[Recorder].AddDemoMarker();
}

class IDemoRecorder *CClient::DemoRecorder(int Recorder)
{
	return &m_DemoRecorder[Recorder];
}

void CClient::Con_Record(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pResult->NumArguments())
		pSelf->DemoRecorder_Start(pResult->GetString(0), false, RECORDER_MANUAL);
	else
		pSelf->DemoRecorder_Start(pSelf->m_aCurrentMap, true, RECORDER_MANUAL);
}

void CClient::Con_StopRecord(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->DemoRecorder_Stop(RECORDER_MANUAL);
}

void CClient::Con_AddDemoMarker(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->DemoRecorder_AddDemoMarker(RECORDER_MANUAL);
	pSelf->DemoRecorder_AddDemoMarker(RECORDER_RACE);
	pSelf->DemoRecorder_AddDemoMarker(RECORDER_AUTO);
}

void CClient::ServerBrowserUpdate()
{
	m_ResortServerBrowser = true;
}

void CClient::ConchainServerBrowserUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CClient *)pUserData)->ServerBrowserUpdate();
}

void CClient::SwitchWindowScreen(int Index)
{

}

void CClient::ConchainWindowScreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
}

void CClient::ToggleFullscreen()
{

}

void CClient::ConchainFullscreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
}

void CClient::ToggleWindowBordered()
{
	g_Config.m_GfxBorderless ^= 1;
}

void CClient::ConchainWindowBordered(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
}

void CClient::ToggleWindowVSync()
{

}

void CClient::ConchainWindowVSync(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
}

void CClient::ConchainTimeoutSeed(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		pSelf->m_GenerateTimeoutSeed = false;
}

void CClient::ConchainPassword(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() && pSelf->m_LocalStartTime) //won't set m_SendPassword before game has started
		pSelf->m_SendPassword = true;
}

void CClient::RegisterCommands()
{
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	// register server dummy commands for tab completion
	m_pConsole->Register("kick", "i[id] ?r[reason]", CFGFLAG_SERVER, 0, 0, "Kick player with specified id for any reason");
	m_pConsole->Register("ban", "s[ip|id] ?i[minutes] r[reason]", CFGFLAG_SERVER, 0, 0, "Ban player with ip/id for x minutes for any reason");
	m_pConsole->Register("unban", "s[ip]", CFGFLAG_SERVER, 0, 0, "Unban ip");
	m_pConsole->Register("bans", "", CFGFLAG_SERVER, 0, 0, "Show banlist");
	m_pConsole->Register("status", "", CFGFLAG_SERVER, 0, 0, "List players");
	m_pConsole->Register("shutdown", "", CFGFLAG_SERVER, 0, 0, "Shut down");
	m_pConsole->Register("record", "s[file]", CFGFLAG_SERVER, 0, 0, "Record to a file");
	m_pConsole->Register("stoprecord", "", CFGFLAG_SERVER, 0, 0, "Stop recording");
	m_pConsole->Register("reload", "", CFGFLAG_SERVER, 0, 0, "Reload the map");

	m_pConsole->Register("dummy_connect", "", CFGFLAG_CLIENT, Con_DummyConnect, this, "connect dummy");
	m_pConsole->Register("dummy_disconnect", "", CFGFLAG_CLIENT, Con_DummyDisconnect, this, "disconnect dummy");

	m_pConsole->Register("quit", "", CFGFLAG_CLIENT|CFGFLAG_STORE, Con_Quit, this, "Quit Teeworlds");
	m_pConsole->Register("exit", "", CFGFLAG_CLIENT|CFGFLAG_STORE, Con_Quit, this, "Quit Teeworlds");
	m_pConsole->Register("minimize", "", CFGFLAG_CLIENT|CFGFLAG_STORE, Con_Minimize, this, "Minimize Teeworlds");
	m_pConsole->Register("connect", "s[host|ip]", CFGFLAG_CLIENT, Con_Connect, this, "Connect to the specified host/ip");
	m_pConsole->Register("disconnect", "", CFGFLAG_CLIENT, Con_Disconnect, this, "Disconnect from the server");
	m_pConsole->Register("ping", "", CFGFLAG_CLIENT, Con_Ping, this, "Ping the current server");
	m_pConsole->Register("screenshot", "", CFGFLAG_CLIENT, Con_Screenshot, this, "Take a screenshot");
	m_pConsole->Register("rcon", "r[rcon-command]", CFGFLAG_CLIENT, Con_Rcon, this, "Send specified command to rcon");
	m_pConsole->Register("rcon_auth", "s[password]", CFGFLAG_CLIENT, Con_RconAuth, this, "Authenticate to rcon");
	m_pConsole->Register("rcon_login", "s[username] r[password]", CFGFLAG_CLIENT, Con_RconLogin, this, "Authenticate to rcon with a username");
	m_pConsole->Register("play", "r[file]", CFGFLAG_CLIENT|CFGFLAG_STORE, Con_Play, this, "Play the file specified");
	m_pConsole->Register("record", "?s[file]", CFGFLAG_CLIENT, Con_Record, this, "Record to the file");
	m_pConsole->Register("stoprecord", "", CFGFLAG_CLIENT, Con_StopRecord, this, "Stop recording");
	m_pConsole->Register("add_demomarker", "", CFGFLAG_CLIENT, Con_AddDemoMarker, this, "Add demo timeline marker");
	m_pConsole->Register("add_favorite", "s[host|ip]", CFGFLAG_CLIENT, Con_AddFavorite, this, "Add a server as a favorite");
	m_pConsole->Register("remove_favorite", "s[host|ip]", CFGFLAG_CLIENT, Con_RemoveFavorite, this, "Remove a server from favorites");
	m_pConsole->Register("demo_slice_start", "", CFGFLAG_CLIENT, Con_DemoSliceBegin, this, "");
	m_pConsole->Register("demo_slice_end", "", CFGFLAG_CLIENT, Con_DemoSliceEnd, this, "");
	m_pConsole->Register("demo_play", "", CFGFLAG_CLIENT, Con_DemoPlay, this, "Play demo");
	m_pConsole->Register("demo_speed", "i[speed]", CFGFLAG_CLIENT, Con_DemoSpeed, this, "Set demo speed");

	m_pConsole->Chain("cl_timeout_seed", ConchainTimeoutSeed, this);

	m_pConsole->Chain("password", ConchainPassword, this);

	// used for server browser update
	m_pConsole->Chain("br_filter_string", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_gametype", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_serveraddress", ConchainServerBrowserUpdate, this);

	m_pConsole->Chain("gfx_screen", ConchainWindowScreen, this);
	m_pConsole->Chain("gfx_fullscreen", ConchainFullscreen, this);
	m_pConsole->Chain("gfx_borderless", ConchainWindowBordered, this);
	m_pConsole->Chain("gfx_vsync", ConchainWindowVSync, this);

	// DDRace


	#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help) m_pConsole->Register(name, params, flags, 0, 0, help);
	#include <game/ddracecommands.h>
}

static CClient *CreateClient()
{
	CClient *pClient = static_cast<CClient *>(malloc(sizeof(*pClient)));
	mem_zero(pClient, sizeof(CClient));
	return new(pClient) CClient;
}

void CClient::HandleConnectLink(const char *pArg)
{
	str_copy(m_aCmdConnect, pArg + sizeof(CONNECTLINK) - 1, sizeof(m_aCmdConnect));
}

/*
	Server Time
	Client Mirror Time
	Client Predicted Time

	Snapshot Latency
		Downstream latency

	Prediction Latency
		Upstream latency
*/

int main(int argc, const char **argv) // ignore_convention
{
	bool Silent = false;
	bool RandInitFailed = false;

	for(int i = 1; i < argc; i++) // ignore_convention
	{
		if(str_comp("-s", argv[i]) == 0 || str_comp("--silent", argv[i]) == 0) // ignore_convention
		{
			Silent = true;
#if defined(CONF_FAMILY_WINDOWS)
			FreeConsole();
#endif
			break;
		}
	}

	if(secure_random_init() != 0)
	{
		RandInitFailed = true;
	}

	CClient *pClient = CreateClient();
	IKernel *pKernel = IKernel::Create();
	pKernel->RegisterInterface(pClient, false);
	pClient->RegisterInterfaces();

	// create the components
	IEngine *pEngine = CreateEngine("DDNet", Silent, 1);
	IConsole *pConsole = CreateConsole(CFGFLAG_CLIENT);
	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_CLIENT, argc, argv); // ignore_convention
	IConfig *pConfig = CreateConfig();
	IEngineInput *pEngineInput = CreateEngineInput();
	IEngineMap *pEngineMap = CreateEngineMap();
	IEngineMasterServer *pEngineMasterServer = CreateEngineMasterServer();

	if(RandInitFailed)
	{
		dbg_msg("secure", "could not initialize secure RNG");
		return -1;
	}

	{
		bool RegisterFail = false;

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngine);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConsole);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConfig);

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineInput*>(pEngineInput)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IInput*>(pEngineInput), false);

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineMap*>(pEngineMap)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IMap*>(pEngineMap), false);

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineMasterServer*>(pEngineMasterServer)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IMasterServer*>(pEngineMasterServer), false);

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(CreateGameClient(), false);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pStorage);

		if(RegisterFail)
		{
			delete pKernel;
			pClient->~CClient();
			free(pClient);
			return -1;
		}
	}

	pEngine->Init();
	pConfig->Init();
	pEngineMasterServer->Init();
	pEngineMasterServer->Load();

	// register all console commands
	pClient->RegisterCommands();

	pKernel->RequestInterface<IGameClient>()->OnConsoleInit();

	// init client's interfaces
	pClient->InitInterfaces();

	// execute config file
	IOHANDLE File = pStorage->OpenFile(CONFIG_FILE, IOFLAG_READ, IStorage::TYPE_ALL);
	if(File)
	{
		io_close(File);
		pConsole->ExecuteFile(CONFIG_FILE);
	}
	else // fallback
	{
		pConsole->ExecuteFile("settings.cfg");
	}

	// execute autoexec file
	File = pStorage->OpenFile(AUTOEXEC_CLIENT_FILE, IOFLAG_READ, IStorage::TYPE_ALL);
	if(File)
	{
		io_close(File);
		pConsole->ExecuteFile(AUTOEXEC_CLIENT_FILE);
	}
	else // fallback
	{
		pConsole->ExecuteFile(AUTOEXEC_FILE);
	}

	if(g_Config.m_ClConfigVersion < 1)
	{
		if(g_Config.m_ClAntiPing == 0)
		{
			g_Config.m_ClAntiPingPlayers = 1;
			g_Config.m_ClAntiPingGrenade = 1;
			g_Config.m_ClAntiPingWeapons = 1;
		}
	}
	g_Config.m_ClConfigVersion = 1;

	// parse the command line arguments
	if(argc == 2 && str_startswith(argv[1], CONNECTLINK))
		pClient->HandleConnectLink(argv[1]);
	else if(argc > 1) // ignore_convention
		pConsole->ParseArguments(argc-1, &argv[1]); // ignore_convention

	pClient->Engine()->InitLogfile();

#if defined(CONF_FAMILY_WINDOWS)
	if(!g_Config.m_ClShowConsole)
		FreeConsole();
#endif

	// run the client
	dbg_msg("client", "starting...");

	spClient = pClient;
	signal(SIGINT, OnCrash);
	signal(SIGILL, OnCrash);
	signal(SIGFPE, OnCrash);
	signal(SIGABRT, OnCrash);

	pClient->Run();

	// write down the config and quit
	pConfig->Save();

	delete pKernel;
	pClient->~CClient();
	free(pClient);

	return 0;
}

// DDRace

const char *CClient::GetCurrentMap()
{
	return m_aCurrentMap;
}

const char *CClient::GetCurrentMapPath()
{
	return m_aCurrentMapPath;
}

unsigned CClient::GetMapCrc()
{
	return m_pMap->Crc();
}

void CClient::RaceRecord_Start(const char *pFilename)
{
	if(State() != IClient::STATE_ONLINE)
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demorec/record", "client is not online");
	else
		m_DemoRecorder[RECORDER_RACE].Start(Storage(), m_pConsole, pFilename, GameClient()->NetVersion(), m_aCurrentMap, m_pMap->Sha256(), m_pMap->Crc(), "client", m_pMap->MapSize(), 0, m_pMap->File());
}

void CClient::RaceRecord_Stop()
{
	if(m_DemoRecorder[RECORDER_RACE].IsRecording())
		m_DemoRecorder[RECORDER_RACE].Stop();
}

bool CClient::RaceRecord_IsRecording()
{
	return m_DemoRecorder[RECORDER_RACE].IsRecording();
}


void CClient::RequestDDNetInfo()
{
	char aUrl[256];
	static bool s_IsWinXP = os_is_winxp_or_lower();
	if(s_IsWinXP)
		str_copy(aUrl, "http://info.ddnet.tw/info", sizeof(aUrl));
	else
		str_copy(aUrl, "https://info.ddnet.tw/info", sizeof(aUrl));

	if(g_Config.m_BrIndicateFinished)
	{
		char aEscaped[128];
		EscapeUrl(aEscaped, sizeof(aEscaped), g_Config.m_PlayerName);
		str_append(aUrl, "?name=", sizeof(aUrl));
		str_append(aUrl, aEscaped, sizeof(aUrl));
	}

	m_pDDNetInfoTask = std::make_shared<CGetFile>(Storage(), aUrl, DDNET_INFO_TMP, IStorage::TYPE_SAVE, true);
	Engine()->AddJob(m_pDDNetInfoTask);
}

int CClient::GetPredictionTime()
{
	int64 Now = time_get();
	return (int)((m_PredictedTime.Get(Now)-m_GameTime[g_Config.m_ClDummy].Get(Now))*1000/(float)time_freq());
}
