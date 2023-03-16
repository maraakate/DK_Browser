#ifndef _GSB_H
#define _GSB_H

#define _WIN32_IE		0x0600
#define _WIN32_WINNT	0x0501

#include <winsock2.h>
#include <winuser.h>
#include <malloc.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <Mmsystem.h>
#include <string.h>
#include <direct.h>
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <process.h>
#include "resource.h"

#define CINTERFACE
#include "dialogsizer.h"

#pragma warning (disable : 4996)

#define MAX_INFO_STRING	4096
#define MAX_INFO_KV_LEN	64
#define MAX_NETNAMELENGTH	16
#define MAX_CLIENTS	64

typedef struct _INFOSTRINGS
{
	_TCHAR key[MAX_INFO_KV_LEN];
	_TCHAR value[MAX_INFO_KV_LEN];
} INFOSTRINGS;

typedef struct _PLAYERINFO
{
	_TCHAR	playername[MAX_NETNAMELENGTH];
	int		ping;
	int		score;
} PLAYERINFO;

typedef struct _SERVERINFO
{
	unsigned int	ip;
	unsigned short	port;
	_TCHAR		szHostName[MAX_INFO_KV_LEN];
	_TCHAR		szMapName[MAX_INFO_KV_LEN];
	_TCHAR		szVersion[MAX_INFO_KV_LEN];
	_TCHAR		szGameDate[MAX_INFO_KV_LEN];
	_TCHAR		szGameName[MAX_INFO_KV_LEN];
	int			curClients;
	int			maxClients;
	DWORD		startPing;
	DWORD		ping;
	PLAYERINFO	players[MAX_CLIENTS];
	BOOL		gotResponse;
	int			attempts;
	_TCHAR		szGameMode[MAX_INFO_KV_LEN];
	int			timelimit;
	int			reservedSlots;
	_TCHAR		infostrings[MAX_INFO_STRING];
	INFOSTRINGS	infoKvP[MAX_INFO_STRING];
} SERVERINFO;

enum
{
	INFO_MAX_PLAYERS,
	INFO_MAP_NAME,
	INFO_TIMELIMIT,
	INFO_FRAGLIMIT,
	INFO_GAMENAME,
	INFO_HOSTNAME,
	INFO_GAMEMODE,
	INFO_GAMEDATE,
	INFO_VERSION,
	INFO_IPADDRESS,
	INFO_PLAYERS,
	INFO_PING,
	INFO_RESERVED_SLOTS,
	INFO_MAX,
};

typedef enum
{
	CSORT_DEFAULT,
	CSORT_NUMERIC,
} sorttype_t;

typedef struct
{
	const char *key;
	int			mapping;
} infokey_t;

typedef struct
{
	_TCHAR *columnTitle;
	int			relWidth;
	int			mapping;
	UINT_PTR			fofs;
	sorttype_t	sortType;
	int			defaultSort;
} column_t;

#define	FOFS(x) (UINT_PTR)&(((SERVERINFO *)0)->x)

const infokey_t	infokey[] =
{
	{"maxclients", INFO_MAX_PLAYERS},
	{"mapname", INFO_MAP_NAME},
	{"timelimit", INFO_TIMELIMIT},
	{"gamename", INFO_GAMENAME},
	{"hostname", INFO_HOSTNAME},
	{"gametype", INFO_GAMEMODE},
	{"version", INFO_GAMEDATE},
	{"version", INFO_VERSION},
};

const column_t	columns[] =
{
	{_T("Host Name"), 30, INFO_HOSTNAME, FOFS(szHostName), CSORT_DEFAULT, 1},
	{_T("IP Address"), 20, INFO_IPADDRESS, FOFS(ip), CSORT_DEFAULT, 1},
	{_T("Map"), 10, INFO_MAP_NAME, FOFS (szMapName), CSORT_DEFAULT, 1},
	{_T("Players"), 10, INFO_PLAYERS, FOFS(curClients), CSORT_NUMERIC, -1},
	{_T("Game Mode"), 10, INFO_GAMEMODE, FOFS(szGameMode), CSORT_DEFAULT, 1},
	{_T("Game Date"), 10, INFO_GAMEDATE, FOFS(szGameDate), CSORT_DEFAULT, 1},
	{_T("Ping"), 10, INFO_PING, FOFS (ping), CSORT_NUMERIC, 1},
};

#define	APP_NAME _T("Daikatana Server Browser")
#define	MASTER_SERVER _T("master.maraakate.org")
#define	MASTER_PORT	27900
#define QUERY_STYLE	2
#define GAME_NAME _T("Daikatana")
#define DEFAULT_EXECUTABLE_NAME _T("daikatana.exe")
#define GAME_REQUEST_PACKET "\xFF\xFF\xFF\xFF" "status"
#define QUERY_RESPONSE_HEADER_LEN	12
#define QUERY_RESPONSE_SERVER_LEN	6
#define GAME_REGPATH _T("SOFTWARE\\Eidos Interactive\\Daikatana\\1.00.000")
#define GAME_REGKEY _T("SourcePath")

#endif // _GSB_H
