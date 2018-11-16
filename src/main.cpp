/*
	Copyright (C) 2016 php42

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Ole2.h>
#include "gui.h"
#include "textconvert.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    MSG msg = {0};
    HRESULT status = OleInitialize(NULL);
    if(!SUCCEEDED(status))
    {
        MessageBoxW(NULL, L"OleInitialize() failed", L"Error", MB_OK);
        return EXIT_FAILURE;
    }

    try
    {
        RandomizerGUI wnd(hInstance);

        while(GetMessageW(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    catch(std::exception& ex)
    {
        MessageBoxW(NULL, utf_widen(ex.what()).c_str(), L"Error", MB_OK);
        OleUninitialize();
        return EXIT_FAILURE;
    }
    catch(...)
    {
        MessageBoxW(NULL, L"Unknown Exception", L"Error", MB_OK);
        OleUninitialize();
        return EXIT_FAILURE;
    }

    OleUninitialize();

    return msg.wParam;
}
