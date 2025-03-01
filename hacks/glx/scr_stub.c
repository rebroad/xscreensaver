#include <windows.h>

extern int main(int argc, char **argv); // From screenhack.c

LRESULT WINAPI ScreenSaverProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static int argc = 1;
    static char *argv[] = {"hextrail.scr", NULL};
    switch (msg) {
        case WM_CREATE:
            main(argc, argv); // Run the screensaver
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefScreenSaverProc(hwnd, msg, wParam, lParam);
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    return FALSE; // No config dialog for now
}

BOOL WINAPI RegisterDialogClasses(HANDLE hInst) {
    return TRUE;
}
