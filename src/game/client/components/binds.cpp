/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/config.h>
#include <engine/shared/config.h>
#include "binds.h"

bool CBinds::CBindsSpecial::OnInput(IInput::CEvent Event)
{
	return false;
}

CBinds::CBinds()
{
	mem_zero(m_apKeyBindings, sizeof(m_apKeyBindings));
	m_SpecialBinds.m_pBinds = this;
}

CBinds::~CBinds()
{
	for(int i = 0; i < KEY_LAST; i++)
		if(m_apKeyBindings[i])
			free(m_apKeyBindings[i]);
}

void CBinds::Bind(int KeyID, const char *pStr, bool FreeOnly)
{
  return;
}

bool CBinds::OnInput(IInput::CEvent e)
{
	return false;
}

void CBinds::UnbindAll()
{
	for(int i = 0; i < KEY_LAST; i++)
	{
		if(m_apKeyBindings[i])
			free(m_apKeyBindings[i]);
		m_apKeyBindings[i] = 0;
	}
}

const char *CBinds::Get(int KeyID)
{
	return "";
}

const char *CBinds::GetKey(const char *pBindStr)
{
	return "";
}

void CBinds::SetDefaults()
{

}

void CBinds::OnConsoleInit()
{
	// bindings
	IConfig *pConfig = Kernel()->RequestInterface<IConfig>();
	if(pConfig)
		pConfig->RegisterCallback(ConfigSaveCallback, this);

	Console()->Register("bind", "s[key] r[command]", CFGFLAG_CLIENT, ConBind, this, "Bind key to execute the command");
	Console()->Register("dump_binds", "?s[key]", CFGFLAG_CLIENT, ConDumpBinds, this, "Print command executed by this keybindind or all binds");
	Console()->Register("unbind", "s[key]", CFGFLAG_CLIENT, ConUnbind, this, "Unbind key");
	Console()->Register("unbindall", "", CFGFLAG_CLIENT, ConUnbindAll, this, "Unbind all keys");

	// default bindings
	SetDefaults();
}

void CBinds::ConBind(IConsole::IResult *pResult, void *pUserData)
{

}

void CBinds::ConDumpBinds(IConsole::IResult *pResult, void *pUserData)
{

}

void CBinds::ConUnbind(IConsole::IResult *pResult, void *pUserData)
{

}

void CBinds::ConUnbindAll(IConsole::IResult *pResult, void *pUserData)
{

}

int CBinds::GetKeyID(const char *pKeyName)
{
	return 0;
}

void CBinds::ConfigSaveCallback(IConfig *pConfig, void *pUserData)
{

}

// DDRace

void CBinds::SetDDRaceBinds(bool FreeOnly)
{

}
