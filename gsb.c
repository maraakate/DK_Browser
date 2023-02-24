/*
    Daikatana Server Browser
    Copyright (C) 2021  Frank Sapone (maraakate.org)

    Based on the code of
    Gloom Server Browser
    Copyright (C) 2001-2007  Richard Stanway (www.r1ch.net)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "gsb.h"

#define tsizeof(x) (sizeof((x))/sizeof((x)[0]))

#define MAX_LOADSTRING 100

#define LV_VIEW_ICON        0x0000
#define LV_VIEW_DETAILS     0x0001
#define LV_VIEW_SMALLICON   0x0002
#define LV_VIEW_LIST        0x0003
#define LV_VIEW_TILE        0x0004
#define LV_VIEW_MAX         0x0004

#define LVM_SETVIEW         (LVM_FIRST + 142)

BOOL	g_isXP;

BOOL	listIsDetailed = FALSE;

CRITICAL_SECTION	critModList;

SERVERINFO *servers;

//terminating strncpy
#define Q_strncpy(dst, src, len) \
do { \
	strncpy ((dst), (src), (len)); \
	(dst)[(len)] = 0; \
} while (0)

#define ID_TRAY_ICON						5500
#define ID_TRAY_RESTORE_CONTEXT_MENU_ITEM	3500
#define ID_TRAY_ABOUT_CONTEXT_MENU_ITEM		3501
#define ID_TRAY_QUIT_CONTEXT_MENU_ITEM		3502
#define WM_TRAYICON						(WM_USER + 1)

HMENU		hTrayMenu;	// tray icon menu
PNOTIFYICONDATAW	notifyIconData;	// data for tray icon

_TCHAR		q2Path[MAX_PATH];
_TCHAR		q2Exe[MAX_PATH];
_TCHAR		*q2Buddies;
int			q2PacketsPerSecond;

DWORD		q2GoodPing, q2MediumPing;

int			numBuddies;

typedef struct
{
	_TCHAR	name[32];
} playername_t;

playername_t	*buddyList;


int globalServers = 0;
int globalPlayers = 0;

SOCKET sckUpdate;

DWORD	show_empty, show_full;

HWND hWndStatus;
HWND hWndPlayerList;
HWND hWndList;
HWND hwndMain;

BOOL	scanInProgress;

HANDLE	hTerminateScanEvent;

// Global Variables:
HINSTANCE hThisInstance;								// current instance

HANDLE PingHandle = NULL;
DWORD PingThreadID = 0;

HIMAGELIST	hCountry;

// Forward declarations of functions included in this code module:
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK Proxy(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void UpdatePlayerList (VOID);
static void WndInitNotifyIconData (void);

int numServers = 0;

static int lastSortOrder;

//#define nParts 4
void RequestHandler (void);


typedef enum DKB_PROCESS_DPI_AWARENESS {
	DKB_PROCESS_DPI_UNAWARE = 0,
	DKB_PROCESS_SYSTEM_DPI_AWARE = 1,
	DKB_PROCESS_PER_MONITOR_DPI_AWARE = 2
} DKB_PROCESS_DPI_AWARENESS;

static void SetHighDPIMode (void)
{
	/* For Vista, Win7 and Win8 */
	BOOL(WINAPI * SetProcessDPIAware)(void) = NULL;

	/* Win8.1 and later */
	HRESULT(WINAPI * SetProcessDpiAwareness)(DKB_PROCESS_DPI_AWARENESS dpiAwareness) = NULL;
	HINSTANCE userDLL;
	HINSTANCE shcoreDLL;

	userDLL = LoadLibrary(_T("USER32.DLL"));
	if (userDLL)
		SetProcessDPIAware = (BOOL(WINAPI *)(void))GetProcAddress(userDLL, "SetProcessDPIAware");

	shcoreDLL = LoadLibrary(_T("SHCORE.DLL"));
	if (shcoreDLL)
		SetProcessDpiAwareness = (HRESULT(WINAPI *)(DKB_PROCESS_DPI_AWARENESS))GetProcAddress(shcoreDLL, "SetProcessDpiAwareness");


	if (SetProcessDpiAwareness)
		SetProcessDpiAwareness(DKB_PROCESS_PER_MONITOR_DPI_AWARE);
	else if (SetProcessDPIAware)
		SetProcessDPIAware();
}

_TCHAR *lstrstr (CONST _TCHAR *str1, CONST _TCHAR *lowered_str2)
{
	_TCHAR	*ret;
	_TCHAR	*lowered;

	lowered = _tcsdup (str1);
	if (!lowered)
	{
		return NULL;
	}
	_tcslwr_s (lowered, _tcslen(lowered)+1);

	ret = _tcsstr (lowered, lowered_str2);
	free (lowered);
	return ret;
}

VOID StatusBar (LPCTSTR fmt, ...)
{
	_TCHAR	buff[1024];
	va_list	va;

	if (!hWndStatus)
		return;

	va_start (va, fmt);
	StringCbVPrintf (buff, sizeof(buff), fmt, va);
	va_end (va);

	SendMessage (hWndStatus, SB_SETTEXT, (WPARAM) 0, (LPARAM)buff);
}

#ifdef LAUNCH_FROM_CONFIG
_TCHAR *HPIFindTADir (_TCHAR *Path)
{
	_TCHAR *TAdir;
	WIN32_FIND_DATA FileData; 
	HANDLE hSearch; 
	_TCHAR myPath[MAX_PATH];
	_TCHAR szNewPath[MAX_PATH]; 
	_TCHAR	*pathPtr;
	SIZE_T		pathLen;

	BOOL fFinished = FALSE; 

	StringCbCopy (myPath, sizeof(myPath), Path);
	StringCbCat (myPath, sizeof(myPath), _T("*"));

	hSearch = FindFirstFile (myPath, &FileData); 

	if (hSearch == INVALID_HANDLE_VALUE)
		return NULL;

	StringCbCopy (szNewPath, sizeof(szNewPath), Path);

	pathLen = _tcslen (szNewPath);
	pathPtr = szNewPath + pathLen;

	while (!fFinished) 
	{ 
		if (_tcslen (FileData.cFileName) + pathLen + 1 > tsizeof(szNewPath)-1)
		{
			FindClose(hSearch);
			return NULL;
		}

		if (FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && _tcscmp (FileData.cFileName, _T(".")) && 
			_tcscmp (FileData.cFileName, _T("..")))
		{
			StringCbCopy (pathPtr, sizeof(pathPtr), FileData.cFileName);
			StringCbCat (pathPtr, sizeof(pathPtr), _T("\\"));
			TAdir = HPIFindTADir (szNewPath);
			if (TAdir)
			{
				FindClose(hSearch);
				return TAdir;
			}
		}
		else if (!_tcsicmp (FileData.cFileName, q2Exe))
		{
			_TCHAR	buff[512];
			int		ans;

			StringCbPrintf (buff, sizeof(buff), APP_NAME _T(" found '%s' in:\r\n\t%s\r\nIs this the one you wish to use?"), q2Exe, Path);
			ans = MessageBox (hwndMain, buff, GAME_NAME _T(" Location"), MB_YESNO + MB_ICONQUESTION);
			if (ans == IDYES)
			{
				FindClose(hSearch);
				return Path;
			}
		}

		if (!FindNextFile(hSearch, &FileData))
			fFinished = TRUE; 
	} 

	// Close the search handle. 
	FindClose(hSearch);
	return NULL;
}
#endif

void SaveStuff(void)
{
	DWORD			result;
	HKEY			hk;
	WINDOWPLACEMENT	winPlacement;
	int				temp;
	
	RegCreateKeyEx  (HKEY_CURRENT_USER, _T("SOFTWARE\\r1ch.net\\") APP_NAME, 0, NULL, 0, KEY_WRITE, NULL, &hk, &result);
	if (!(RegOpenKeyEx (HKEY_CURRENT_USER, _T("SOFTWARE\\r1ch.net\\") APP_NAME, 0, KEY_WRITE, &hk)))
	{
		RegSetValueEx (hk, GAME_NAME _T(" Directory"), 0, REG_SZ, (const BYTE *)q2Path, (_tcslen(q2Path)+1) * sizeof(_TCHAR));
		RegSetValueEx (hk, GAME_NAME _T(" Executable"), 0, REG_SZ, (const BYTE *)q2Exe, (_tcslen(q2Exe)+1) * sizeof(_TCHAR));

		if (q2Buddies[0])
			RegSetValueEx (hk, _T("Buddy List"), 0, REG_SZ, (const BYTE *)q2Buddies, (_tcslen(q2Buddies)+1) * sizeof(_TCHAR));
		else
			RegSetValueEx (hk, _T("Buddy List"), 0, REG_SZ, (const BYTE *)_T(""), 1 * sizeof(_TCHAR));

		RegSetValueEx (hk, _T("Packets Per Second"), 0, REG_DWORD, (const BYTE *)&q2PacketsPerSecond, sizeof(DWORD));
		RegSetValueEx (hk, _T("Player View Style"), 0, REG_DWORD, (const BYTE *)&listIsDetailed, sizeof(DWORD));
		RegSetValueEx (hk, _T("Good Ping Threshold"), 0, REG_DWORD, (const BYTE *)&q2GoodPing, sizeof(DWORD));
		RegSetValueEx (hk, _T("Medium Ping Threshold"), 0, REG_DWORD, (const BYTE *)&q2MediumPing, sizeof(DWORD));

		temp = 2;
		RegSetValueEx (hk, _T("Version"), 0, REG_DWORD, (const BYTE *)&temp, sizeof(DWORD));

		RegSetValueEx (hk, _T("Sort Order"), 0, REG_DWORD, (const BYTE *)&lastSortOrder, sizeof(DWORD));

		result = SendDlgItemMessage (hwndMain, IDC_INCLUDE_EMPTY, BM_GETCHECK, 0, 0);
		RegSetValueEx (hk, _T("Show Empty Servers"), 0, REG_DWORD, (const BYTE *)&result, sizeof(DWORD));

		result = SendDlgItemMessage (hwndMain, IDC_INCLUDE_FULL, BM_GETCHECK, 0, 0);
		RegSetValueEx (hk, _T("Show Full Servers"), 0, REG_DWORD, (const BYTE *)&show_full, sizeof(DWORD));

		winPlacement.length = sizeof(winPlacement);

		if (GetWindowPlacement (hwndMain, &winPlacement))
			RegSetValueEx (hk, _T("Window Position"), 0, REG_BINARY, (const BYTE *)&winPlacement, sizeof(winPlacement));

		RegCloseKey (hk);
	}

	/*cache = fopen("./server-1.0.9.cache", "wb");
	if (cache) {
		for (i = 0; i < numServers; i++) {
			fwrite (&servers[i], sizeof(SERVERINFO), 1, cache);
		}
		fclose (cache);
	}*/

	WSACleanup();
}

void RunQ2 (NMITEMACTIVATE *info)
{
	HCURSOR		hcSave;
	LVITEM		pitem;
	BOOL foundIt = FALSE;
	_TCHAR		 Server[128];
	int			index;
	int size = MAX_PATH;

	if (info->iItem == -1)
		return;

	//busy = TRUE;

	hcSave = SetCursor(LoadCursor(NULL, IDC_WAIT));

	memset (&pitem, 0, sizeof(pitem));

	pitem.iItem = info->iItem;
	pitem.iSubItem = 0;
	pitem.mask = LVIF_PARAM;

	//index = SendMessage (hWndList, LVM_GETSELECTIONMARK, 0, 0);

	//q2Path[0] = 0;
	SendMessage (hWndList, LVM_GETITEM, 0, (LPARAM)(LPLVITEM)&pitem);

	index = pitem.lParam;

#ifdef LAUNCH_FROM_CONFIG
	if (!q2Path[0])
	{
		int drive;

		for (drive = 3; drive <= 26; drive++ )
		{
			_TCHAR	*temp;
			_TCHAR	dirtogo[4];
			StringCbPrintf (dirtogo, sizeof(dirtogo), _T("%c:\\"), drive + 'A' - 1);
			if (GetDriveType (dirtogo) == DRIVE_FIXED)
			{
				StatusBar (_T("Trying to find %s on %s..."), q2Exe, dirtogo);
				temp = HPIFindTADir (dirtogo);
				if (temp)
					StringCbCopy (q2Path, sizeof(q2Path), temp);

				if (q2Path[0])
					break;
			}
		}
	}
#endif

	{
		struct in_addr tmp;
		tmp.S_un.S_addr = servers[index].ip;
#ifdef _UNICODE
		StringCbPrintf (Server, sizeof(Server), _T("%S:%d"), inet_ntoa(tmp), servers[index].port); 
#else
		StringCbPrintf (Server, sizeof(Server), _T("%s:%d"), inet_ntoa(tmp), servers[index].port); 
#endif
	}

	//busy = FALSE;
	//SetClassLong(hwndMain, GCL_HCURSOR, (LONG) LoadCursor (hThisInstance, IDC_ARROW));   

	SetCursor(hcSave);

#ifdef LAUNCH_FROM_CONFIG
	if (!q2Path[0])
	{
		StatusBar (_T("Couldn't find a ") GAME_NAME _T(" executable!"));
		MessageBox (hwndMain, _T("Unable to find your ") GAME_NAME _T(" executable!\r\n\r\nYou can't join a server until you have specified where ") GAME_NAME _T(" is on your system.\n\nTry setting the executable name manually in the config if you have a different executable name."), _T("Can't find ") GAME_NAME, MB_OK | MB_ICONEXCLAMATION);
	}
	else
#endif
	{
		STARTUPINFO			s; 
		_TCHAR				q2buff[MAX_PATH];
		PROCESS_INFORMATION	p;
		_TCHAR				cmdLine[512];
		_TCHAR				myQ2Path[MAX_PATH];

		StatusBar (_T("Starting ") GAME_NAME _T("..."));

		StringCbCopy (myQ2Path, sizeof(myQ2Path), q2Path);

		memset (&s, 0, sizeof(s));
		s.cb = sizeof(s);

#ifdef LAUNCH_FROM_CONFIG
		StringCbPrintf (q2buff, sizeof(q2buff), _T("%s\\%s"), myQ2Path, q2Exe);
#else
		StringCbPrintf (q2buff, sizeof(q2buff), _T("%s"), q2Exe);
#endif
		if (GetFileAttributes (q2buff) == -1)
		{
			_TCHAR buff[512];
			StatusBar (_T("Couldn't find %s! Check your config."), q2Exe);
			StringCbPrintf (buff, sizeof(buff), _T("Error: Couldn't find a ") GAME_NAME _T(" executable:\r\n\r\n\t%s: File Not Found"), q2buff);
			MessageBox (hwndMain, buff, _T("Couldn't find ") GAME_NAME, MB_OK | MB_ICONEXCLAMATION);
			return;
		}

#ifdef LAUNCH_FROM_CONFIG
		SetCurrentDirectory (myQ2Path);
		StringCbPrintf (cmdLine, sizeof(cmdLine), GAME_CMDLINE, myQ2Path, q2Exe, Server);
		CreateProcess (NULL, cmdLine, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, myQ2Path, &s, &p);
#else
		myQ2Path[0] = _T('\0');
		StringCbPrintf (cmdLine, sizeof(cmdLine), GAME_CMDLINE, q2Exe, Server);
		CreateProcess (NULL, cmdLine, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &s, &p);
#endif
		CloseHandle (p.hProcess);
		CloseHandle (p.hThread);

		//SaveStuff ();
		DestroyWindow (hwndMain);
	}
}

BOOL InitListViewImageLists(HWND hWndListView) 
{
	int	i;

	HICON hiconItem;     // icon for list-view items 
	// HIMAGELIST hLarge;   // image list for icon view 
	HIMAGELIST hSmall;   // image list for other views 
	HIMAGELIST hSmall2;   // image list for other views 

	// Create the full-sized icon image lists. 
	hSmall = ImageList_Create(16, 14, ILC_COLOR32, 8,0); 

	hCountry = ImageList_Create(18, 14, ILC_COLOR32, 65, 0); 

	hSmall2 = ImageList_Create(16, 14, ILC_COLOR32, 1, 0); 

	ImageList_SetBkColor (hSmall, CLR_NONE);

	// Add an icon to each image list.  
	hiconItem = LoadIcon(hThisInstance, MAKEINTRESOURCE(IDI_SERVER)); 
	ImageList_AddIcon (hSmall, hiconItem); 
	DestroyIcon(hiconItem);

	hiconItem = LoadIcon(hThisInstance, MAKEINTRESOURCE(IDI_ICON4)); 
	ImageList_AddIcon(hSmall, hiconItem); 
	DestroyIcon(hiconItem);

	hiconItem = LoadIcon(hThisInstance, MAKEINTRESOURCE(IDI_ICON6)); 
	ImageList_AddIcon(hSmall, hiconItem); 
	DestroyIcon(hiconItem);

	hiconItem = LoadIcon(hThisInstance, MAKEINTRESOURCE(IDI_ICON5)); 
	ImageList_AddIcon(hSmall, hiconItem); 
	DestroyIcon(hiconItem);

	hiconItem = LoadIcon(hThisInstance, MAKEINTRESOURCE(IDI_LINUX)); 
	ImageList_AddIcon(hSmall, hiconItem); 
	DestroyIcon(hiconItem); 

	hiconItem = LoadIcon(hThisInstance, MAKEINTRESOURCE(IDI_WIN32)); 
	ImageList_AddIcon(hSmall, hiconItem); 
	DestroyIcon(hiconItem);

	hiconItem = LoadIcon(hThisInstance, MAKEINTRESOURCE(IDI_FREEBSD)); 
	ImageList_AddIcon(hSmall, hiconItem); 
	DestroyIcon(hiconItem);

	hiconItem = LoadIcon(hThisInstance, MAKEINTRESOURCE(IDI_MACOSX)); 
	ImageList_AddIcon(hSmall, hiconItem); 
	DestroyIcon(hiconItem);

	for (i = IDB_BITMAP0; i <= IDB_BITMAP64; i++)
	{
		HBITMAP	hBmp;
		hBmp = LoadBitmap (hThisInstance, MAKEINTRESOURCE(i));
		ImageList_Add (hCountry, hBmp, NULL);
		DeleteObject (hBmp);
	}

	hiconItem = LoadIcon(hThisInstance, MAKEINTRESOURCE(IDI_ICON1)); 
	ImageList_AddIcon(hSmall2, hiconItem); 
	DestroyIcon(hiconItem); 
 
	/*********************************************************
	Usually you have multiple icons; therefore, the previous
	four lines of code can be inside a loop. The following code 
	shows such a loop. The icons are defined in the application's
	header file as resources, which are numbered consecutively
	starting with IDS_FIRSTICON. The number of icons is
	defined in the header file as C_ICONS.

	for(index = 0; index < C_ICONS; index++)
	{
	hIconItem = LoadIcon (hInst, MAKEINTRESOURCE (IDS_FIRSTICON +
	index));
	ImageList_AddIcon(hSmall, hIconItem);
	ImageList_AddIcon(hLarge, hIconItem);
	Destroy(hIconItem);
	}
	*********************************************************/

	// Assign the image lists to the list-view control. 
	//ListView_SetImageList(hwndLV, hLarge, LVSIL_NORMAL); 
	ListView_SetImageList(hWndListView, hSmall, LVSIL_SMALL);
	ListView_SetImageList(hWndPlayerList, hSmall2, LVSIL_SMALL);
	return TRUE; 
}

int CALLBACK CompareFunc (LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	int				index;
	const column_t	*column;
	SERVERINFO		*a, *b;
	int				inverted;

	index = (int)lParamSort;

	if (index & 0x4000)
	{
		index &= ~0x4000;
		inverted = -1;
	}
	else
		inverted = 1;

	column = &columns[index];

	a = &servers[lParam1];
	b = &servers[lParam2];

	if (column->sortType == SORT_DEFAULT)
	{
		return _tcsicmp ((_TCHAR *)((BYTE *)a + column->fofs), (_TCHAR *)((BYTE *)b + column->fofs)) * inverted * column->defaultSort;
	}
	else
	{
		if (*(UINT_PTR *)((BYTE *)a + column->fofs) < *(UINT_PTR *)((BYTE *)b + column->fofs))
			return -1 * inverted * column->defaultSort;
		else if (*(UINT_PTR *)((BYTE *)a + column->fofs) > *(UINT_PTR *)((BYTE *)b + column->fofs))
			return 1 * inverted * column->defaultSort;
		else
			return 0;
	}
}

int CALLBACK CompareFunc2 (LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	PLAYERINFO	*a, *b;

	a = (PLAYERINFO *)lParam1;
	b = (PLAYERINFO *)lParam2;

	switch ((int)lParamSort)
	{
		case 0:
			return _tcscmp (a->playername, b->playername);
		case 1:
			if (a->score < b->score)
				return 1;
			else if (a->score > b->score)
				return -1;
			else
				return 0;
		case 2:
			if (a->ping < b->ping)
				return -1;
			else if (a->ping > b->ping)
				return 1;
			else
				return 0;
		default:
			return 0;
	}
}

int GetImageForVersion (_TCHAR *version)
{
	int		image;
	_TCHAR	*lowered;

	lowered = _tcsdup (version);
	if (!lowered)
	{
		return -1;
	}
	_tcslwr_s (lowered, _tcslen(lowered)+1);

	if (_tcsstr (lowered, _T("linux")))
		image = 4;
	else if (_tcsstr (lowered, _T("mingw")) || _tcsstr (lowered, _T("win")) || _tcsstr (lowered, _T("msvc")))
		image = 5;
	else if (_tcsstr (lowered, _T("freebsd")))
		image = 6;
	else if (_tcsstr (lowered, _T("osx")) || _tcsstr (lowered, _T("ppc")))
		image = 7;
	else
		image = 0;

	free (lowered);

	return image;
}

HWND LV_CreateListView (HWND hWndParent, HINSTANCE hInst, int NumServers,
						SERVERINFO *pServer)
{
	HICON		icon;
	HWND		hWndList;      // handle to the list view window
	RECT		rcl;           // rectangle for setting the size of the window
	int			index;          // index used in FOR loops
	LVCOLUMN	lvC;      // list view column structure
	int			iWidth;         // column width
	int			width[6];
	_TCHAR		*columnTitle[6];

	hWndList = GetDlgItem (hwndMain, IDC_SERVERLIST);
	GetClientRect(hWndList, &rcl);
	iWidth = (rcl.right - rcl.left);

	/*width[0] = 35;
	width[1] = 20;
	width[2] = 10;
	width[3] = 11;
	width[4] = 15;
	width[5] = 9;

	columnTitle[0] = "Host Name";
	columnTitle[1] = "Address";
	columnTitle[2] = "Map";
	columnTitle[3] = "Players";
	columnTitle[4] = "DLL Date";
	columnTitle[5] = "Ping";*/

	SendMessage (hWndList, LVM_SETEXTENDEDLISTVIEWSTYLE, (WPARAM)0, (LPARAM) LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER );

	lvC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_ORDER;
	lvC.fmt = LVCFMT_LEFT;

	iWidth -= 16;

	// Add the columns.
	for (index = 0; index < sizeof(columns) / sizeof(columns[0]); index++)
	{
		lvC.cx = (int)(iWidth * (columns[index].relWidth / 100.0f));
		lvC.iSubItem = index;
		lvC.iOrder = index;
		lvC.pszText = columns[index].columnTitle;

		if (columns[index].mapping == INFO_PING)
			lastSortOrder = index;

		if (ListView_InsertColumn(hWndList, index, &lvC) == -1)
			return NULL;
	}

	hWndPlayerList = GetDlgItem (hwndMain, IDC_PLAYERS);
	SendMessage (hWndPlayerList, LVM_SETEXTENDEDLISTVIEWSTYLE, (WPARAM)0, (LPARAM) LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT);

	GetClientRect(hWndPlayerList, &rcl);
	iWidth = (rcl.right - rcl.left);

	columnTitle[0] = _T("Name");
	columnTitle[1] = _T("Score");
	columnTitle[2] = _T("Ping");

	width[0] = 50;
	width[1] = 25;
	width[2] = 25;

	iWidth -= 16;

	for (index = 0; index < 3; index++)
	{
		lvC.cx = (int)(iWidth * (width[index] / 100.0f));
		lvC.iSubItem = index;
		lvC.pszText = columnTitle[index];
		if (ListView_InsertColumn(hWndPlayerList, index, &lvC) == -1)
			return NULL;
	}

	GetClientRect(hwndMain, &rcl);
	iWidth = (rcl.right - rcl.left);

	hWndStatus = CreateWindow (STATUSCLASSNAME, _T(""), WS_CHILD | WS_VISIBLE, 0, 0, iWidth, 0,  hwndMain, (HMENU)NULL, hInst, 0);

	width[0] = 40;
	width[1] = 15;
	width[2] = 15;

	for (index =0; index < 3; index++)
	{
		if (index)
			width[index] = (int)(iWidth * (width[index] / 70.0f)) + width[index-1];
		else
			width[index] = (int)(iWidth * (width[index] / 70.0f));
	}

	// Tell the status bar to create the window parts. 
	SendMessage(hWndStatus, SB_SETPARTS, (WPARAM) 3, 
		(LPARAM) width);

	SetWindowLongPtr (hWndStatus, GWL_ID, IDC_STATUSBAR);

	icon = (HICON)LoadImage(hThisInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 14, LR_DEFAULTCOLOR);
	SendMessage(hWndStatus, SB_SETICON, (WPARAM)(INT) 1, 
		(LPARAM)icon);

	icon = (HICON)LoadImage(hThisInstance, MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, 16, 14, LR_DEFAULTCOLOR);
	SendMessage(hWndStatus, SB_SETICON, (WPARAM)(INT) 2, 
		(LPARAM)icon);

	icon = (HICON)LoadImage(hThisInstance, MAKEINTRESOURCE(IDI_ICON3), IMAGE_ICON, 14, 14, LR_DEFAULTCOLOR);
	SendMessage(hWndStatus, SB_SETICON, (WPARAM)(INT) 0, 
		(LPARAM)icon);

	StatusBar (_T("Updating Server List..."));
	SendMessage (hWndStatus, SB_SETTEXT, (WPARAM) 1, (LPARAM) _T("0 Players"));
	SendMessage (hWndStatus, SB_SETTEXT, (WPARAM) 2, (LPARAM) _T("0 Servers"));

	InitListViewImageLists (hWndList);

	return (hWndList);
}

char *GetLine (char **contents, int *len)
{
	int		num;
	int		i;
	char	line[2048];
	char	*ret;

	num = 0;
	line[0] = '\0';

	if (*len <= 0)
		return NULL;

	for (i = 0; i < *len; i++)
	{
		if ((*contents)[i] == '\n')
		{
			*contents += (num + 1);
			*len -= (num + 1);
			line[num] = '\0';
			ret = (char *)malloc (sizeof(line));
			if (!ret)
			{
				return NULL;
			}
			StringCbCopyA (ret, sizeof(line), line);
			return ret;
		}
		else
		{
			line[num] = (*contents)[i];
			num++;
		}
	}

	ret = (char *)malloc (sizeof(line));
	if (!ret)
	{
		return NULL;
	}
	StringCbCopyA (ret, sizeof(line), line);
	return ret;
}

VOID UpdateInfoCounts (VOID)
{
	_TCHAR	buff[512];

	StringCbPrintf (buff, sizeof(buff), _T("%d Player%s"), globalPlayers, globalPlayers != 1 ? _T("s") : _T(""));
	SendMessage (hWndStatus, SB_SETTEXT, (WPARAM) 1, (LPARAM)buff);

	StringCbPrintf (buff, sizeof(buff), _T("%d Server%s"), globalServers, globalServers != 1 ? _T("s") : _T(""));
	SendMessage (hWndStatus, SB_SETTEXT, (WPARAM) 2, (LPARAM)buff);
}

SOCKET				querySocket;
HANDLE				hQueryReadEvent;
HANDLE				hGotAllServers;
CRITICAL_SECTION	critServerList;
#ifdef DEFERRED_REFRESH
BOOL				needRefresh;
#endif
HANDLE				hTerminateReaderEvent;

VOID ParseQueryResponse (BYTE *recvBuff, int result, SERVERINFO *server)
{
	CHAR		lastToken[64];
	DWORD		now, msecs;
	int			players;
	int			i;
	SIZE_T len = 0;
	char		*token, *seps, *p, *rLine;
	LVITEM		lvI;
	char		*serverInfo[INFO_MAX];
	char		*next_token;

	mbstowcs(server->infostrings, (_TCHAR *)(recvBuff + 10), tsizeof(server->infostrings) - 1);
	now = timeGetTime();

	server->gotResponse = TRUE;

	players = 0;

	p = recvBuff;

	//discard print
	rLine = GetLine (&p, &result);
	free (rLine);

	msecs = now - server->startPing;

	server->ping = msecs;

	//serverinfo
	rLine = GetLine (&p, &result);

	seps = "\\";
	lastToken[0] = 0;

	ZeroMemory (&serverInfo, sizeof(serverInfo));

	next_token = NULL;

	/* Establish string and get the first token: */
	token = strtok_s (rLine, seps, &next_token);
	while( token != NULL )
	{
		StringCbCopyA (lastToken, sizeof(lastToken), token);

		token = strtok_s (NULL, seps, &next_token);
		if (!token)
			break;

		for (i = 0; i < sizeof(infokey) / sizeof(infokey[0]); i++)
		{
			if (!_stricmp (lastToken, infokey[i].key))
				serverInfo[infokey[i].mapping] = token;
		}

		
		token = strtok_s (NULL, seps, &next_token);
	}

	if (serverInfo[INFO_MAX_PLAYERS])
		server->maxClients = atoi(serverInfo[INFO_MAX_PLAYERS]);
	else
		server->maxClients = 0;

	if (serverInfo[INFO_RESERVED_SLOTS])
		server->reservedSlots = atoi(serverInfo[INFO_RESERVED_SLOTS]);
	else
		server->reservedSlots = 0;

	if (serverInfo[INFO_TIMELIMIT])
		server->timelimit = atoi(serverInfo[INFO_TIMELIMIT]);
	else
		server->timelimit = 0;

	if (serverInfo[INFO_MAP_NAME])
#ifdef _UNICODE
		MultiByteToWideChar (CP_ACP, 0, serverInfo[INFO_MAP_NAME], -1, server->szMapName, tsizeof(server->szMapName)-1);
#else
		Q_strncpy (server->szMapName, serverInfo[INFO_MAP_NAME], sizeof(server->szMapName)-1);
#endif
	else
		StringCbCopy (server->szMapName, sizeof(server->szMapName), _T("?"));

	if (serverInfo[INFO_GAMENAME] && serverInfo[INFO_GAMENAME][0])
#ifdef _UNICODE
		MultiByteToWideChar (CP_ACP, 0, serverInfo[INFO_GAMENAME], -1, server->szGameName, tsizeof(server->szGameName)-1);
#else
		Q_strncpy (server->szGameName, serverInfo[INFO_MAP_NAME], sizeof(server->szGameName)-1);
#endif
	else
		StringCbCopy (server->szGameName, sizeof(server->szGameName), _T("default"));

	if (serverInfo[INFO_GAMEMODE] && serverInfo[INFO_GAMEMODE][0])
#ifdef _UNICODE
		MultiByteToWideChar (CP_ACP, 0, serverInfo[INFO_GAMEMODE], -1, server->szGameMode, tsizeof(server->szGameMode)-1);
#else
		Q_strncpy (server->szGameMode, serverInfo[INFO_GAMEMODE], sizeof(server->szGameMode)-1);
#endif
	else
		StringCbCopy (server->szGameMode, sizeof(server->szGameMode), _T("default"));

	if (serverInfo[INFO_VERSION])
#ifdef _UNICODE
		MultiByteToWideChar (CP_ACP, 0, serverInfo[INFO_VERSION], -1, server->szVersion, tsizeof(server->szVersion)-1);
#else
		Q_strncpy (server->szVersion, serverInfo[INFO_VERSION], sizeof(server->szVersion)-1);
#endif
	else
		StringCbCopy (server->szVersion, sizeof(server->szVersion), _T(""));

	if (serverInfo[INFO_HOSTNAME])
	{
#if QUERY_STYLE == 2
		_TCHAR	*p, *q;
		int		skip;
		BOOL	found_ascii = FALSE;
#endif

#ifdef _UNICODE
		MultiByteToWideChar (CP_ACP, 0, serverInfo[INFO_HOSTNAME], -1, server->szHostName, tsizeof(server->szHostName)-1);
#else
		Q_strncpy (server->szHostName, serverInfo[INFO_HOSTNAME], sizeof(server->szHostName)-1);
#endif

#if QUERY_STYLE == 2
		p = q = server->szHostName;
		skip = 0;
		while (p[0])
		{
			if (p[0] == '^')
				skip++;
			else if (!_istprint (p[0]) || p[0] == '\t' || (!found_ascii && _istspace(p[0])))
			{
			}
			else
			{
				if (!skip)
				{
					found_ascii = TRUE;
					q[0] = p[0];
					q++;
				}
				else
					skip--;
			}

			p++;
		}
		q[0] = 0;
#endif
	}
	else
		StringCbCopy (server->szHostName, sizeof(server->szHostName), _T(""));

	if (serverInfo[INFO_GAMEDATE])
	{
		_TCHAR	*p;
		_TCHAR *built = _T("Built ");
		int		sc = 0;

#ifdef _UNICODE
		MultiByteToWideChar (CP_ACP, 0, serverInfo[INFO_GAMEDATE], -1, server->szGameDate, tsizeof(server->szGameDate)-1);
#else
		Q_strncpy (server->szGameDate, serverInfo[INFO_GAMEDATE], sizeof(server->szGameDate)-1);
#endif

		p = _tcsstr(server->szGameDate, built);
		if (p)
		{
			StringCbCopy(server->szGameDate, sizeof(server->szGameDate), p + 6);
			while (*p && *(p+1))
			{
				if (_istspace (*p) && !_istspace (*(p+1)))
					sc++;

				if (sc == 3)
				{
					*p = 0;
					break;
				}
				p++;
			}
		}
	}

	len = _tcslen(server->szGameDate);
#ifdef _UNICODE
	if (len >= (sizeof(server->szGameDate) / sizeof(*server->szGameDate)) - 1)
		len = (sizeof(server->szGameDate) / sizeof(*server->szGameDate)) - 1;
#endif

	if (len > 0)
	{
		/* FS: Builds before 2022 are reporting a period at the end.  So fix it here. */
		if (server->szGameDate[len - 1] == _T('.'))
			len--;
	}

	server->szGameDate[len] = _T('\0');

	free (rLine);

	//playerinfo
	next_token = NULL;
	seps = " ";
	while (rLine = GetLine (&p, &result))
	{
		/* Establish string and get the first token: */
		token = strtok_s (rLine, seps, &next_token);
		if (!token)
			continue;

		server->players[players].score = atoi(token);

		token = strtok_s (NULL, seps, &next_token);
		if (!token)
			continue;

		server->players[players].ping = atoi(token);

		token = strtok_s ( NULL, "\"", &next_token);

		if (token)
		{
#if QUERY_STYLE == 2
			_TCHAR	*p, *q;
			int		skip;
#endif

#ifdef _UNICODE
			MultiByteToWideChar (CP_ACP, 0, token, -1, server->players[players].playername, tsizeof(server->players[players].playername)-1);
#else
			Q_strncpy (server->players[players].playername, token, sizeof(server->players[players].playername)-1);
#endif

#if QUERY_STYLE == 2
		p = q = server->players[players].playername;
		skip = 0;
		while (p[0])
		{
			if (p[0] == '^')
				skip++;
			else if (!_istprint (p[0]))
			{
			}
			else
			{
				if (!skip)
				{
					q[0] = p[0];
					q++;
				}
				else
					skip--;
			}

			p++;
		}
		q[0] = 0;
#endif

		}
		else
			server->players[players].playername[0] = '\0';

		players++;
		free (rLine);
	}

	server->curClients = players;

	if (!((!show_full && server->curClients >= server->maxClients - server->reservedSlots) || 
		(!show_empty && server->curClients == 0)))
	{
		memset (&lvI, 0, sizeof(lvI));

		lvI.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;

		lvI.iItem = globalServers;

		lvI.iImage = GetImageForVersion (server->szVersion);

		lvI.iSubItem = 0;
		lvI.lParam = (LPARAM)(server - servers);
		lvI.pszText = LPSTR_TEXTCALLBACK;

		i = SendMessage (hWndList, LVM_INSERTITEM, 0, (LPARAM) (const LPLVITEM) &lvI);
		SendMessage (hWndList, LVM_SORTITEMS, lastSortOrder, (LPARAM)CompareFunc);

#ifdef DEFERRED_REFRESH
		needRefresh = TRUE;
		ValidateRect (hWndList, NULL);
#endif

		globalServers ++;
		globalPlayers += players;

		UpdateInfoCounts ();
	}

	StatusBar (_T("Found %d player%s on '%s'"), players, players != 1 ? _T("s") : _T(""), server->szHostName);
}

LONG QueryReader (VOID *arg)
{
	BYTE		response[4096];
	SOCKADDR_IN	from;
	int			fromlen;
	int			length;
	int			i;
	int			ret;
	int			pending;
	HANDLE		handleArray[2];

	handleArray[0] = hQueryReadEvent;
	handleArray[1] = hTerminateReaderEvent;

	timeBeginPeriod (1);

	for (;;)
	{
		ret = WaitForMultipleObjects (2, handleArray, FALSE, INFINITE);

		if (ret == WAIT_FAILED)
			break;

		if (ret == (1 - WAIT_OBJECT_0))
			break;

		fromlen = sizeof(from);

		length = recvfrom (querySocket, response, sizeof(response), 0, (SOCKADDR *)&from, &fromlen);
		if (length <= 0)
			continue;

		response[length] = 0;

		EnterCriticalSection (&critServerList);

		for (i = 0; i < numServers; i++)
		{
			if (*(unsigned int *)&from.sin_addr == servers[i].ip &&
				ntohs(from.sin_port) == servers[i].port && servers[i].startPing && !servers[i].gotResponse)
			{
				ParseQueryResponse (response, length, &servers[i]);
				break;
			}
		}

		pending = 0;
		for (i = 0; i < numServers; i++)
		{
			if (!servers[i].gotResponse)
				pending++;
		}

		if (!pending)
			SetEvent (hGotAllServers);

		LeaveCriticalSection (&critServerList);
	}

	CloseHandle (hTerminateReaderEvent);
	CloseHandle (hQueryReadEvent);

	timeEndPeriod (1);

	return 0;
}

void SendRequest (SERVERINFO *server)
{
	SOCKADDR_IN		addr;
	BYTE			request[] = GAME_REQUEST_PACKET;	
	int				result;

	ZeroMemory (&addr, sizeof(addr));

	*(unsigned int *)&addr.sin_addr = server->ip;
	addr.sin_port = htons(server->port);
	addr.sin_family = AF_INET;

	if (server->attempts == 0)
		StatusBar (_T("Querying %S:%d..."), inet_ntoa(addr.sin_addr), server->port);
	else
		StatusBar (_T("Retrying %S:%d (attempt %d)..."), inet_ntoa(addr.sin_addr), server->port, server->attempts + 1);

	server->attempts++;
	server->startPing = timeGetTime ();
	result = sendto (querySocket, request, sizeof(request), 0, (struct sockaddr *)&addr, sizeof(addr));
	if (result == SOCKET_ERROR)
	{
		StatusBar (_T("Can't send: error %d"), WSAGetLastError());
	}
}

void RequestHandler (void)
{
	int		i;
	int		ret;
	int		temp;
	int		pending;
	DWORD	threadID;
	BOOL	needSleep;
	DWORD	lastListRefresh;
	_TCHAR	*text;

	HANDLE		handleArray[2];

	hQueryReadEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (hQueryReadEvent == NULL)
		return;

	hTerminateReaderEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (hTerminateReaderEvent == NULL)
		return;

	hGotAllServers = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (hGotAllServers == NULL)
		return;

	querySocket = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (querySocket == INVALID_SOCKET)
		return;

	if (WSAEventSelect (querySocket, hQueryReadEvent, FD_READ))
		return;

	pending = 0;

	timeBeginPeriod (1);

	handleArray[0] = hGotAllServers;
	handleArray[1] = hTerminateScanEvent;

	_beginthreadex (NULL, 0, (LPTHREAD_START_ROUTINE)QueryReader, NULL, 0, &threadID);

	lastListRefresh = timeGetTime();

	for (i = 0; i < 3; i++)
	{
		for (temp = 0; temp < numServers; temp++)
		{
			needSleep = FALSE;
			EnterCriticalSection (&critServerList);
			if (!servers[temp].gotResponse)
			{
				needSleep = TRUE;
				SendRequest (&servers[temp]);
			}
			LeaveCriticalSection (&critServerList);

			if (needSleep)
			{
				ret = WaitForMultipleObjects (2, handleArray, FALSE, ((int)(1000.0f / (float)q2PacketsPerSecond)));
				if (ret == (1 - WAIT_OBJECT_0))
					goto abortLoop;
			}

#ifdef DEFERRED_REFRESH
			if (timeGetTime() - lastListRefresh > 250)
			{
				if (needRefresh)
					InvalidateRect (hWndList, NULL, FALSE);
				needRefresh = FALSE;
			}
#endif
		}

		pending = 0;

		EnterCriticalSection (&critServerList);
		for (temp = 0; temp < numServers; temp++)
		{
			if (!servers[temp].gotResponse)
				pending++;
		}
		LeaveCriticalSection (&critServerList);

		if (pending > 0)
		{
			ret = WaitForMultipleObjects (2, handleArray, FALSE, 1000);
			if (ret == WAIT_OBJECT_0)
			{
				pending = 0;
				break;
			}
		}
		else
			break;
	}

abortLoop:
	if (WaitForSingleObject (hTerminateScanEvent, 0) == WAIT_OBJECT_0)
	{
		pending = 0;
		EnterCriticalSection (&critServerList);
		for (temp = 0; temp < numServers; temp++)
		{
			if (!servers[temp].gotResponse)
				pending++;
		}
		LeaveCriticalSection (&critServerList);

		text = _T("Aborted");
	}
	else
		text = _T("Completed");

#ifdef DEFERRED_REFRESH
	if (needRefresh)
		InvalidateRect (hWndList, NULL, FALSE);
#endif

	SetEvent (hTerminateReaderEvent);

	closesocket (querySocket);
	CloseHandle (hGotAllServers);

	StatusBar (_T("%s. %d of %d servers responded."), text, numServers - pending, numServers);

	timeEndPeriod (1);
}


void GetServerList (void)
{
	SOCKET		master;
	HOSTENT		*hp;
	int			i, result, total;
	TIMEVAL		delay;
	fd_set		stoc;
	SOCKADDR_IN	dgFrom;
	char		recvBuff[0xFFFF], *p;
	char		queryBuff[256];
	BOOL		gotEOF;

	StatusBar (_T("Resolving ") MASTER_SERVER _T("..."));

#ifdef _UNICODE
	WideCharToMultiByte (CP_ACP, 0, MASTER_SERVER, -1, queryBuff, sizeof(queryBuff)-1, NULL, NULL);
#else
	Q_strncpy (queryBuff, MASTER_SERVER, sizeof(queryBuff)-1);
#endif

	hp = gethostbyname (queryBuff);

	if (!hp)
	{
		StatusBar (_T("Couldn't resolve ") MASTER_SERVER _T("."));
		SetDlgItemText (hwndMain, IDC_UPDATE, _T("Update"));
		scanInProgress = FALSE;
		EnableWindow (GetDlgItem (hwndMain, IDC_CONFIG), TRUE);
		EnableWindow (GetDlgItem (hwndMain, IDC_UPDATE), TRUE);
		_endthreadex (1); 
	}

	gotEOF = FALSE;

	memset (recvBuff, 0, sizeof(recvBuff));

	master = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	StatusBar (_T("Querying ") MASTER_SERVER _T("..."));
	for (i = 0; i < 3; i++)
	{
		dgFrom.sin_family = AF_INET;
		dgFrom.sin_port = htons (MASTER_PORT);
		memset (&dgFrom.sin_zero, 0, sizeof(dgFrom.sin_zero));
		memcpy (&dgFrom.sin_addr, hp->h_addr_list[0], sizeof(dgFrom.sin_addr));
#if QUERY_STYLE == 1
		StringCbCopyA (queryBuff, sizeof(queryBuff), "query");
		result = strlen (queryBuff);
#elif QUERY_STYLE == 2
		result = sprintf_s (queryBuff, sizeof(queryBuff), "\xFF\xFF\xFF\xFF" "getservers daikatana");
#if 0 /* FS: TODO: Implement. */
		if (show_full)
		{
			result += 5;
			strcat (queryBuff, " full");
		}

		if (show_empty)
		{
			result += 6;
			strcat (queryBuff, " empty");
		}
#endif
#endif
		result = sendto (master, queryBuff, result, 0, (const struct sockaddr *)&dgFrom, sizeof (dgFrom));

		if (result == SOCKET_ERROR)
			goto socketError;

		if (WaitForSingleObject (hTerminateScanEvent, 0) == WAIT_OBJECT_0)
			goto socketError;

		FD_ZERO (&stoc);
		FD_SET (master, &stoc);
		delay.tv_sec = 2;
		delay.tv_usec = 0;
		result = select (0, &stoc, NULL, NULL, &delay);
		if (result)
		{
			int fromlen = sizeof(dgFrom);
			result = recvfrom (master, recvBuff, sizeof(recvBuff), 0, (struct sockaddr *)&dgFrom, &fromlen);
			if (result > 0)
				break;
			else
				goto socketError;
		}
		else
		{
			StatusBar (_T("Retrying ") MASTER_SERVER _T(" (attempt %d)..."), i+1);
		}
	}

	if (i == 3)
	{
		scanInProgress = FALSE;
		closesocket (master);
		StatusBar (_T("No response from ") MASTER_SERVER _T("."));
		SetDlgItemText (hwndMain, IDC_UPDATE, _T("Update"));
		EnableWindow (GetDlgItem (hwndMain, IDC_CONFIG), TRUE);
		_endthreadex (1);
	}

	numServers = 0;
	total = 0;

	if (servers)
	{
		HeapFree (GetProcessHeap(), 0, servers);
		servers = NULL;
	}
reParse:

	p = recvBuff + QUERY_RESPONSE_HEADER_LEN;

	result -= QUERY_RESPONSE_HEADER_LEN;

	total += result / QUERY_RESPONSE_SERVER_LEN;

#if QUERY_STYLE == 2
	if (servers)
		servers = HeapReAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, servers, total * sizeof(SERVERINFO));
	else
#endif
		servers = HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, total * sizeof(SERVERINFO));

	while (result)
	{
#if QUERY_STYLE == 2
		if (!memcmp (p, "EOT\0\0\0", 6))
		{
			gotEOF = TRUE;
			break;
		}
#endif

		memcpy (&servers[numServers].ip, p, sizeof (servers[numServers].ip));
		p += 4;

		memcpy (&servers[numServers].port, p, sizeof (servers[numServers].port));
		servers[numServers].port = ntohs(servers[numServers].port);
		servers[numServers].port += 10; /* FS: Daikatana port is + 10 */
		p += 2;

		for (i = 0; i < numServers; i++)
		{
			if (servers[i].ip == servers[numServers].ip &&
				servers[i].port == servers[numServers].port)
			{
				numServers--;
				break;
			}
		}

#if QUERY_STYLE == 2
		if (p[0] == '\\')
			p++;
#endif

		result -= QUERY_RESPONSE_SERVER_LEN;

		numServers++;
	}

#if QUERY_STYLE == 2
	FD_ZERO (&stoc);
	FD_SET (master, &stoc);
	if (!gotEOF)
	{
		delay.tv_sec = 1;
		delay.tv_usec = 0;
	}
	else
	{
		delay.tv_sec = 0;
		delay.tv_usec = 500000;
	}

	if (WaitForSingleObject (hTerminateScanEvent, 0) == WAIT_OBJECT_0)
		goto socketError;

	result = select (0, &stoc, NULL, NULL, &delay);
	if (result)
	{
		int fromlen = sizeof(dgFrom);
		result = recvfrom (master, recvBuff, sizeof(recvBuff), 0, (struct sockaddr *)&dgFrom, &fromlen);
		if (result > 0)
			goto reParse;
		else
			goto socketError;
	}
#endif

	closesocket (master);

	//clear lists
	SendMessage (hWndList, LVM_DELETEALLITEMS, 0, 0);
	SendMessage (hWndPlayerList, LVM_DELETEALLITEMS, 0, 0);

	globalPlayers = globalServers = 0;

	StatusBar (_T("Querying servers..."));
	SendMessage (hWndStatus, SB_SETTEXT, (WPARAM) 1, (LPARAM) _T("0 Players"));
	SendMessage (hWndStatus, SB_SETTEXT, (WPARAM) 2, (LPARAM) _T("0 Servers"));

	RequestHandler ();

	SetDlgItemText (hwndMain, IDC_UPDATE, _T("Update"));
	EnableWindow (GetDlgItem (hwndMain, IDC_CONFIG), TRUE);
	EnableWindow (GetDlgItem (hwndMain, IDC_MOD), TRUE);
	EnableWindow (GetDlgItem (hwndMain, IDC_UPDATE), TRUE);

	ResetEvent (hTerminateScanEvent);
	scanInProgress = FALSE;

	_endthreadex (0);

socketError:
	closesocket (master);
	StatusBar (_T("Error querying ") MASTER_SERVER _T("."));

	SetDlgItemText (hwndMain, IDC_UPDATE, _T("Update"));
	EnableWindow (GetDlgItem (hwndMain, IDC_CONFIG), TRUE);
	EnableWindow (GetDlgItem (hwndMain, IDC_UPDATE), TRUE);
	EnableWindow (GetDlgItem (hwndMain, IDC_MOD), TRUE);

	ResetEvent (hTerminateScanEvent);
	scanInProgress = FALSE;

	_endthreadex (1);
}

void UpdateServerList (VOID)
{
	LVITEM		lvI;
	int			i;
	SERVERINFO	*server;
	int			newGlobalServers;

	show_empty = SendDlgItemMessage (hwndMain, IDC_INCLUDE_EMPTY, BM_GETCHECK, 0, 0);
	show_full = SendDlgItemMessage (hwndMain, IDC_INCLUDE_FULL, BM_GETCHECK, 0, 0);

	newGlobalServers = 0;

	for (i = 0; i < numServers; i++)
	{
		server = &servers[i];

		if (!server->gotResponse)
			continue;

		if (!show_full && server->curClients >= server->maxClients - server->reservedSlots)
			continue;

		if (!show_empty && server->curClients == 0)
			continue;

		newGlobalServers++;
	}

	if (newGlobalServers == globalServers)
		return;

	SendMessage (hWndList, LVM_DELETEALLITEMS, 0, 0);

	globalServers = globalPlayers = 0;

	for (i = 0; i < numServers; i++)
	{
		server = &servers[i];

		if (!server->gotResponse)
			continue;

		if (!show_full && server->curClients >= server->maxClients - server->reservedSlots)
			continue;

		if (!show_empty && server->curClients == 0)
			continue;

		globalServers++;
		globalPlayers += server->curClients;

		memset (&lvI, 0, sizeof(lvI));

		lvI.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;

		lvI.iItem = i;
		//lvI.iImage = 1;

		lvI.iImage = GetImageForVersion (server->szVersion);

		lvI.iSubItem = 0;
		lvI.lParam = (LPARAM)(server - servers);
		lvI.pszText = LPSTR_TEXTCALLBACK;
		SendMessage (hWndList, LVM_INSERTITEM, 0, (LPARAM) (const LPLVITEM) &lvI);
	}

	UpdatePlayerList ();
	UpdateInfoCounts ();

	SendMessage (hWndList, LVM_SORTITEMS, lastSortOrder, (LPARAM)CompareFunc);

	i = SendMessage (hWndList, LVM_GETITEMCOUNT, 0, 0);
	SendMessage (hWndList, LVM_REDRAWITEMS, 0, i);
}

void UpdatePlayerList (VOID)
{
	LVITEM lvI;
	LVITEM pitem;
	int index;
	int i;

	SendMessage (hWndPlayerList, LVM_DELETEALLITEMS, 0, 0);

	//if (info->iItem == -1)
	//	return;

	i = SendMessage (hWndList, LVM_GETNEXTITEM, -1, LVNI_ALL | LVNI_SELECTED);
	if (i == -1)
		return;

	memset (&pitem, 0, sizeof(pitem));

	pitem.iItem = i;
	pitem.iSubItem = 0;
	pitem.mask = LVIF_PARAM;

	SendMessage (hWndList, LVM_GETITEM, 0, (LPARAM)(LPLVITEM)&pitem);
	
	index = pitem.lParam;

	memset (&lvI, 0, sizeof(lvI));

	lvI.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;

	lvI.state = 0; 
	lvI.stateMask = 0;

	// Initialize LVITEM members that are different for each item. 
	//for (index = 0; index < 3; index++)	{
	for (i = 0; i < servers[index].curClients; i++)
	{
   		lvI.iItem = i;
		lvI.iImage = 0;
		lvI.iSubItem = 0;
		lvI.lParam = (LPARAM)&servers[index].players[i];
		lvI.pszText = LPSTR_TEXTCALLBACK;

		SendMessage (hWndPlayerList, LVM_INSERTITEM, 0, (LPARAM) (const LPLVITEM) &lvI);
	}

	//SendMessage (hWndInfoList, LVM_INSERTITEM, 0, (LPARAM) (const LPLVITEM) &lvI);
}

LRESULT ResizeWindow (WPARAM wParam, LPARAM lParam)
{
	int		newWidth;
	int		newHeight;
	int		index;

	float	widthScale, heightScale;
    
	int		width[3];

	newWidth = LOWORD(lParam);
	newHeight = HIWORD(lParam);

	widthScale = newWidth / 686.0f;
	heightScale = newHeight / 419.0f;

	// Add the columns.
	for (index = 0; index < sizeof(columns) / sizeof(columns[0]); index++)
	{
		int	cx = (int)(((659 * widthScale)-20) * (columns[index].relWidth / 100.0f));
		ListView_SetColumnWidth (hWndList, index, cx);
	}

	if (listIsDetailed)
	{
		width[0] = 50;
		width[1] = 25;
		width[2] = 25;
		for (index = 0; index < 3; index++)
		{
			int cx = (int)(((296 * widthScale)-20) * (width[index] / 100.0f));
			ListView_SetColumnWidth (hWndPlayerList, index, cx);
		}
	}
	else
	{
		int cx = (int)(((296 * widthScale)-20) * 0.33f);
		ListView_SetColumnWidth (hWndPlayerList, 0, cx);
	}

	return FALSE;
}

VOID UpdateServers (VOID)
{
	if (scanInProgress)
	{
		SetEvent (hTerminateScanEvent);
        EnableWindow (GetDlgItem (hwndMain, IDC_UPDATE), FALSE);
		return;
	}
	SetDlgItemText (hwndMain, IDC_UPDATE, _T("Abort"));
	EnableWindow (GetDlgItem (hwndMain, IDC_CONFIG), FALSE);
	EnableWindow (GetDlgItem (hwndMain, IDC_MOD), FALSE);
	scanInProgress = TRUE;
	PingHandle = (HANDLE)_beginthreadex (NULL, 0, (LPTHREAD_START_ROUTINE)GetServerList, 0, 0, &PingThreadID);
	if (PingHandle)
		CloseHandle (PingHandle);
}

LRESULT HandleMinMax (WPARAM wParam, LPARAM lParam)
{
	MINMAXINFO	*info;

	info = (PMINMAXINFO)lParam;

	info->ptMinTrackSize.x = 686 + 8;
	info->ptMinTrackSize.y = 419 + 28;

	return FALSE;
}

VOID ParseBuddyList (VOID)
{
	_TCHAR	*buddies;
	_TCHAR	*p;
	INT		x;
	_TCHAR	buddy[32];
	SIZE_T		len;

	numBuddies = 0;

	if (!q2Buddies[0])
		return;

	x = 0;

	p = q2Buddies;
	while ((p = _tcschr (p, '\n')))
	{
		x++;
		p++;
	}

	x += 2;

	if (buddyList)
		HeapFree (GetProcessHeap(), 0, buddyList);

	buddyList = HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, x * sizeof(buddy));

	len = (_tcslen (q2Buddies) * sizeof(_TCHAR)) + sizeof(_TCHAR);
	buddies = HeapAlloc (GetProcessHeap(), 0, len);
	StringCbCopy (buddies, len, q2Buddies);

	p = buddies;
	x = 0;
	for (;;)
	{
		if (p[0] == '\r' || p[0] == '\n' || p[0] == '\0')
		{
			if (x)
			{
				buddy[x] = '\0';
				StringCbCopy (buddyList[numBuddies++].name, sizeof (buddyList[0].name), buddy);
				x = 0;
			}
		}
		else
		{
			if (x < tsizeof(buddy)-1)
				buddy[x++] = p[0];
			else
				buddy[tsizeof(buddy)-1] = 0;
		}

		if (p[0] == '\0')
			break;

		p++;
	}

	HeapFree (GetProcessHeap(), 0, buddies);
}

VOID InitMainDialog (HWND hWnd)
{
	int				version;
	int				i = 0;
	HKEY			hk;
	int size = sizeof(DWORD);
	WSADATA			ws;
	int				error;
	OSVERSIONINFO	osvi;

	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx (&osvi);

	InitializeCriticalSection (&critModList);

	hTerminateScanEvent = CreateEvent (NULL, TRUE, FALSE, NULL);
	if (hTerminateScanEvent == INVALID_HANDLE_VALUE)
	{
		PostQuitMessage (1);
		return;
	}

	g_isXP = ( (osvi.dwMajorVersion > 5) || ( (osvi.dwMajorVersion == 5) && (osvi.dwMinorVersion >= 1) ));

	error = WSAStartup ((WORD)MAKEWORD (2,2), &ws);
	if (error)
	{
		MessageBox (hwndMain, _T("Couldn't load Winsock!"), _T("Error"), MB_OK);
		PostQuitMessage (1);
		return;
	}

	if (LOBYTE(ws.wVersion) != 2 || HIBYTE(ws.wHighVersion) != 2)
	{
		MessageBox(hwndMain, _T("Could not find a usable version of Winsock.dll"), _T("Error"), MB_OK);
		WSACleanup();
		PostQuitMessage(1);
	}

	numServers = 0;

	hWndList = LV_CreateListView (hwndMain, hThisInstance, 5, NULL);

	{
		DIALOG_SIZER_START( sz )
			DIALOG_SIZER_ENTRY( IDC_CONFIG, DS_MoveX | DS_MoveY )
			DIALOG_SIZER_ENTRY( IDC_ABOUT, DS_MoveX | DS_MoveY )
			DIALOG_SIZER_ENTRY( IDC_UPDATE, DS_MoveX | DS_MoveY )
			DIALOG_SIZER_ENTRY( IDC_EXIT, DS_MoveX | DS_MoveY )
			DIALOG_SIZER_ENTRY( IDC_SERVERLIST, DS_SizeX | DS_SizeY )
			DIALOG_SIZER_ENTRY( IDC_PLAYERS, DS_SizeX | DS_MoveY )
			DIALOG_SIZER_ENTRY( IDC_SERVERFRAME, DS_SizeY | DS_SizeX )
			DIALOG_SIZER_ENTRY( IDC_PLAYERSFRAME, DS_SizeX | DS_MoveY )
			DIALOG_SIZER_ENTRY( IDC_CONTROLSFRAME, DS_MoveX | DS_MoveY )
			DIALOG_SIZER_ENTRY( IDC_STATUSBAR, DS_MoveX | DS_MoveY | DS_SizeX )
			DIALOG_SIZER_ENTRY( IDC_INCLUDE_EMPTY, DS_MoveX | DS_MoveY )
			DIALOG_SIZER_ENTRY( IDC_INCLUDE_FULL, DS_MoveX | DS_MoveY )
			DIALOG_SIZER_ENTRY( IDC_FILTERGROUP, DS_MoveX | DS_MoveY )
			DIALOG_SIZER_ENTRY( IDC_MOD, DS_MoveX | DS_MoveY )
			DIALOG_SIZER_ENTRY( IDC_LABELMOD, DS_MoveX | DS_MoveY )
			DIALOG_SIZER_ENTRY( IDC_FILTERGROUP, DS_MoveX | DS_MoveY )
		DIALOG_SIZER_END()
		DialogSizer_Set( hWnd, sz, TRUE, NULL );
	}

	if (!(RegOpenKeyEx(HKEY_CURRENT_USER, _T("SOFTWARE\\r1ch.net\\") APP_NAME, 0, KEY_READ, &hk)))
	{
		WINDOWPLACEMENT	winPlacement;

		size = sizeof(DWORD);
		RegQueryValueEx(hk, _T("Packets Per Second"), 0, NULL, (LPBYTE)&q2PacketsPerSecond, (LPDWORD)&size);

		size = sizeof(DWORD);
		RegQueryValueEx(hk, _T("Player View Style"), 0, NULL, (LPBYTE)&listIsDetailed, (LPDWORD)&size);

		size = sizeof(DWORD);
		RegQueryValueEx(hk, _T("Good Ping Threshold"), 0, NULL, (LPBYTE)&q2GoodPing, (LPDWORD)&size);

		size = sizeof(DWORD);
		RegQueryValueEx(hk, _T("Medium Ping Threshold"), 0, NULL, (LPBYTE)&q2MediumPing, (LPDWORD)&size);

		size = sizeof(DWORD);
		RegQueryValueEx(hk, _T("Show Empty Servers"), 0, NULL, (LPBYTE)&show_empty, (LPDWORD)&size);
		if (show_empty)
			SendDlgItemMessage (hwndMain, IDC_INCLUDE_EMPTY, BM_SETCHECK, 1, 0);

		size = sizeof(DWORD);
		RegQueryValueEx(hk, _T("Show Full Servers"), 0, NULL, (LPBYTE)&show_full, (LPDWORD)&size);
		if (show_full)
			SendDlgItemMessage (hwndMain, IDC_INCLUDE_FULL, BM_SETCHECK, 1, 0);

		size = sizeof(DWORD);
		if (RegQueryValueEx (hk, _T("Version"), 0, NULL, (LPBYTE)&version, (LPDWORD)&size) != ERROR_SUCCESS)
			version = 1;

		size = sizeof(DWORD);
		RegQueryValueEx (hk, _T("Sort Order"), 0, NULL, (LPBYTE)&lastSortOrder, (LPDWORD)&size);

		size = sizeof (q2Path);
		RegQueryValueEx(hk, GAME_NAME _T(" Directory"), 0, NULL, (LPBYTE)q2Path, (LPDWORD)&size);

		size = sizeof (q2Exe);
		RegQueryValueEx(hk, GAME_NAME _T(" Executable"), 0, NULL, (LPBYTE)q2Exe, (LPDWORD)&size);

		size = sizeof (winPlacement);
		if (RegQueryValueEx (hk, _T("Window Position"), 0, NULL, (LPBYTE)&winPlacement, (LPDWORD)&size) == ERROR_SUCCESS)
		{
			if (!(GetWindowLongPtr (hwndMain, GWL_STYLE) & WS_VISIBLE))
				winPlacement.showCmd = SW_HIDE;
			SetWindowPlacement (hwndMain, &winPlacement);
		}

		size = 0;
		if (RegQueryValueEx (hk, _T("Buddy List"), 0, NULL, NULL, (LPDWORD)&size) != ERROR_SUCCESS)
			size = 1;

		q2Buddies = HeapAlloc (GetProcessHeap(), 0, size);
		if (RegQueryValueEx (hk, _T("Buddy List"), 0, NULL, (LPBYTE)q2Buddies, (LPDWORD)&size) != ERROR_SUCCESS)
			q2Buddies[0] = 0;

		RegCloseKey(hk);
	}
	else
	{
		show_full = show_empty = 1;
		SendDlgItemMessage (hwndMain, IDC_INCLUDE_EMPTY, BM_SETCHECK, 1, 0);
		SendDlgItemMessage (hwndMain, IDC_INCLUDE_FULL, BM_SETCHECK, 1, 0);
	}

	if (!q2Buddies)
	{
		q2Buddies = HeapAlloc (GetProcessHeap(), 0, sizeof(_TCHAR));
		q2Buddies[0] = '\0';
	}
	else if (version == 1)
	{
#ifdef _UNICODE
		char	*p;
		p = (char *)_tcsdup (q2Buddies);
		if (!p)
		{
			return;
		}
		HeapFree (GetProcessHeap(), 0, q2Buddies);
		q2Buddies = HeapAlloc (GetProcessHeap(), 0, size * sizeof(_TCHAR));
		MultiByteToWideChar (CP_ACP, 0, p, -1, q2Buddies, size);
		free (p);
#endif
	}

	ParseBuddyList ();

	if (!q2Path[0])
	{
		if (!(RegOpenKeyEx(HKEY_LOCAL_MACHINE, GAME_REGPATH, 0, KEY_READ, &hk)))
		{
			size = sizeof (q2Path);
			RegQueryValueEx(hk, GAME_REGKEY, 0, NULL, (LPBYTE)q2Path, (LPDWORD)&size);
			RegCloseKey (hk);
		}
	}
		
	if (!q2Exe[0])
		StringCbCopy (q2Exe, sizeof(q2Exe), DEFAULT_EXECUTABLE_NAME);

	if (!q2PacketsPerSecond)
		q2PacketsPerSecond = 10;

	if (!q2GoodPing)
		q2GoodPing = 100;

	if (!q2MediumPing)
		q2MediumPing = 200;

	if (listIsDetailed && g_isXP)
		SendMessage (hWndPlayerList, LVM_SETVIEW, LV_VIEW_DETAILS, 0);

	SetWindowText (hwndMain, APP_NAME);

	WndInitNotifyIconData();

	ShowWindow (hwndMain, SW_SHOWNORMAL);
	UpdateWindow (hwndMain);
	UpdateServers ();
}

VOID ShowServerContextMenu (LPNMITEMACTIVATE ev)
{
	HMENU			menu;
	int				i;

	if (ev->iItem == -1)
		return;

	menu = CreatePopupMenu ();

	/*ZeroMemory (&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_STRING | MIIM_ID;
	item.fType = MIIM_STRING;
	item.dwItemData = _T("Connect");
	item.wId = 1;*/

	AppendMenu (menu, MF_STRING, 1, _T("Connect"));
	AppendMenu (menu, MF_STRING, 2, _T("Copy Server IP"));
	AppendMenu (menu, MF_STRING, 3, _T("Server Info"));

	ClientToScreen (hwndMain, &ev->ptAction); 

    i = TrackPopupMenuEx (menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, ev->ptAction.x, ev->ptAction.y, hwndMain, NULL); 

	if (i == 1)
		RunQ2 (ev);
	else if (i == 2)
	{
		struct	in_addr tmp;	
		DWORD	index;
		LVITEM	pitem;
		SIZE_T	cch;
		LPTSTR  lptstrCopy; 
		HGLOBAL hglbCopy;
		_TCHAR	serverIP[64];

		if (!OpenClipboard(hwndMain))
			goto abortMenu; 

		EmptyClipboard (); 

		memset (&pitem, 0, sizeof(pitem));

		pitem.iItem = ev->iItem;
		pitem.iSubItem = 0;
		pitem.mask = LVIF_PARAM;

		SendMessage (hWndList, LVM_GETITEM, 0, (LPARAM)(LPLVITEM)&pitem);

		index = pitem.lParam;

		tmp.S_un.S_addr = servers[index].ip;
#ifdef _UNICODE
		StringCbPrintf (serverIP, sizeof(serverIP), _T("%S:%d"), inet_ntoa(tmp), servers[index].port); 
#else
		StringCbPrintf (serverIP, sizeof(serverIP), _T("%s:%d"), inet_ntoa(tmp), servers[index].port); 
#endif

		cch = _tcslen (serverIP);

		hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (cch + 1) * sizeof(TCHAR)); 
		if (hglbCopy == NULL) 
		{ 
			CloseClipboard(); 
			goto abortMenu; 
		} 

		lptstrCopy = GlobalLock(hglbCopy); 
		memcpy (lptstrCopy, serverIP, cch * sizeof(TCHAR)); 
		lptstrCopy[cch] = (TCHAR) 0;
		GlobalUnlock(hglbCopy); 

		// Place the handle on the clipboard.
#ifdef _UNICODE
		SetClipboardData (CF_UNICODETEXT, hglbCopy);
#else
		SetClipboardData (CF_TEXT, hglbCopy);
#endif

		CloseClipboard ();
	}
	else if (i == 3)
	{
		DWORD	index;
		LVITEM	pitem;

		memset (&pitem, 0, sizeof(pitem));

		pitem.iItem = ev->iItem;
		pitem.iSubItem = 0;
		pitem.mask = LVIF_PARAM;

		SendMessage (hWndList, LVM_GETITEM, 0, (LPARAM)(LPLVITEM)&pitem);

		index = pitem.lParam;

		/* FS: TODO: Format this in a way that's not crap. */
		MessageBox(hwndMain, servers[index].infostrings, _T("Server Info"), MB_OK);
	}

abortMenu:

	DestroyMenu (menu);
}

BOOL BuddyIsOnServer (SERVERINFO *server)
{
	BOOL	ret;
	int		i, x;

	if (!numBuddies)
		return FALSE;

	ret = FALSE;

	for (i = 0; i < server->curClients; i++)
	{
		for (x = 0; x < numBuddies; x++)
		{
			if (!_tcsicmp (server->players[i].playername, buddyList[x].name) ||
				_tcsstr (server->players[i].playername, buddyList[x].name))
			{
				ret = TRUE;
				goto finished;
			}
		}
	}

finished:

	return ret;
}

BOOL BuddyIsPlayer (PLAYERINFO *player)
{
	BOOL	ret;
	int		x;

	if (!numBuddies)
		return FALSE;

	ret = FALSE;

	for (x = 0; x < numBuddies; x++)
	{
		if (!_tcsicmp (player->playername, buddyList[x].name) || 
			_tcsstr (player->playername, buddyList[x].name))
		{
			ret = TRUE;
			break;
		}
	}

	return ret;
}

LRESULT CustomDrawHandler (HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	NMHDR *p = (NMHDR *)lParam;
	LRESULT	pResult;

	pResult = FALSE;

	if (p->code == NM_CUSTOMDRAW)
	{
		NMLVCUSTOMDRAW *lvcd = (NMLVCUSTOMDRAW *)p;
		NMCUSTOMDRAW   *nmcd = &lvcd->nmcd;

		switch (nmcd->dwDrawStage)
		{
		case CDDS_PREPAINT:

			// We want item prepaint notifications, so...
			pResult = CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYSUBITEMDRAW;
			break;

		case CDDS_ITEMPREPAINT:
		case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
			{
				int iRow = (int)nmcd->dwItemSpec;

				/*bHighlighted = IsRowHighlighted(g_hListView, iRow);
				if (bHighlighted)
				{
					lvcd->clrText   = g_MyClrFgHi; // Use my foreground hilite color
					lvcd->clrTextBk = g_MyClrBgHi; // Use my background hilite color

					// Turn off listview highlight otherwise it uses the system colors!
					EnableHighlighting(g_hListView, iRow, false);
				}*/

				pResult = CDRF_DODEFAULT | CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYSUBITEMDRAW;

				if (hWnd == hWndList)
				{
					SERVERINFO	*server;
					LVITEM		item;
					BOOL		buddy;

					memset (&item, 0, sizeof(item));
					item.iItem = iRow;
					item.iSubItem = 0;
					item.mask = LVIF_PARAM;

					SendMessage (hWndList, LVM_GETITEM, 0, (LPARAM)&item);

					server = &servers[item.lParam];

					buddy = BuddyIsOnServer (server);

					if (buddy)
					{
						lvcd->clrTextBk = RGB(255, 255, 0);
					}

					/*if (lvcd->iSubItem == 1)
					{
						DWORD		i;
						_TCHAR		szText[64];
						IN_ADDR	tmp;
						HIMAGELIST	list;
						RECT		rc, rc2;
						HBRUSH		bg = NULL;

						if (nmcd->uItemState & CDIS_FOCUS)
							SetTextColor (nmcd->hdc, GetSysColor (COLOR_HIGHLIGHTTEXT));
						else
							SetTextColor (nmcd->hdc, GetSysColor (COLOR_BTNTEXT));

						if (nmcd->uItemState & CDIS_FOCUS)
							bg = (HBRUSH)(COLOR_HIGHLIGHT+1);
						else if (buddy)
							bg = CreateSolidBrush (RGB(255,255,0));
						else if (SendMessage (hWndList, LVM_GETNEXTITEM, -1, LVNI_ALL | LVNI_SELECTED) == iRow)
							bg = (HBRUSH)(COLOR_INACTIVEBORDER+1);

						ListView_GetSubItemRect (hWndList, iRow, 1, LVIR_BOUNDS, &rc2);

						if (bg)
						{
							FillRect (nmcd->hdc, &rc2, bg);
							DeleteObject (bg);
						}
						else
							FillRect (nmcd->hdc, &rc2, (HBRUSH)(COLOR_WINDOW+1));

						ListView_GetSubItemRect (hWndList, iRow, 1, LVIR_ICON, &rc);
						ImageList_DrawEx (hCountry, server->cID, nmcd->hdc, rc.left + 1, rc.top + 1, 18, 12, CLR_NONE, CLR_NONE, ILD_NORMAL);
						pResult |= CDRF_DOERASE | CDRF_NOTIFYPOSTPAINT;

						tmp.S_un.S_addr = server->ip;
						StringCchPrintf (szText, sizeof(szText), _T("aa%S:%d"), inet_ntoa (tmp), server->port);

						ListView_GetSubItemRect (hWndList, iRow, 1, LVIR_BOUNDS, &rc2);
						rc2.left += rc.right - rc.left + 8;
						DrawText (nmcd->hdc, szText, _tcslen(szText), &rc2, DT_LEFT);
					}
					else if (lvcd->iSubItem == 6)
					{
						DWORD		i;
						_TCHAR		szText[64];
						int			iImage;
						HIMAGELIST	list;
						RECT		rc, rc2;
						HBRUSH		bg = NULL;

						if (nmcd->uItemState & CDIS_FOCUS)
							SetTextColor (nmcd->hdc, GetSysColor (COLOR_HIGHLIGHTTEXT));
						else
							SetTextColor (nmcd->hdc, GetSysColor (COLOR_BTNTEXT));

						if (nmcd->uItemState & CDIS_FOCUS)
							bg = (HBRUSH)(COLOR_HIGHLIGHT+1);
						else if (buddy)
							bg = CreateSolidBrush (RGB(255,255,0));
						else if (SendMessage (hWndList, LVM_GETNEXTITEM, -1, LVNI_ALL | LVNI_SELECTED) == iRow)
							bg = (HBRUSH)(COLOR_INACTIVEBORDER+1);

						ListView_GetSubItemRect (hWndList, iRow, 6, LVIR_BOUNDS, &rc2);

						if (bg)
						{
							FillRect (nmcd->hdc, &rc2, bg);
							DeleteObject (bg);
						}
						else
							FillRect (nmcd->hdc, &rc2, (HBRUSH)(COLOR_WINDOW+1));

						ListView_GetSubItemRect (hWndList, iRow, 6, LVIR_ICON, &rc);
						list = ListView_GetImageList (hWndList, LVSIL_SMALL);

						if (server->ping <= q2GoodPing)
							iImage = 1;
						else if (server->ping <= q2MediumPing)
							iImage = 2;
						else
							iImage = 3;

						ImageList_DrawEx (list, iImage, nmcd->hdc, rc.left, rc.top, 16, 14, CLR_NONE, CLR_NONE, ILD_NORMAL);
						pResult |= CDRF_SKIPDEFAULT ;

						StringCchPrintf (szText, sizeof(szText), _T("%d"), server->ping);

						ListView_GetSubItemRect (hWndList, iRow, 6, LVIR_BOUNDS, &rc2);
						rc2.left += rc.right - rc.left + 6;
						DrawText (nmcd->hdc, szText, _tcslen(szText), &rc2, DT_LEFT);
					}*/
				}
				else if (hWnd == hWndPlayerList)
				{
					LVITEM		item;
					PLAYERINFO	*player;

					memset (&item, 0, sizeof(item));
					item.iItem = iRow;
					item.iSubItem = 0;
					item.mask = LVIF_PARAM;

					SendMessage (hWndPlayerList, LVM_GETITEM, 0, (LPARAM)&item);

					player = (PLAYERINFO *)item.lParam;

					if (BuddyIsPlayer (player))
					{
						lvcd->clrTextBk = RGB(255, 255, 0);
					}
				}


				break;
			}

		case CDDS_ITEMPOSTPAINT:
		case CDDS_ITEMPOSTPAINT | CDDS_SUBITEM:
			{
				int iRow = (int)nmcd->dwItemSpec;

				/*bHighlighted = IsRowHighlighted(g_hListView, iRow);
				if (bHighlighted)
				{
					lvcd->clrText   = g_MyClrFgHi; // Use my foreground hilite color
					lvcd->clrTextBk = g_MyClrBgHi; // Use my background hilite color

					// Turn off listview highlight otherwise it uses the system colors!
					EnableHighlighting(g_hListView, iRow, false);
				}*/

				pResult = CDRF_DODEFAULT | CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYSUBITEMDRAW;

				if (hWnd == hWndList)
				{
					SERVERINFO	*server;
					LVITEM		item;
					BOOL		buddy;

					memset (&item, 0, sizeof(item));
					item.iItem = iRow;
					item.iSubItem = 0;
					item.mask = LVIF_PARAM;

					SendMessage (hWndList, LVM_GETITEM, 0, (LPARAM)&item);

					server = &servers[item.lParam];

					buddy = BuddyIsOnServer (server);

					if (buddy)
					{
						lvcd->clrTextBk = RGB(255, 255, 0);
					}
#if 0
					if (lvcd->iSubItem == 1)
					{
						_TCHAR		szText[64];
						IN_ADDR	tmp;
						RECT		rc, rc2;
						HBRUSH		bg = NULL;

						/*if (nmcd->uItemState & CDIS_FOCUS)
							bg = (HBRUSH)(COLOR_HIGHLIGHT+1);
						else if (buddy)
							bg = CreateSolidBrush (RGB(255,255,0));
						else if (SendMessage (hWndList, LVM_GETNEXTITEM, -1, LVNI_ALL | LVNI_SELECTED) == iRow)
							bg = (HBRUSH)(COLOR_INACTIVEBORDER+1);

						ListView_GetSubItemRect (hWndList, iRow, 1, LVIR_BOUNDS, &rc2);*/

					/*	if (bg)
						{
							FillRect (nmcd->hdc, &rc2, bg);
							DeleteObject (bg);
						}
						else
							FillRect (nmcd->hdc, &rc2, (HBRUSH)(COLOR_WINDOW+1));*/

						ListView_GetSubItemRect (hWndList, iRow, 1, LVIR_ICON, &rc);
//						ImageList_DrawEx (hCountry, server->cID, nmcd->hdc, rc.left + 1, rc.top + 1, 18, 12, CLR_NONE, CLR_NONE, ILD_NORMAL);
						pResult |= CDRF_SKIPDEFAULT;

						tmp.S_un.S_addr = server->ip;
#ifdef _UNICODE
						StringCchPrintf (szText, sizeof(szText), _T("%S:%d"), inet_ntoa (tmp), server->port);
#else
						StringCchPrintf (szText, sizeof(szText), _T("%s:%d"), inet_ntoa (tmp), server->port);
#endif

						ListView_GetSubItemRect (hWndList, iRow, 1, LVIR_BOUNDS, &rc2);
						rc2.left += rc.right - rc.left - 12;

						if (nmcd->uItemState & CDIS_FOCUS)
							SetTextColor (nmcd->hdc, GetSysColor (COLOR_HIGHLIGHTTEXT));
						else
							SetTextColor (nmcd->hdc, GetSysColor (COLOR_BTNTEXT));

						DrawText (nmcd->hdc, szText, _tcslen(szText), &rc2, DT_LEFT);
					}
					else if (lvcd->iSubItem == 6)
					{
						_TCHAR		szText[16];
						RECT		rc, rc2;
						int			iImage;

						if (server->ping <= q2GoodPing)
							iImage = 1;
						else if (server->ping <= q2MediumPing)
							iImage = 2;
						else
							iImage = 3;
						ListView_GetSubItemRect (hWndList, iRow, 6, LVIR_ICON, &rc);
						ImageList_DrawEx (ListView_GetImageList (hWndList, LVSIL_SMALL), iImage, nmcd->hdc, rc.left, rc.top, 0, 0, CLR_NONE, CLR_NONE, ILD_NORMAL);
						pResult |= CDRF_SKIPDEFAULT;

						StringCchPrintf (szText, sizeof(szText), _T("%d"), server->ping);

						ListView_GetSubItemRect (hWndList, iRow, 6, LVIR_BOUNDS, &rc2);
						rc2.left += rc.right - rc.left + 4;

						if (nmcd->uItemState & CDIS_FOCUS)
							SetTextColor (nmcd->hdc, GetSysColor (COLOR_HIGHLIGHTTEXT));
						else
							SetTextColor (nmcd->hdc, GetSysColor (COLOR_BTNTEXT));

						DrawText (nmcd->hdc, szText, _tcslen(szText), &rc2, DT_LEFT);
					}
#endif
				}
				/*if (bHighlighted)
				{
					int  iRow = (int)nmcd.dwItemSpec;

					// Turn listview control's highlighting back on now that we have
					// drawn the row in the colors we want.
					EnableHighlighting(g_hListView, iRow, true);
				}*/

				//pResult = CDRF_DODEFAULT;
				break;
			}

		default:
			pResult = CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYSUBITEMDRAW;
			break;
		}
	}

	return pResult;
}

VOID SortServerListView (NMLVDISPINFO *info)
{
	int	i;

	i = info->item.iItem;

	if (i == lastSortOrder)
		i |= 0x4000;

	SendMessage (hWndList, LVM_SORTITEMS, (WPARAM)i, (LPARAM)CompareFunc);
	lastSortOrder = i;
}

VOID FillServerListView (NMLVDISPINFO *info)
{
	int					index;
	const column_t		*column;
	const SERVERINFO	*server;

	index = info->item.iSubItem;
	if (index >= sizeof(columns) / sizeof(columns[0]))
		return;

	column = &columns[index];

	if (info->item.lParam >= numServers)
		return;

	server = &servers[info->item.lParam];
	
	switch (column->mapping)
	{
		case INFO_HOSTNAME:
			StringCchCopy (info->item.pszText, info->item.cchTextMax, server->szHostName);
			break;

		case INFO_PING:
		{
#if 1
			info->item.mask |= LVIF_IMAGE;
			if (server->ping <= q2GoodPing)
				info->item.iImage = 1;
			else if (server->ping <= q2MediumPing)
				info->item.iImage = 2;
			else
				info->item.iImage = 3;

			info->item.pszText[0] = _T('\0');
			StringCchPrintf (info->item.pszText, info->item.cchTextMax, _T("  %d"), server->ping);
#else
			info->item.pszText[0] = _T('\0');
#endif
		}
			break;
		case INFO_IPADDRESS:
		{
#if 1
			IN_ADDR tmp;

			//info->item.mask |= LVIF_IMAGE;
			//info->item.iImage = 7 + server->cID;

			tmp.S_un.S_addr = server->ip;

			info->item.pszText[0] = _T('\0');
#ifdef _UNICODE
			StringCchPrintf (info->item.pszText, info->item.cchTextMax, _T("%S:%d"), inet_ntoa (tmp), server->port);
#else
			StringCchPrintf (info->item.pszText, info->item.cchTextMax, _T("%s:%d"), inet_ntoa (tmp), server->port);
#endif // _UNICODE
#else
			info->item.pszText[0] = _T('\0');
#endif // 1
		}
			break;

		case INFO_MAP_NAME:
			StringCchCopy (info->item.pszText, info->item.cchTextMax, server->szMapName);
			break;

		case INFO_PLAYERS:
			//_stprintf (buff, _T("%d / %d"), server->curClients, server->maxClients - server->reservedSlots);
			//_tcscpy (info->item.pszText, buff);
			StringCchPrintf (info->item.pszText, info->item.cchTextMax, _T("%d / %d"), server->curClients, server->maxClients - server->reservedSlots);
			break;

		case INFO_GAMEDATE:
			StringCchCopy (info->item.pszText, info->item.cchTextMax, server->szGameDate);
			break;

		case INFO_TIMELIMIT:
			StringCchPrintf (info->item.pszText, info->item.cchTextMax, _T("%d"), server->timelimit);
			//_tcscpy (info->item.pszText, buff);
			break;

		case INFO_GAMENAME:
			StringCchCopy (info->item.pszText, info->item.cchTextMax, server->szGameName);
			break;

		case INFO_GAMEMODE:
			StringCchCopy (info->item.pszText, info->item.cchTextMax, server->szGameMode);
			break;

		case INFO_VERSION:
			StringCchCopy (info->item.pszText, info->item.cchTextMax, server->szVersion);
			break;

		default:
			info->item.pszText[0] = '\0';
			break;
	}
}

static void WndInitNotifyIconData (void)
{
	notifyIconData = (PNOTIFYICONDATAW)calloc(1, sizeof(NOTIFYICONDATAW));
	memset (notifyIconData, 0, sizeof(*notifyIconData));
	notifyIconData->cbSize = sizeof(NOTIFYICONDATAW);
	notifyIconData->hWnd = hwndMain;
	notifyIconData->uID = ID_TRAY_ICON;
	notifyIconData->uFlags = (NIF_ICON | NIF_MESSAGE | NIF_TIP);
	notifyIconData->uCallbackMessage = WM_TRAYICON;
	notifyIconData->hIcon = LoadIcon(hThisInstance, MAKEINTRESOURCE(IDI_COMP));
	_tcscpy(notifyIconData->szTip, TEXT("Daikatana Server Browser"));
}

static void WndMinToTray (BOOL minimize)
{
	if (minimize)
	{
		Shell_NotifyIcon(NIM_ADD, notifyIconData);
		ShowWindow (hwndMain, SW_HIDE);
	}
	else
	{
		Shell_NotifyIcon(NIM_DELETE, notifyIconData);
		ShowWindow(hwndMain, SW_SHOW);
	}
}

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProcMain(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int		wmId, wmEvent;

	switch (message)
	{
		case WM_CREATE:
			return TRUE;
		case WM_INITDIALOG:
			hwndMain = hWnd;
			InitMainDialog (hWnd);
			hTrayMenu = CreatePopupMenu();
			AppendMenu (hTrayMenu, MF_STRING, ID_TRAY_RESTORE_CONTEXT_MENU_ITEM, TEXT("Restore"));
			AppendMenu (hTrayMenu, MF_STRING, ID_TRAY_ABOUT_CONTEXT_MENU_ITEM, TEXT("About"));
			AppendMenu (hTrayMenu, MF_SEPARATOR, 0, 0);
			AppendMenu (hTrayMenu, MF_STRING, ID_TRAY_QUIT_CONTEXT_MENU_ITEM, TEXT("Quit"));
			return TRUE;
		case WM_SIZE:
			return ResizeWindow (wParam, lParam);
		case WM_GETMINMAXINFO:
			return HandleMinMax (wParam, lParam);
		case WM_SYSCOMMAND:
			switch (wParam & 0xfff0)
			{
				case SC_MINIMIZE:
					WndMinToTray (TRUE);
					return TRUE;
			}
			break;
		case WM_TRAYICON:
			if (lParam == WM_LBUTTONDBLCLK)
			{
				WndMinToTray (FALSE);
			}
			else if (lParam == WM_RBUTTONUP)
			{
				UINT	itemClicked;
				POINT	cursorPoint;
				GetCursorPos (&cursorPoint);
				SetForegroundWindow(hwndMain);
				itemClicked = TrackPopupMenu (hTrayMenu, TPM_RETURNCMD | TPM_NONOTIFY, cursorPoint.x, cursorPoint.y, 0, hwndMain, NULL);
				if (itemClicked == ID_TRAY_RESTORE_CONTEXT_MENU_ITEM)
				{
					WndMinToTray (FALSE);
				}
				else if (itemClicked == ID_TRAY_QUIT_CONTEXT_MENU_ITEM)
				{
					SaveStuff();
					PostQuitMessage(0);
				}
				else if (itemClicked == ID_TRAY_ABOUT_CONTEXT_MENU_ITEM)
				{
					DialogBox(hThisInstance, (LPCTSTR)IDD_ABOUTBOX, hwndMain, (DLGPROC)About);
				}
			}
			break;
		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			 
			// Parse the menu selections:
			switch (wmId)
			{
				case IDC_UPDATE:
					UpdateServers ();
					break;
				case IDC_CONFIG:
					DialogBox(hThisInstance, (LPCTSTR)IDD_GSB_PROXY, hWnd, (DLGPROC)Proxy);
					break;
				case IDC_ABOUT:
					DialogBox(hThisInstance, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
					break;
				case IDC_EXIT:
					DestroyWindow (hwndMain);
					break;
				case IDC_MOD:
					if (wmEvent == CBN_SELCHANGE)
						UpdateServers ();
					break;
				case IDC_INCLUDE_EMPTY:
				case IDC_INCLUDE_FULL:
					if (wmEvent == BN_CLICKED)
						UpdateServerList ();
					break;
				default:
					return FALSE;
			}
			break;
		/*case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			// TODO: Add any drawing code here...
			EndPaint(hWnd, &ps);
			break;*/
		case WM_CLOSE:
			DestroyWindow (hwndMain);
			return TRUE;
		case WM_DESTROY:
			SaveStuff();
			PostQuitMessage(0);
			break;
		case WM_NOTIFY:
			// Parse the menu selections:
			switch (wParam)
			{
				case IDC_SERVERLIST:
					switch (((LPNMHDR)lParam)->code)
					{
						case NM_CUSTOMDRAW:
							SetWindowLongPtr (hWnd, DWLP_MSGRESULT, CustomDrawHandler (hWndList, wParam, lParam));
							return TRUE;
						case LVN_ITEMCHANGED:
							UpdatePlayerList ();
							break;
						case NM_DBLCLK:
							RunQ2 ((LPNMITEMACTIVATE)lParam);
							break;
						case NM_RCLICK:
							ShowServerContextMenu ((LPNMITEMACTIVATE)lParam);
							break;
						case LVN_COLUMNCLICK:
							SortServerListView ((NMLVDISPINFO *)lParam);
							break;
						case LVN_GETDISPINFO:
							FillServerListView ((NMLVDISPINFO *)lParam);
							break;
					}
					break;
				case IDC_PLAYERS:
						switch (((LPNMHDR) lParam)->code)
						{
							case NM_CUSTOMDRAW:
								SetWindowLongPtr (hWnd, DWLP_MSGRESULT, CustomDrawHandler (hWndPlayerList, wParam, lParam));
								return TRUE;
							case NM_RCLICK:
								if (g_isXP)
								{
									if (listIsDetailed)
										SendMessage (hWndPlayerList, LVM_SETVIEW, LV_VIEW_SMALLICON, 0);
									else
										SendMessage (hWndPlayerList, LVM_SETVIEW, LV_VIEW_DETAILS, 0);

									listIsDetailed = !listIsDetailed;
								}
								break;
							case LVN_COLUMNCLICK:
								SendMessage (hWndPlayerList, LVM_SORTITEMS, ((LPNMLVDISPINFOW)lParam)->item.iItem, (LPARAM)CompareFunc2);
								/*if (lastSortOrder == ((LPNMLVDISPINFOW)lParam)->item.iItem)
									lastSortOrder = -1;
								else
									lastSortOrder = ((LPNMLVDISPINFOW)lParam)->item.iItem;*/
								break;
							case LVN_GETDISPINFO:
								/*index = SendMessage (hWndList, LVM_GETSELECTIONMARK, 0, 0);
								memset (&pitem, 0, sizeof(pitem));
								pitem.iItem = index;
								pitem.iSubItem = 0;
								pitem.mask = LVIF_PARAM;

								SendMessage (hWndList, LVM_GETITEM, 0, (LPARAM)(LPLVITEM)&pitem);
								index = pitem.lParam;*/
								switch (((NMLVDISPINFO *)lParam)->item.iSubItem)
								{
									case 0:
										//((LPNMLVDISPINFOW)lParam)->item.pszText = (LPWSTR)servers[index].players[((LPNMLVDISPINFOW)lParam)->item.lParam].playername;
										StringCchCopy (((NMLVDISPINFO *)lParam)->item.pszText, ((NMLVDISPINFO *)lParam)->item.cchTextMax, ((PLAYERINFO *)((NMLVDISPINFO *)lParam)->item.lParam)->playername);
										break;
									case 1:
										StringCchPrintf (((NMLVDISPINFO *)lParam)->item.pszText, ((NMLVDISPINFO *)lParam)->item.cchTextMax, _T("%d"), ((PLAYERINFO *)((LPNMLVDISPINFOW)lParam)->item.lParam)->score);
										//((LPNMLVDISPINFOW)lParam)->item.pszText = (LPWSTR)buff;
										//_tcscpy (((NMLVDISPINFO *)lParam)->item.pszText, buff);
										break;
									case 2:
										//_stprintf (buff, _T("%d"), ((PLAYERINFO *)((LPNMLVDISPINFOW)lParam)->item.lParam)->ping);
										//((LPNMLVDISPINFOW)lParam)->item.pszText = (LPWSTR)buff;
										StringCchPrintf (((NMLVDISPINFO *)lParam)->item.pszText, ((NMLVDISPINFO *)lParam)->item.cchTextMax, _T("%d"), ((PLAYERINFO *)((LPNMLVDISPINFOW)lParam)->item.lParam)->ping);
										break;
								}
						}
						break;
			}
			break;

		default:
			return FALSE;
			//return DefDlgProc (hWnd, message, wParam, lParam);
	}
	return FALSE;
}

void GetResultsFromProxyDialog (HWND hDlg)
{
	SIZE_T	size;

	memset (q2Path, 0, sizeof(q2Path));
	memset (q2Exe, 0, sizeof(q2Exe));

	SendDlgItemMessage (hDlg, IDC_Q2PATH, WM_GETTEXT, tsizeof(q2Path)-1, (LPARAM)q2Path);
	SendDlgItemMessage (hDlg, IDC_Q2EXE, WM_GETTEXT, tsizeof(q2Exe)-1, (LPARAM)q2Exe);

	q2PacketsPerSecond = GetDlgItemInt (hDlg, IDC_PPS, NULL, TRUE);

	q2GoodPing = GetDlgItemInt (hDlg, IDC_GOODPING, NULL, TRUE);
	q2MediumPing = GetDlgItemInt (hDlg, IDC_OKPING, NULL, TRUE);

	size = SendDlgItemMessage (hDlg, IDC_BUDDIES, WM_GETTEXTLENGTH, 0, 0);

	if (q2Buddies)
		HeapFree (GetProcessHeap(), 0, q2Buddies);

	q2Buddies = HeapAlloc (GetProcessHeap(), 0, (size + 1) * sizeof(_TCHAR));

	SendDlgItemMessage (hDlg, IDC_BUDDIES, WM_GETTEXT, size + 1, (LPARAM)q2Buddies);
	q2Buddies[size] = 0;

	ParseBuddyList ();

	ListView_RedrawItems (hWndList, 0, ListView_GetItemCount (hWndList));
	ListView_RedrawItems (hWndPlayerList, 0, ListView_GetItemCount (hWndList));

	UpdateWindow (hWndList);
	UpdateWindow (hWndPlayerList);

#ifdef _UNICODE
	if (q2Path[0] && (q2Path[(sizeof(q2Path) / sizeof(*q2Path)) - 1] != '\\'))
#else
	if (q2Path[0] && (q2Path[_tcslen(q2Path)-1] != '\\'))
#endif
		StringCbCat (q2Path, sizeof(q2Path), _T("\\"));
}

LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CLOSE:
			EndDialog(hDlg, 0);
			return TRUE;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
			{
				EndDialog(hDlg, 0);
				return TRUE;
			}
			break;
	}

	return FALSE;
}

// Message handler for about box.
LRESULT CALLBACK Proxy(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		SetDlgItemText (hDlg, IDC_Q2PATH, q2Path);
		SetDlgItemText (hDlg, IDC_Q2EXE, q2Exe);
		SetDlgItemInt (hDlg, IDC_PPS, q2PacketsPerSecond, TRUE);
		SetDlgItemInt (hDlg, IDC_GOODPING, q2GoodPing, TRUE);
		SetDlgItemInt (hDlg, IDC_OKPING, q2MediumPing, TRUE);
		SetDlgItemText (hDlg, IDC_BUDDIES, q2Buddies);
		return TRUE;

	case WM_CLOSE:
		GetResultsFromProxyDialog(hDlg);

		EndDialog(hDlg, 0);
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			GetResultsFromProxyDialog(hDlg);

			EndDialog(hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

int PASCAL WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
					LPSTR lpszCmdLine, int nCmdShow)
{
	MSG						msg;
	HICON					hIcon;
	INITCOMMONCONTROLSEX	common;

	hThisInstance = hInstance;

	common.dwSize = sizeof(common);
	common.dwICC = ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES;

	SetHighDPIMode();

	InitCommonControlsEx (&common);

	InitializeCriticalSection (&critServerList);

	CreateDialog (hThisInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, WndProcMain);

	if (!hwndMain)
		return 1;

	hIcon = (HICON)LoadImage(hThisInstance,
		MAKEINTRESOURCE(IDI_Q2),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		0);

	if(hIcon)
		SendMessage (hwndMain, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

	//PostMessage(hwndMain, UM_INIT2, 0, (LPARAM) lpszCmdLine);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsDialogMessage (hwndMain, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return (int)(msg.wParam);
}

