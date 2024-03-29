//
// File:       iTunesPlugInWin.cpp
//
// Abstract:   Visual plug-in for iTunes on Windows
//
// Version:    2.0
//
// Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple Inc. ( "Apple" )
//             in consideration of your agreement to the following terms, and your use,
//             installation, modification or redistribution of this Apple software
//             constitutes acceptance of these terms.  If you do not agree with these
//             terms, please do not use, install, modify or redistribute this Apple
//             software.
//
//             In consideration of your agreement to abide by the following terms, and
//             subject to these terms, Apple grants you a personal, non - exclusive
//             license, under Apple's copyrights in this original Apple software ( the
//             "Apple Software" ), to use, reproduce, modify and redistribute the Apple
//             Software, with or without modifications, in source and / or binary forms;
//             provided that if you redistribute the Apple Software in its entirety and
//             without modifications, you must retain this notice and the following text
//             and disclaimers in all such redistributions of the Apple Software. Neither
//             the name, trademarks, service marks or logos of Apple Inc. may be used to
//             endorse or promote products derived from the Apple Software without specific
//             prior written permission from Apple.  Except as expressly stated in this
//             notice, no other rights or licenses, express or implied, are granted by
//             Apple herein, including but not limited to any patent rights that may be
//             infringed by your derivative works or by other works in which the Apple
//             Software may be incorporated.
//
//             The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
//             WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
//             WARRANTIES OF NON - INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A
//             PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION
//             ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
//
//             IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
//             CONSEQUENTIAL DAMAGES ( INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//             SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//             INTERRUPTION ) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION
//             AND / OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER
//             UNDER THEORY OF CONTRACT, TORT ( INCLUDING NEGLIGENCE ), STRICT LIABILITY OR
//             OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Copyright (c) 2001-2011 Apple Inc. All Rights Reserved.
//

//-------------------------------------------------------------------------------------------------
//	includes
//-------------------------------------------------------------------------------------------------

#include "iTunesPlugIn.h"
#include "Time.h"
#include <GdiPlus.h>

//-------------------------------------------------------------------------------------------------
//	constants, etc.
//-------------------------------------------------------------------------------------------------

#define kTVisualPluginName              L"Waveform Visualizer"

static	WNDPROC			 giTunesWndProc			= NULL;
static VisualPluginData* gVisualPluginData		= NULL;

//-------------------------------------------------------------------------------------------------
//	WndProc
//-------------------------------------------------------------------------------------------------
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CHAR:			// handle key pressed messages
			if (wParam == 'I' || wParam == 'i')
			{
				UpdateInfoTimeOut( gVisualPluginData );
			}
			break;

		default:
			break;
	}

	// call iTunes with the event.  Always send space bar, ESC key, TAB key and the arrow keys: iTunes reserves those keys
	return CallWindowProc( giTunesWndProc, hWnd, message, wParam, lParam );
}

//-------------------------------------------------------------------------------------------------
//	DrawVisual
//-------------------------------------------------------------------------------------------------
//
void DrawVisual( VisualPluginData * visualPluginData )
{
	// this shouldn't happen but let's be safe
	if (!visualPluginData || visualPluginData->destView == NULL) return;

	HDC hDC = ::GetDC( visualPluginData->destView );
	if (!hDC) return;

	// fill the whole view with black to start
	RECT clientRect;
	if (!::GetClientRect(visualPluginData->destView, &clientRect))
	{
		ReleaseDC(visualPluginData->destView, hDC);
		return;
	}
	::FillRect( hDC, &clientRect, (HBRUSH) GetStockObject( BLACK_BRUSH ) );

	HPEN pen = ::CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
	if (!pen)
	{
		ReleaseDC(visualPluginData->destView, hDC);
		return;
	}
	SelectObject(hDC, pen);

	const int width = clientRect.right - clientRect.left;
	const int height = clientRect.bottom - clientRect.top;
	const int centerHeight = height / 2;

	if (visualPluginData->playing)
	{
		// scale it to the window
		for (int i = 0; i < visualPluginData->points.size(); i++)
		{
			POINT* point = &visualPluginData->points.at(i);

			LONG x = point->x;
			LONG y = point->y;

			x = (x * width) / kVisualNumWaveformEntries;

			y = (y * height) / 255; // waveforms are 0-255, scale it to the window height
			y = centerHeight + (y - centerHeight); // invert the waveform across y-axis

			point->x = x;
			point->y = y;
		}

		Polyline(hDC, visualPluginData->points.data(), visualPluginData->points.size());
		
		visualPluginData->points.clear();
	}
	else
	{
		const POINT points[2] = {
			{ 0, centerHeight },
			{ width, centerHeight }
		};
		Polyline(hDC, points, 2);
	}

	DeleteObject(pen);

	ReleaseDC(visualPluginData->destView, hDC);
}

//-------------------------------------------------------------------------------------------------
//	UpdateArtwork
//-------------------------------------------------------------------------------------------------
//
void UpdateArtwork( VisualPluginData * visualPluginData, VISUAL_PLATFORM_DATA coverArt, UInt32 coverArtSize, UInt32 coverArtFormat )
{
	visualPluginData->currentArtwork = NULL;

	HGLOBAL	globalMemory  = GlobalAlloc( GMEM_MOVEABLE, coverArtSize );
	if ( globalMemory ) 
	{
		void* pBuffer = ::GlobalLock(globalMemory);
		if ( pBuffer )
		{
			memcpy( pBuffer, coverArt, coverArtSize );

			IStream* stream = NULL;
			CreateStreamOnHGlobal( globalMemory, TRUE, &stream );

			if ( stream )
			{
				Gdiplus::Bitmap* gdiPlusBitmap = Gdiplus::Bitmap::FromStream( stream );
				stream->Release();

				if ( gdiPlusBitmap && gdiPlusBitmap->GetLastStatus() == Gdiplus::Ok)
				{
					visualPluginData->currentArtwork = gdiPlusBitmap;
				}
			}
		}
	}

	UpdateInfoTimeOut( visualPluginData );
}

//-------------------------------------------------------------------------------------------------
//	InvalidateVisual
//-------------------------------------------------------------------------------------------------
//
void InvalidateVisual( VisualPluginData * visualPluginData )
{
	(void) visualPluginData;

	// set flags for internal state to decide what to draw the next time around
}

//-------------------------------------------------------------------------------------------------
//	ActivateVisual
//-------------------------------------------------------------------------------------------------
//
OSStatus ActivateVisual( VisualPluginData * visualPluginData, VISUAL_PLATFORM_VIEW destView, OptionBits options )
{
	gVisualPluginData = visualPluginData;

	GetClientRect( destView, &visualPluginData->destRect );
	visualPluginData->destView		= destView;
	visualPluginData->destOptions	= options;

	if ( giTunesWndProc == NULL )
	{
		giTunesWndProc = (WNDPROC)SetWindowLongPtr( destView, GWLP_WNDPROC, (LONG_PTR)WndProc );
	}

	UpdateInfoTimeOut( visualPluginData );

	return noErr;
}

//-------------------------------------------------------------------------------------------------
//	MoveVisual
//-------------------------------------------------------------------------------------------------
//
OSStatus MoveVisual( VisualPluginData * visualPluginData, OptionBits newOptions )
{
	GetClientRect( visualPluginData->destView, &visualPluginData->destRect );
	visualPluginData->destOptions = newOptions;

	return noErr;
}

//-------------------------------------------------------------------------------------------------
//	DeactivateVisual
//-------------------------------------------------------------------------------------------------
//
OSStatus DeactivateVisual( VisualPluginData * visualPluginData )
{
	gVisualPluginData = NULL;

	if (giTunesWndProc != NULL )
	{
		SetWindowLongPtr( visualPluginData->destView, GWLP_WNDPROC, (LONG_PTR)giTunesWndProc );
		giTunesWndProc = NULL;
	}

	visualPluginData->destView = NULL;

	return noErr;
}

//-------------------------------------------------------------------------------------------------
//	ResizeVisual
//-------------------------------------------------------------------------------------------------
//
OSStatus ResizeVisual( VisualPluginData * visualPluginData )
{
	GetClientRect( visualPluginData->destView, &visualPluginData->destRect );
	visualPluginData->lastDrawTime = 0;

	return noErr;
}

//-------------------------------------------------------------------------------------------------
//	ConfigureVisual
//-------------------------------------------------------------------------------------------------
//
OSStatus ConfigureVisual( VisualPluginData * visualPluginData )
{
	(void) visualPluginData;

	visualPluginData->currentArtwork = NULL;

	return noErr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//	GetVisualName
//-------------------------------------------------------------------------------------------------
//
void GetVisualName( ITUniStr255 name )
{	
	name[0] = (UniChar)wcslen( kTVisualPluginName );
	wcscpy_s( (wchar_t *)&name[1], 255, kTVisualPluginName );
}

//-------------------------------------------------------------------------------------------------
//	GetVisualOptions
//-------------------------------------------------------------------------------------------------
//
OptionBits GetVisualOptions( void )
{
	return kVisualWantsIdleMessages;
}

//-------------------------------------------------------------------------------------------------
//	iTunesPluginMain
//-------------------------------------------------------------------------------------------------
//
extern "C" __declspec(dllexport) OSStatus iTunesPluginMain( OSType message, PluginMessageInfo * messageInfo, void * refCon )
{
	OSStatus		status;
	
	(void) refCon;
	
	switch ( message )
	{
		case kPluginInitMessage:
			status = RegisterVisualPlugin( messageInfo );
			break;
			
		case kPluginCleanupMessage:
			status = noErr;
			break;
			
		default:
			status = unimpErr;
			break;
	}
	
	return status;
}
