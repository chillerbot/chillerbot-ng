// ChillerDragon 2020 - chillerbot ux

#include <engine/config.h>
#include <engine/console.h>
#include <engine/keys.h>
#include <game/client/components/camera.h>
#include <game/client/components/chat.h>
#include <game/client/components/chillerbot/chathelper.h>
#include <game/client/components/chillerbot/version.h>
#include <game/client/components/controls.h>
#include <game/client/components/menus.h>
#include <game/client/components/voting.h>
#include <game/client/race.h>
#include <game/generated/protocol.h>
#include <game/version.h>

#include "chillerbotux.h"

void CChillerBotUX::OnRender()
{
	RenderSpeedHud();
	RenderEnabledComponents();
	FinishRenameTick();
	ChangeTileNotifyTick();
	m_ForceDir = 0;
	CampHackTick();
	if(!m_ForceDir && m_LastForceDir)
	{
		m_pClient->m_pControls->m_InputDirectionRight[g_Config.m_ClDummy] = 0;
		m_pClient->m_pControls->m_InputDirectionLeft[g_Config.m_ClDummy] = 0;
	}
	m_LastForceDir = m_ForceDir;
}

void CChillerBotUX::ChangeTileNotifyTick()
{

}

void CChillerBotUX::RenderSpeedHud()
{
}

void CChillerBotUX::RenderEnabledComponents()
{

}

void CChillerBotUX::EnableComponent(const char *pComponent, const char *pNoteShort, const char *pNoteLong)
{

}

void CChillerBotUX::DisableComponent(const char *pComponent)
{

}

bool CChillerBotUX::SetComponentNoteShort(const char *pComponent, const char *pNote)
{

	return false;
}

bool CChillerBotUX::SetComponentNoteLong(const char *pComponent, const char *pNote)
{

	return false;
}

void CChillerBotUX::CampHackTick()
{
	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;
	if(!g_Config.m_ClCampHack)
		return;
	if(!m_CampHackX1 || !m_CampHackX2 || !m_CampHackY1 || !m_CampHackY2)
		return;
	if(g_Config.m_ClCampHack < 2 || GameClient()->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_HAMMER)
		return;
	if(m_CampHackX1 > GameClient()->m_Snap.m_pLocalCharacter->m_X)
	{
		m_pClient->m_pControls->m_InputDirectionRight[g_Config.m_ClDummy] = 1;
		m_pClient->m_pControls->m_InputDirectionLeft[g_Config.m_ClDummy] = 0;
		m_ForceDir = 1;
	}
	else if(m_CampHackX2 < GameClient()->m_Snap.m_pLocalCharacter->m_X)
	{
		m_pClient->m_pControls->m_InputDirectionRight[g_Config.m_ClDummy] = 0;
		m_pClient->m_pControls->m_InputDirectionLeft[g_Config.m_ClDummy] = 1;
		m_ForceDir = -1;
	}
}

void CChillerBotUX::SelectCampArea(int Key)
{
	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;
	if(g_Config.m_ClCampHack != 1)
		return;
	if(Key != KEY_MOUSE_1)
		return;
	if(GameClient()->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_GUN)
		return;
	m_CampClick++;
	if(m_CampClick % 2 == 0)
	{
		// UNSET IF CLOSE
		vec2 Current = vec2(GameClient()->m_Snap.m_pLocalCharacter->m_X, GameClient()->m_Snap.m_pLocalCharacter->m_Y);
		vec2 CrackPos1 = vec2(m_CampHackX1, m_CampHackY1);
		float dist = distance(CrackPos1, Current);
		if(dist < 100.0f)
		{
			m_CampHackX1 = 0;
			m_CampHackY1 = 0;
			GameClient()->m_pChat->AddLine(-2, 0, "Unset camp[1]");
			return;
		}
		vec2 CrackPos2 = vec2(m_CampHackX2, m_CampHackY2);
		dist = distance(CrackPos2, Current);
		if(dist < 100.0f)
		{
			m_CampHackX2 = 0;
			m_CampHackY2 = 0;
			GameClient()->m_pChat->AddLine(-2, 0, "Unset camp[2]");
			return;
		}
		// SET OTHERWISE
		if(m_CampClick == 2)
		{
			m_CampHackX1 = GameClient()->m_Snap.m_pLocalCharacter->m_X;
			m_CampHackY1 = GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		else
		{
			m_CampHackX2 = GameClient()->m_Snap.m_pLocalCharacter->m_X;
			m_CampHackY2 = GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf),
			"Set camp[%d] %d",
			m_CampClick == 2 ? 1 : 2,
			GameClient()->m_Snap.m_pLocalCharacter->m_X / 32);
		GameClient()->m_pChat->AddLine(-2, 0, aBuf);
	}
	if(m_CampClick > 3)
		m_CampClick = 0;
}

void CChillerBotUX::FinishRenameTick()
{
	// if(!m_pClient->m_Snap.m_pLocalCharacter)
	// 	return;
	// if(!g_Config.m_ClFinishRename)
	// 	return;
	// vec2 Pos = m_pClient->m_PredictedChar.m_Pos;
	// if(CRaceHelper::IsNearFinish(m_pClient, Pos))
	// {
	// 	if(!m_IsNearFinish)
	// 	{
	// 		m_IsNearFinish = true;
	// 		m_pClient->SendFinishName();
	// 	}
	// }
	// else
	// {
	// 	m_IsNearFinish = false;
	// }
}

void CChillerBotUX::OnInit()
{
	m_pChatHelper = m_pClient->m_pChatHelper;

	m_AfkTill = 0;
	m_AfkActivity = 0;
	m_aAfkMessage[0] = '\0';

	for(auto &c : m_aEnabledComponents)
	{
		c.m_aName[0] = '\0';
		c.m_aNoteShort[0] = '\0';
		c.m_aNoteLong[0] = '\0';
	}
	UpdateComponents();
	m_GotoSwitchOffset = 0;
	m_GotoSwitchLastX = -1;
	m_GotoSwitchLastY = -1;
	m_GotoTeleOffset = 0;
	m_GotoTeleLastX = -1;
	m_GotoTeleLastY = -1;
}

void CChillerBotUX::UpdateComponents()
{

}

void CChillerBotUX::OnConsoleInit()
{
	Console()->Register("afk", "?i[minutes]?r[message]", CFGFLAG_CLIENT, ConAfk, this, "Activate afk mode (auto chat respond)");
	Console()->Register("camp", "?i[left]i[right]?s[tile|raw]", CFGFLAG_CLIENT, ConCampHack, this, "Activate camp mode relative to tee");
	Console()->Register("camp_abs", "i[x1]i[y1]i[x2]i[y2]?s[tile|raw]", CFGFLAG_CLIENT, ConCampHackAbs, this, "Activate camp mode absolute in the map");
	Console()->Register("uncamp", "", CFGFLAG_CLIENT, ConUnCampHack, this, "Same as cl_camp_hack 0 but resets walk input");
	Console()->Register("goto_switch", "i[number]?i[offset]", CFGFLAG_CLIENT, ConGotoSwitch, this, "Pause switch found (at offset) with given number");
	Console()->Register("goto_tele", "i[number]?i[offset]", CFGFLAG_CLIENT, ConGotoTele, this, "Pause tele found (at offset) with given number");

	Console()->Chain("cl_camp_hack", ConchainCampHack, this);
	Console()->Chain("cl_auto_reply", ConchainAutoReply, this);
	Console()->Chain("cl_finish_rename", ConchainFinishRename, this);
}

void CChillerBotUX::ConchainCampHack(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->EnableComponent("camp hack");
	else
		pSelf->DisableComponent("camp hack");
}

void CChillerBotUX::ConchainAutoReply(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->EnableComponent("auto reply");
	else
		pSelf->DisableComponent("auto reply");
}

void CChillerBotUX::ConchainFinishRename(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->GetInteger(0))
		pSelf->EnableComponent("finish rename");
	else
		pSelf->DisableComponent("finish rename");
}

void CChillerBotUX::ConAfk(IConsole::IResult *pResult, void *pUserData)
{
	((CChillerBotUX *)pUserData)->GoAfk(pResult->NumArguments() ? pResult->GetInteger(0) : -1, pResult->GetString(1));
}

void CChillerBotUX::ConCampHackAbs(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	int Tile = 32;
	if(!str_comp(pResult->GetString(0), "raw"))
		Tile = 1;
	g_Config.m_ClCampHack = 2;
	pSelf->EnableComponent("camp hack");
	// absolute all coords
	if(pResult->NumArguments() > 1)
	{
		if(pSelf->GameClient()->m_Snap.m_pLocalCharacter)
		{
			pSelf->m_CampHackX1 = Tile * pResult->GetInteger(0);
			pSelf->m_CampHackY1 = Tile * pResult->GetInteger(1);
			pSelf->m_CampHackX2 = Tile * pResult->GetInteger(2);
			pSelf->m_CampHackY2 = Tile * pResult->GetInteger(3);
		}
		return;
	}
}

void CChillerBotUX::ConCampHack(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	int Tile = 32;
	if(!str_comp(pResult->GetString(0), "raw"))
		Tile = 1;
	g_Config.m_ClCampHack = 2;
	pSelf->EnableComponent("camp hack");
	if(!pResult->NumArguments())
	{
		if(pSelf->GameClient()->m_Snap.m_pLocalCharacter)
		{
			pSelf->m_CampHackX1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X - 32 * 3;
			pSelf->m_CampHackY1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
			pSelf->m_CampHackX2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X + 32 * 3;
			pSelf->m_CampHackY2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		return;
	}
	// relative left and right
	if(pResult->NumArguments() > 1)
	{
		if(pSelf->GameClient()->m_Snap.m_pLocalCharacter)
		{
			pSelf->m_CampHackX1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X - Tile * pResult->GetInteger(0);
			pSelf->m_CampHackY1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
			pSelf->m_CampHackX2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X + Tile * pResult->GetInteger(1);
			pSelf->m_CampHackY2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		return;
	}
}

void CChillerBotUX::ConUnCampHack(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	g_Config.m_ClCampHack = 0;
	pSelf->DisableComponent("camp hack");
	pSelf->m_pClient->m_pControls->m_InputDirectionRight[g_Config.m_ClDummy] = 0;
	pSelf->m_pClient->m_pControls->m_InputDirectionLeft[g_Config.m_ClDummy] = 0;
}

void CChillerBotUX::ConGotoSwitch(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pSelf->GotoSwitch(pResult->GetInteger(0), pResult->NumArguments() > 1 ? pResult->GetInteger(1) : -1);
}

void CChillerBotUX::ConGotoTele(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pSelf->GotoTele(pResult->GetInteger(0), pResult->NumArguments() > 1 ? pResult->GetInteger(1) : -1);
}

void CChillerBotUX::GotoSwitch(int Number, int Offset)
{
	int Match = -1;
	int MatchX = -1;
	int MatchY = -1;
	for(int x = 0; x < Collision()->GetWidth(); x++)
	{
		for(int y = 0; y < Collision()->GetHeight(); y++)
		{
			int i = y * Collision()->GetWidth() + x;
			if(Number == Collision()->GetSwitchNumber(i))
			{
				Match++;
				if(Offset != -1)
				{
					if(Match == Offset)
					{
						MatchX = x;
						MatchY = y;
						m_GotoSwitchOffset = Match;
						goto set_view;
					}
					continue;
				}
				MatchX = x;
				MatchY = y;
				if(Match == m_GotoSwitchOffset)
					goto set_view;
			}
		}
	}
set_view:
	if(MatchX == -1 || MatchY == -1)
		return;
	if(Match < m_GotoSwitchOffset)
		m_GotoSwitchOffset = -1;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "set_view %d %d", MatchX, MatchY);
	Console()->ExecuteLine(aBuf);
	m_GotoSwitchOffset++;
}

void CChillerBotUX::GotoTele(int Number, int Offset)
{
	int Match = -1;
	int MatchX = -1;
	int MatchY = -1;
	for(int x = 0; x < Collision()->GetWidth(); x++)
	{
		for(int y = 0; y < Collision()->GetHeight(); y++)
		{
			int i = y * Collision()->GetWidth() + x;
			int Tele = Collision()->TeleLayer()[i].m_Number;
			if(Number == Tele)
			{
				Match++;
				if(Offset != -1)
				{
					if(Match == Offset)
					{
						MatchX = x;
						MatchY = y;
						m_GotoTeleOffset = Match;
						goto set_view;
					}
					continue;
				}
				MatchX = x;
				MatchY = y;
				if(m_GotoTeleLastX != -1 && m_GotoTeleLastY != -1)
				{
					if(distance(vec2(m_GotoTeleLastX, m_GotoTeleLastY), vec2(x, y)) < 10.0f)
					{
						m_GotoTeleOffset++;
						continue;
					}
				}
				m_GotoTeleLastX = x;
				m_GotoTeleLastY = y;
				if(Match == m_GotoTeleOffset)
					goto set_view;
			}
		}
	}
set_view:
	if(MatchX == -1 || MatchY == -1)
		return;
	if(Match < m_GotoTeleOffset)
		m_GotoTeleOffset = -1;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "set_view %d %d", MatchX, MatchY);
	Console()->ExecuteLine(aBuf);
	m_GotoTeleOffset++;
}

void CChillerBotUX::GoAfk(int Minutes, const char *pMsg)
{
	if(pMsg)
	{
		str_copy(m_aAfkMessage, pMsg, sizeof(m_aAfkMessage));
		if((unsigned long)str_length(pMsg) > sizeof(m_aAfkMessage))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "error: afk message too long %d/%lu", str_length(pMsg), sizeof(m_aAfkMessage));
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
			return;
		}
	}
	m_AfkTill = time_get() + time_freq() * 60 * Minutes;
	m_AfkActivity = 0;
	m_pChatHelper->ClearLastAfkPingMessage();
	EnableComponent("afk");
}

void CChillerBotUX::ReturnFromAfk(const char *pChatMessage)
{
	if(!m_AfkTill)
		return;
	if(pChatMessage && pChatMessage[0] != '/')
	{
		if(m_IgnoreChatAfk > 0)
			m_IgnoreChatAfk--;
		else
			m_AfkActivity += 400;
	}
	m_AfkActivity++;
	if(m_AfkActivity < 200)
		return;
	m_pClient->m_pChat->AddLine(-2, 0, "[chillerbot-ux] welcome back :)");
	m_AfkTill = 0;
	DisableComponent("afk");
}
