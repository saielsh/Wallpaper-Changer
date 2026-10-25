#include <windows.h>
#include <gdiplus.h>
#include <wchar.h>
#include <stdlib.h>
#include <time.h>

#define MAX_WALLPAPERS 1000
#define INTERVAL 60000

typedef struct {
    WCHAR path[MAX_PATH];
} Wallpaper;

Wallpaper wallpapers[MAX_WALLPAPERS];

int count = 0;
int current = 0;
int previous = -1;

WCHAR appFolder[MAX_PATH];
WCHAR wallpaperFolder[MAX_PATH];
WCHAR bitmapPath[MAX_PATH];

ULONG_PTR gdiplusToken;

// GDI+ converts PNG and JPEG images to BMP 
int ConvertToBitmap(const WCHAR *source){
    GpImage *image = NULL;
    CLSID bmpEncoder = {
        0x557cf400, 0x1a04, 0x11d3,
        {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}
    };

    if(GdipLoadImageFromFile(source, &image) != 0)
        return 0;

    GpStatus result = GdipSaveImageToFile(
        image, bitmapPath, &bmpEncoder, NULL
    );

    GdipDisposeImage(image);
    return result == 0;
}

// Find images without requiring specific filenames 
void LoadWallpapers(void){
    WCHAR searchPath[MAX_PATH];
    swprintf(searchPath, MAX_PATH, L"%s\\*.*",
             wallpaperFolder);
    WIN32_FIND_DATAW data;
    HANDLE search = FindFirstFileW(searchPath, &data);

    if (search == INVALID_HANDLE_VALUE)
        return;
    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        WCHAR *extension = wcsrchr(data.cFileName, L'.');
        if (!extension)
            continue;
        if (_wcsicmp(extension, L".jpg") != 0 &&
            _wcsicmp(extension, L".jpeg") != 0 &&
            _wcsicmp(extension, L".png") != 0)
            continue;
        if (count >= MAX_WALLPAPERS)
            break;
        swprintf(wallpapers[count].path, MAX_PATH,
                 L"%s\\%s",
                 wallpaperFolder, data.cFileName);
        count++;
    } while (FindNextFileW(search, &data));
    FindClose(search);
}

// Shuffle the images into a random order 
void ShuffleWallpapers(void){
    for (int i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);

        Wallpaper temp = wallpapers[i];
        wallpapers[i] = wallpapers[j];
        wallpapers[j] = temp;
    }

    // Avoid repeating the last image across cycles 
    if(count > 1 && previous == 0){
        Wallpaper temp = wallpapers[0];
        wallpapers[0] = wallpapers[1];
        wallpapers[1] = temp;
    }
}

// Register this program to start with Windows 
void EnableStartup(void){
    HKEY key;
    if (RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return;
    WCHAR exePath[MAX_PATH];
    WCHAR command[MAX_PATH + 4];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    swprintf(command, MAX_PATH + 4, L"\"%s\"", exePath);

    RegSetValueExW(
        key, L"WallpaperChanger", 0, REG_SZ,
        (const BYTE *)command,
        (DWORD)((wcslen(command) + 1) * sizeof(WCHAR))
    );
    RegCloseKey(key);
}

// Change the desktop wallpaper 
int SetWallpaper(const WCHAR *image){
    if (!ConvertToBitmap(image))
        return 0;

    return SystemParametersInfoW(
        SPI_SETDESKWALLPAPER,
        0,
        bitmapPath,
        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE
    );
}

// Choose the next image 
void ChangeWallpaper(void){
    if (count == 0)
        return;
    if (current >= count) {
        current = 0;
        ShuffleWallpapers();
    }
    if (SetWallpaper(wallpapers[current].path)) {
        previous = current;
        current++;
    }
}

// Windows background application entry point 
int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE previousInstance,
    PWSTR commandLine,
    int showCommand
)
{
    (void)instance;
    (void)previousInstance;
    (void)commandLine;
    (void)showCommand;

    srand((unsigned int)time(NULL));

    // Find the folder containing this program 
    GetModuleFileNameW(NULL, appFolder, MAX_PATH);

    WCHAR *slash = wcsrchr(appFolder, L'\\');

    if (!slash)
        return 1;

    *slash = L'\0';

    swprintf(wallpaperFolder, MAX_PATH,
             L"%s\\wallpapers", appFolder);

    swprintf(bitmapPath, MAX_PATH,
             L"%s\\current.bmp", appFolder);

    // Start GDI+ 
    GdiplusStartupInput input = {0};
    input.GdiplusVersion = 1;

    if (GdiplusStartup(&gdiplusToken, &input, NULL) != 0)
        return 1;
    LoadWallpapers();
    if (count == 0) {
        GdiplusShutdown(gdiplusToken);
        return 1;
    }

    ShuffleWallpapers();

    // Show the first image immediately 
    ChangeWallpaper();

    // Automatically start on future Windows logins 
    EnableStartup();

    // Keep running silently 
    for (;;) {
        Sleep(INTERVAL);
        ChangeWallpaper();
    }
    return 0;
}
