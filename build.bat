SET ADDON_NAME=g-force
SET ADDON_NAME_ID=visualization.g-force
SET VS_SOLUTION=%ADDON_NAME%.sln
SET DLL_ADDON=%ADDON_NAME%.vis

ECHO Cleaning project...
"%VS71COMNTOOLS%\..\IDE\devenv.com" .\src\libGForce\libGForce.sln /clean Release
"%VS71COMNTOOLS%\..\IDE\devenv.com" .\src\G-ForceXBOX\%VS_SOLUTION% /clean Release

ECHO Downloading prerequests...
IF NOT EXIST "src\include" (
    git clone https://github.com/xbmc4xbox/binary-addons-deps
    XCOPY binary-addons-deps\xbmc\addons\include\ src\include\ /E /H /C /I /Y
    @RD /S /Q binary-addons-deps
)

ECHO Compiling addon...
"%VS71COMNTOOLS%\..\IDE\devenv.com" .\src\libGForce\libGForce.sln /build Release
IF NOT EXIST "src\libGForce\Release\libGForce.lib" (
    ECHO Could not compile libGForce. Aborting...
    EXIT
)
XCOPY src\libGForce\Release\libGForce.lib .\src\G-ForceXBOX\lib\ /E /H /C /I /Y
"%VS71COMNTOOLS%\..\IDE\devenv.com" .\src\G-ForceXBOX\%VS_SOLUTION% /build Release
IF NOT EXIST "src\G-ForceXBOX\Release\%DLL_ADDON%" (
    ECHO Could not compile visualizer. Aborting...
    EXIT
)

ECHO Building addon...
XCOPY src\G-ForceXBOX\Release\%DLL_ADDON% %ADDON_NAME_ID%\ /E /H /C /I /Y

FOR /F "tokens=* USEBACKQ" %%F IN (`powershell -NoProfile -Command ^
    "[xml]$xml = Get-Content '%ADDON_NAME_ID%\addon.xml'; $xml.addon.version"`) DO (
    SET "VERSION=%%F"
)

ECHO Compressing addon...
7z a -tzip "%ADDON_NAME_ID%-%VERSION%.zip" "%ADDON_NAME_ID%\*"