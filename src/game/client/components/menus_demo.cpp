/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/tl/string.h>

#include <base/math.h>

#include <engine/demo.h>
#include <engine/keys.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/localization.h>

#include <game/generated/client_data.h>

#include "maplayers.h"
#include "menus.h"

bool CMenus::DemoFilterChat(const void *pData, int Size, void *pUser)
{
  return false;
}

int CMenus::UiDoListboxEnd(float *pScrollValue, bool *pItemActivated, bool *pListBoxActive)
{
  return 0;
}

int CMenus::DemolistFetchCallback(const char *pName, time_t Date, int IsDir, int StorageType, void *pUser)
{
	return 0;
}

void CMenus::DemolistPopulate()
{

}

void CMenus::DemolistOnUpdate(bool Reset)
{

}

bool CMenus::FetchHeader(CDemoItem &Item)
{
  return false;
}
