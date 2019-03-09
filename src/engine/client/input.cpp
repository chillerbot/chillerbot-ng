/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/input.h>
#include <engine/keys.h>

#include "input.h"

//print >>f, "int inp_key_code(const char *key_name) { int i; if (!strcmp(key_name, \"-?-\")) return -1; else for (i = 0; i < 512; i++) if (!strcmp(key_strings[i], key_name)) return i; return -1; }"

void CInput::AddEvent(char *pText, int Key, int Flags)
{

}

CInput::CInput()
{
	mem_zero(m_aInputCount, sizeof(m_aInputCount));
	mem_zero(m_aInputState, sizeof(m_aInputState));

	m_InputCounter = 1;
	m_InputGrabbed = 0;

	m_LastRelease = 0;
	m_ReleaseDelta = -1;

	m_NumEvents = 0;
	m_MouseFocus = true;

	m_VideoRestartNeeded = 0;
	m_pClipboardText = NULL;

	m_CountEditingText = 0;
}

void CInput::Init()
{

}

void CInput::MouseRelative(float *x, float *y)
{

}

void CInput::MouseModeAbsolute()
{

}

void CInput::MouseModeRelative()
{

}

int CInput::MouseDoubleClick()
{
	return 0;
}

const char* CInput::GetClipboardText()
{
	return "";
}

void CInput::SetClipboardText(const char *Text)
{

}

void CInput::Clear()
{
	mem_zero(m_aInputState, sizeof(m_aInputState));
	mem_zero(m_aInputCount, sizeof(m_aInputCount));
	m_NumEvents = 0;
}

bool CInput::KeyState(int Key) const
{
  return false;
}

void CInput::NextFrame()
{

}

bool CInput::GetIMEState()
{
	return m_CountEditingText > 0;
}

void CInput::SetIMEState(bool Activate)
{

}

const char* CInput::GetIMECandidate()
{
		return "";
}

int CInput::GetEditingCursor()
{
	return m_EditingCursor;
}

int CInput::Update()
{
	return 0;
}

int CInput::VideoRestartNeeded()
{
	return 0;
}

IEngineInput *CreateEngineInput() { return new CInput; }
