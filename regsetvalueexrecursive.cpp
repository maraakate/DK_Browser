/*----------------------------------------------------------------------
Copyright (c) 1998 Russell Freeman. All Rights Reserved.
File:	RegSetValueExRecursive.cpp
Web site: http://gipsysoft.com

This software is provided 'as-is', without any express or implied warranty.

In no event will the author be held liable for any damages arising from the
use of this software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject
to the following restrictions:

1) The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a product,
	 an acknowledgment in the product documentation is requested but not required.
2) Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software. Altered source is encouraged
	 to be submitted back to the original author so it can be shared with the
	 community. Please share your changes.
3) This notice may not be removed or altered from any source distribution.

Owner:	russf@gipsysoft.com
Purpose:	Perform a reg set value ex but allow the value name to be a path
					into the registry
----------------------------------------------------------------------*/
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <commctrl.h>

long RegSetValueExRecursive(HKEY hKey, LPCTSTR lpValueName, DWORD Reserved, DWORD dwType, CONST BYTE *lpData, DWORD cbData)
{
	TCHAR szBuffer[256];

	(void)lstrcpy(szBuffer, lpValueName);

	LPTSTR pszBuffer = szBuffer;
	LPTSTR pszLast = szBuffer;
	while (*pszBuffer)
	{
		if (*pszBuffer == _T('\\') || *pszBuffer == _T('/'))
		{
			pszLast = pszBuffer;
			lpValueName = pszLast + 1;
		}
		pszBuffer++;
	}

	bool m_bNeedToCloseKey = false;
	if (pszLast != szBuffer)
	{
		*pszLast = _T('\000');
		HKEY hkeyTemp;
		long lRet = RegOpenKey(hKey, szBuffer, &hkeyTemp);
		if (lRet != ERROR_SUCCESS)
		{
			lRet = RegCreateKey(hKey, szBuffer, &hkeyTemp);
			if (lRet != ERROR_SUCCESS)
				return lRet;
		}
		hKey = hkeyTemp;
		m_bNeedToCloseKey = true;
	}

	long lRet = RegSetValueEx(hKey, lpValueName, Reserved, dwType, lpData, cbData);
	if (m_bNeedToCloseKey)
	{
		RegCloseKey(hKey);
	}
	return lRet;
}
