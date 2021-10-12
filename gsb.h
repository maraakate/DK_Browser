#define _WIN32_IE		0x0600
#define _WIN32_WINNT	0x0501

#include <winsock2.h>
#include <winuser.h>
#include <Winhttp.h>
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
#include "resource.h"

#define CINTERFACE
#include "dialogsizer.h"


typedef struct _PLAYERINFO
{
	_TCHAR	playername[32];
	int		ping;
	int		score;
} PLAYERINFO;

typedef struct _SERVERINFO
{
	unsigned int	ip;
	unsigned short	port;
	_TCHAR		szHostName[256];
	_TCHAR		szMapName[256];
	_TCHAR		szVersion[256];
	_TCHAR		szGameDate[64];
	_TCHAR		szGameName[64];
	int			curClients;
	int			maxClients;
	DWORD		startPing;
	DWORD		ping;
	PLAYERINFO	players[64];
	BOOL		gotResponse;
	int			attempts;
	int			anticheat;
	int			timelimit;
	int			reservedSlots;
	int			cID;
} SERVERINFO;

enum
{
	INFO_MAX_PLAYERS,
	INFO_MAP_NAME,
	INFO_TIMELIMIT,
	INFO_FRAGLIMIT,
	INFO_GAMENAME,
	INFO_HOSTNAME,
	INFO_ANTICHEAT,
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
	const char	*key;
	int			mapping;
} infokey_t;

typedef struct
{
	_TCHAR		*columnTitle;
	int			relWidth;
	int			mapping;
	int			fofs;
	sorttype_t	sortType;
	int			defaultSort;
} column_t;

//#define __TREMULOUS__
//#define __GLOOM__
#define __QUAKE2__

#define	FOFS(x) (int)&(((SERVERINFO *)0)->x)
#define	FOFP(x) (int)&(((PLAYERINFO *)0)->x)

#ifdef __TREMULOUS__

const infokey_t	infokey[] =
{
	{"sv_maxclients", INFO_MAX_PLAYERS},
	{"sv_privateclients", INFO_RESERVED_SLOTS},
	{"mapname", INFO_MAP_NAME},
	{"timelimit", INFO_TIMELIMIT},
	{"gamename", INFO_GAMENAME},
	{"sv_hostname", INFO_HOSTNAME},
	{"sv_pure", INFO_ANTICHEAT},
	{"version", INFO_VERSION},
};

const column_t	columns[] = 
{
	{_T("Host Name"), 40, INFO_HOSTNAME, FOFS(szHostName), CSORT_DEFAULT, 1},
	{_T("IP Address"), 20, INFO_IPADDRESS, FOFS(ip), CSORT_NUMERIC, 0},
	{_T("Map"), 10, INFO_MAP_NAME, FOFS (szMapName), CSORT_DEFAULT, 1},
	{_T("Mod"), 10, INFO_GAMENAME, FOFS (szGameName), CSORT_DEFAULT, 1},
	//{_T("Mod"), 10, INFO_GAMEDATE, FOFS (szGameDate), CSORT_DEFAULT, 1},
	//{"Pure", 10, INFO_ANTICHEAT, FOFS (anticheat), CSORT_DEFAULT, 1},
	{_T("Players"), 11, INFO_PLAYERS, FOFS(curClients), CSORT_NUMERIC, -1},
	//{"Timelimit", 10, INFO_TIMELIMIT, FOFS(timelimit), CSORT_NUMERIC, 1},
	//{"Server Version", 35, INFO_GAMENAME, FOFS(szV), CSORT_DEFAULT, 1},
	{_T("Ping"), 9, INFO_PING, FOFS (ping), CSORT_NUMERIC, 1},
};	

#define APP_NAME _T("Tremulous Server Browser")
#define MASTER_SERVER _T("master.tremulous.net")
#define MASTER_PORT 30710
#define	QUERY_STYLE	2
#define GAME_NAME _T("Tremulous")
#define DEFAULT_EXECUTABLE_NAME _T("tremulous.exe")
#define GAME_REQUEST_PACKET "\xFF\xFF\xFF\xFFgetstatus\n"
#define QUERY_RESPONSE_HEADER_LEN	23
#define QUERY_RESPONSE_SERVER_LEN	7
#define GAME_CMDLINE _T("\"%s\\%s\" +connect \"%s\"")
#define GAME_REGPATH _T("SOFTWARE\\Tremulous")
#define GAME_REGKEY _T("InstallDir")
#define DEFERRED_REFRESH 1
#endif

#ifdef __GLOOM__

const infokey_t	infokey[] =
{
	{"maxclients", INFO_MAX_PLAYERS},
	{"mapname", INFO_MAP_NAME},
	{"timelimit", INFO_TIMELIMIT},
	{"gamename", INFO_GAMENAME},
	{"hostname", INFO_HOSTNAME},
	{"anticheat", INFO_ANTICHEAT},
	{"gamedate", INFO_GAMEDATE},
	{"version", INFO_VERSION},
};

const column_t	columns[] = 
{
	{_T("Host Name"), 30, INFO_HOSTNAME, FOFS(szHostName), CSORT_DEFAULT, 1},
	{_T("IP Address"), 20, INFO_IPADDRESS, FOFS(ip), CSORT_NUMERIC, 0},
	{_T("Map"), 10, INFO_MAP_NAME, FOFS (szMapName), CSORT_DEFAULT, 1},
	{_T("Players"), 10, INFO_PLAYERS, FOFS(curClients), CSORT_NUMERIC, -1},
	{_T("AC"), 5, INFO_ANTICHEAT, FOFS(anticheat), CSORT_NUMERIC, 1},
	{_T("Game Date"), 15, INFO_GAMEDATE, FOFS(szGameDate), CSORT_DEFAULT, 1},
	{_T("Ping"), 10, INFO_PING, FOFS (ping), CSORT_NUMERIC, 1},
};	

#define	APP_NAME _T("Gloom Server Browser")
#define	MASTER_SERVER _T("master.planetgloom.com")
#define	MASTER_PORT	27900
#define QUERY_STYLE	1
#define GAME_NAME _T("Quake II")
#define DEFAULT_EXECUTABLE_NAME _T("quake2.exe")
#define GAME_REQUEST_PACKET "\xFF\xFF\xFF\xFFstatus\n"
#define QUERY_RESPONSE_HEADER_LEN	12
#define QUERY_RESPONSE_SERVER_LEN	6
#define GAME_CMDLINE _T("\"%s\\%s\" +set game \"gloom\" +connect \"%s\"")
#define GAME_REGPATH _T("SOFTWARE\\Gloom")
#define GAME_REGKEY _T("InstallDir")
#endif

#ifdef __QUAKE2__
//#define DEFERRED_REFRESH 1
const infokey_t	infokey[] =
{
	{"maxclients", INFO_MAX_PLAYERS},
	{"mapname", INFO_MAP_NAME},
	{"timelimit", INFO_TIMELIMIT},
	{"gamename", INFO_GAMENAME},
	{"hostname", INFO_HOSTNAME},
	{"anticheat", INFO_ANTICHEAT},
	{"gamedate", INFO_GAMEDATE},
	{"version", INFO_VERSION},
};

const column_t	columns[] = 
{
	{_T("Host Name"), 30, INFO_HOSTNAME, FOFS(szHostName), CSORT_DEFAULT, 1},
	{_T("IP Address"), 20, INFO_IPADDRESS, FOFS(ip), CSORT_NUMERIC, 0},
	{_T("Map"), 10, INFO_MAP_NAME, FOFS (szMapName), CSORT_DEFAULT, 1},
	{_T("Players"), 10, INFO_PLAYERS, FOFS(curClients), CSORT_NUMERIC, -1},
	{_T("AC"), 5, INFO_ANTICHEAT, FOFS(anticheat), CSORT_NUMERIC, 1},
	{_T("Mod"), 15, INFO_GAMENAME, FOFS(szGameName), CSORT_DEFAULT, 1},
	{_T("Ping"), 10, INFO_PING, FOFS (ping), CSORT_NUMERIC, 1},
};	

#define	APP_NAME _T("Quake II Server Browser")
//#define	MASTER_SERVER _T("master.planetgloom.com")
//#define	MASTER_PORT	27900
//#define QUERY_STYLE	1
#define GAME_NAME _T("Quake II")
#define DEFAULT_EXECUTABLE_NAME _T("quake2.exe")
#define GAME_REQUEST_PACKET "\xFF\xFF\xFF\xFFstatus\n"
#define QUERY_RESPONSE_HEADER_LEN	0
#define QUERY_RESPONSE_SERVER_LEN	7
#define GAME_CMDLINE _T("\"%s\\%s\" +connect \"%s\"")
//#define GAME_REGPATH _T("SOFTWARE\\Gloom")
//#define GAME_REGKEY _T("InstallDir")
#endif