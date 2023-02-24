/*----------------------------------------------------------------------
Copyright (c)  Gipsysoft. All Rights Reserved.
File:	DialogSizer_Set.cpp
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
Purpose:	Main functionality for sizeable dialogs

	Store a local copy of the user settings
	Subclass the window
	Respond to various messages withinn the subclassed window.

----------------------------------------------------------------------*/
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <commctrl.h>
#include "DialogSizer.h"

static LPCTSTR pcszDialogDataPropertyName = _T("SizerProperty");
static LPCTSTR pcszWindowProcPropertyName = _T("SizerWindowProc");

extern long RegQueryValueExRecursive(HKEY hKey, LPCTSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
extern long RegSetValueExRecursive(HKEY hKey, LPCTSTR lpValueName, DWORD Reserved, DWORD dwType, CONST BYTE *lpData, DWORD cbData);

WINDOWPLACEMENT	m_wpl;

struct DialogData	//	dd
{
	//
	//	The number of items contained in the psd member.
	//	Used in the DeferWindowPos structure and in allocating memory
	int nItemCount;
	DialogSizerSizingItem *psd;

	//
	//	We need the smallest to respond to the WM_GETMINMAXINFO message
	POINT m_ptSmallest;

	//
	//	We don't strictly speaking need to say how big the biggest can be but
	POINT m_ptLargest;
	bool m_bLargestSet;

	//
	//	we need this to decide how much the window has changed size when we get a WM_SIZE message
	SIZE m_sizeClient;

	//
	//	Draw the sizing grip...or not
	bool m_bMaximised;
	BOOL m_bShowSizingGrip;

	RECT rcGrip;
};


static LRESULT CALLBACK SizingProc(HWND, UINT, WPARAM, LPARAM);


static inline int GetItemCount(const DialogSizerSizingItem *psd)
//
//	Given an array of dialog item structures determine how many of them there
//	are by scanning along them until I reach the last.
{
	int nCount = 0;
	while (psd->uSizeInfo != UINT_MAX)
	{
		nCount++;
		psd++;
	}
	return nCount;
}


static void UpdateGripperRect(const int cx, const int cy, RECT *rcGrip)
{
	const int nGripWidth = GetSystemMetrics(SM_CYVSCROLL);
	const int nGripHeight = GetSystemMetrics(SM_CXVSCROLL);

	rcGrip->left = cx - nGripWidth;
	rcGrip->top = cy - nGripHeight;
	rcGrip->right = cx;
	rcGrip->bottom = cy;
}


static void UpdateGripper(HWND hwnd, DialogData *pdd)
{
	if (pdd->m_bShowSizingGrip)
	{
		RECT old;

		old = pdd->rcGrip;

		UpdateGripperRect(pdd->m_sizeClient.cx, pdd->m_sizeClient.cy, &pdd->rcGrip);

		//
		//	We also need to invalidate the combined area of the old and new rectangles
		//	otherwise we would have trail of grippers when we sized the dialog larger
		//	in any axis
		(void)UnionRect(&old, &old, &pdd->rcGrip);
		(void)InvalidateRect(hwnd, &old, TRUE);
	}
}


static inline void CopyItems(DialogSizerSizingItem *psdDest, const DialogSizerSizingItem *psdSource)
//
//	Will copy all of the items in psdSource into psdDest.
{
	//
	//	Loop til we reach the end
	while (psdSource->uSizeInfo != UINT_MAX)
	{
		*psdDest = *psdSource;
		psdDest++;
		psdSource++;
	}
	//	And when we do copy the last item
	*psdDest = *psdSource;
}


static DialogData *AddDialogData(HWND hwnd)
//
//	Firstly determine if the data already exists, if it does then return that, if not then we will
//	create and initialise a brand new structure.
{
	DialogData *pdd = reinterpret_cast<DialogData *>(GetProp(hwnd, pcszDialogDataPropertyName));
	if (!pdd)
	{
		pdd = reinterpret_cast<DialogData *>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DialogData)));
	}

	if (pdd)
	{
		//
		//	Store some sizes etc. for later.
		RECT	rc;
		GetWindowRect(hwnd, &rc);
		pdd->m_ptSmallest.x = rc.right - rc.left;
		pdd->m_ptSmallest.y = rc.bottom - rc.top;

		GetClientRect(hwnd, &rc);
		pdd->m_sizeClient.cx = rc.right - rc.left;
		pdd->m_sizeClient.cy = rc.bottom - rc.top;

		SetProp(hwnd, pcszDialogDataPropertyName, reinterpret_cast<HANDLE>(pdd));
		UpdateGripperRect(pdd->m_sizeClient.cx, pdd->m_sizeClient.cy, &pdd->rcGrip);

		//
		//	Because we have successffuly created our data we need to subclass the control now, if not
		//	we could end up in a situation where our data was never freed.
		SetProp(hwnd, pcszWindowProcPropertyName, reinterpret_cast<HANDLE>(SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SizingProc))));
	}
	return pdd;
}

extern "C"
{
	BOOL DialogSizer_Set(HWND hwnd, const DialogSizerSizingItem *psd, BOOL bShowSizingGrip, SIZE *psizeMax)
		//
		//	Setting a dialog sizeable involves subclassing the window and handling it's
		//	WM_SIZE messages, if we have a hkRootSave and pcszName then we will also be loading/saving
		//	the size and position of the window from the registry. We load from the registry when we 
		//	subclass the window and we save to the registry when we get a WM_DESTROY.
		//
		//	It will return non-zero for success and zero if it fails
	{
		//
		//	Make sure all of the parameters are valid.
		if (IsWindow (hwnd))
		{
			HANDLE heap = GetProcessHeap();
			DialogData *pdd = AddDialogData(hwnd);
			if (pdd)
			{
				pdd->m_bShowSizingGrip = bShowSizingGrip;
				pdd->nItemCount = GetItemCount(psd) + 1;
				pdd->psd = reinterpret_cast<DialogSizerSizingItem *>(HeapAlloc(heap, 0, sizeof(DialogSizerSizingItem) * pdd->nItemCount));
				if (pdd->psd)
				{
					//
					//	Copy all of the user controls etc. for later, this way the user can quite happily
					//	let the structure go out of scope.
					CopyItems(pdd->psd, psd);
					if (psizeMax)
					{
						pdd->m_ptLargest.x = psizeMax->cx;
						pdd->m_ptLargest.y = psizeMax->cy;
						pdd->m_bLargestSet = true;
					}

					//
					//	If the there was save info passed in then we need to make damn good use of it
					//	by attempting to load the RegistryData structure 
					/*if( hkRootSave && pcszName )
					{
						WINDOWPLACEMENT rd;
						DWORD dwSize = sizeof( rd );
						DWORD dwType = REG_BINARY;
						if( RegQueryValueExRecursive( hkRootSave, pcszName, NULL, &dwType, reinterpret_cast<LPBYTE>( &rd ), &dwSize ) == ERROR_SUCCESS && dwSize == sizeof( rd ) )
						{
							if( !(GetWindowStyle( hwnd ) & WS_VISIBLE) )
								rd.showCmd = SW_HIDE;

							SetWindowPlacement( hwnd, &rd );
						}
					}*/

					return TRUE;
				}
				else
				{
					HeapFree(heap, 0, pdd);
				}
			}
		}
		return FALSE;
	}
}

void UpdateWindowSize(const int cx, const int cy, HWND hwnd)
{
	DialogData *pdd = reinterpret_cast<DialogData *>(GetProp(hwnd, pcszDialogDataPropertyName));
	if (pdd)
	{
		const int nDeltaX = cx - pdd->m_sizeClient.cx;
		const int nDeltaY = cy - pdd->m_sizeClient.cy;
		HDWP	defer;
		defer = BeginDeferWindowPos (pdd->nItemCount);
		RECT rc;
		const DialogSizerSizingItem *psd = pdd->psd;

		if (!nDeltaX && !nDeltaY)
			return;

		while (psd->uSizeInfo != UINT_MAX)
		{
			HWND hwndChild = GetDlgItem(hwnd, psd->uControlID);

			if (!hwndChild)
			{
				int err = GetLastError ();
				psd++;
				continue;
			}

			GetWindowRect(hwndChild, &rc);
			(void)MapWindowPoints(GetDesktopWindow(), hwnd, (LPPOINT)&rc, 2);

			//
			//	Adjust the window horizontally
			if (psd->uSizeInfo & DS_MoveX)
			{
				rc.left += nDeltaX;
				rc.right += nDeltaX;
			}

			//
			//	Adjust the window vertically
			if (psd->uSizeInfo & DS_MoveY)
			{
				rc.top += nDeltaY;
				rc.bottom += nDeltaY;
			}

			//
			//	Size the window horizontally
			if (psd->uSizeInfo & DS_SizeX)
			{
				rc.right += nDeltaX;
			}

			//
			//	Size the window vertically
			if (psd->uSizeInfo & DS_SizeY)
			{
				rc.bottom += nDeltaY;
			}

			DeferWindowPos(defer, hwndChild, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOACTIVATE | SWP_NOZORDER);
			psd++;
		}

		EndDeferWindowPos (defer);

		pdd->m_sizeClient.cx = cx;
		pdd->m_sizeClient.cy = cy;

		//
		//	If we have a sizing grip enabled then adjust it's position
		UpdateGripper(hwnd, pdd);


		//UpdateWindow (hwnd);
	}
}


static LRESULT CALLBACK SizingProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
//
//	Actual window procedure that will handle saving window size/position and moving
//	the controls whilst the window sizes.
{
	WNDPROC pOldProc = reinterpret_cast<WNDPROC>(GetProp(hwnd, pcszWindowProcPropertyName));
	switch (msg)
	{
		case WM_ERASEBKGND:
		{
			LRESULT lr = CallWindowProc(pOldProc, hwnd, msg, wParam, lParam);
			DialogData *pdd = reinterpret_cast<DialogData *>(GetProp(hwnd, pcszDialogDataPropertyName));
			if (pdd && pdd->m_bShowSizingGrip && !pdd->m_bMaximised)
			{
				::DrawFrameControl(reinterpret_cast<HDC>(wParam), &pdd->rcGrip, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
			}
			return lr;
		}

		case WM_SIZE:
		{
			DialogData *pdd = reinterpret_cast<DialogData *>(GetProp(hwnd, pcszDialogDataPropertyName));
			if (pdd && wParam != SIZE_MINIMIZED)
			{
				pdd->m_bMaximised = (wParam == SIZE_MAXIMIZED ? true : false);
				UpdateWindowSize(LOWORD(lParam), HIWORD(lParam), hwnd);
				//InvalidateRect (hwnd, NULL, TRUE);
			}
		}
		break;


		case WM_NCHITTEST:
		{
			//
			//	If the gripper is enabled then perform a simple hit test on our gripper area.
			DialogData *pdd = reinterpret_cast<DialogData *>(GetProp(hwnd, pcszDialogDataPropertyName));
			if (pdd && pdd->m_bShowSizingGrip)
			{
				POINT pt = { LOWORD(lParam), HIWORD(lParam) };
				(void)ScreenToClient(hwnd, &pt);
				if (PtInRect(&pdd->rcGrip, pt))
					return HTBOTTOMRIGHT;
			}
		}
		break;


		case WM_GETMINMAXINFO:
		{
			//
			//	Our opportunity to say that we do not want the dialog to grow or shrink any more.
			DialogData *pdd = reinterpret_cast<DialogData *>(GetProp(hwnd, pcszDialogDataPropertyName));
			LPMINMAXINFO lpmmi = reinterpret_cast<LPMINMAXINFO>(lParam);
			lpmmi->ptMinTrackSize = pdd->m_ptSmallest;
			if (pdd->m_bLargestSet)
			{
				lpmmi->ptMaxTrackSize = pdd->m_ptLargest;
			}
		}
		return 0;


		case WM_NOTIFY:
		{
			if (reinterpret_cast<LPNMHDR>(lParam)->code == PSN_SETACTIVE)
			{
				RECT rc;
				GetClientRect(GetParent(hwnd), &rc);
				UpdateWindowSize(rc.right - rc.left, rc.bottom - rc.top, GetParent(hwnd));
			}
		}
		break;


		case WM_DESTROY:
		{
			//
			//	Our opportunty for cleanup.
			//	Simply acquire all of our objects, free the appropriate memory and remove the 
			//	properties from the window. If we do not remove the properties then they will constitute
			//	a resource leak.
			DialogData *pdd = reinterpret_cast<DialogData *>(GetProp(hwnd, pcszDialogDataPropertyName));
			if (pdd)
			{
				/*RegistryData rd;
				rd.m_wpl.length = sizeof( rd.m_wpl );
				GetWindowPlacement( hwnd, &rd.m_wpl );

				if( pdd->hkRootSave && pdd->pcszName )
				{
					(void)RegSetValueExRecursive( pdd->hkRootSave, pdd->pcszName, NULL, REG_BINARY, reinterpret_cast<LPBYTE>( &rd ), sizeof( rd ) );
				}*/

				if (pdd->psd)
				{
					HeapFree(GetProcessHeap(), 0, pdd->psd);
				}
				HeapFree(GetProcessHeap(), 0, pdd);
				(void)RemoveProp(hwnd, pcszDialogDataPropertyName);
			}

			(void)SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(pOldProc));
			(void)RemoveProp(hwnd, pcszWindowProcPropertyName);
		}
		break;
	}
	return CallWindowProc(pOldProc, hwnd, msg, wParam, lParam);
}
