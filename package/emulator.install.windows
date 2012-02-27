@ECHO OFF
set vtm_shortcut_name=Emulator Manager
set execute_path=Emulator\bin
set vtm_execute_file=emulator-manager.exe
set icon_path=Emulator\skins\icons
set vtm_icon_file=vtm.ico

set program_path=%INSTALLED_PATH%\%execute_path%
set desktop_menu_icon_path=%INSTALLED_PATH%\%icon_path%

echo Program path : %program_path%
echo Desktop menu icon path : %desktop_menu_icon_path%
echo Setting shortcut...
wscript.exe %MAKESHORTCUT_PATH% /shortcut:"%vtm_shortcut_name%" /target:"%program_path%\%vtm_execute_file%" /icon:"%desktop_menu_icon_path%\%vtm_icon_file%"
echo Setting registry
reg add "hklm\SOFTWARE\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers"  /f /v %program_path%\%vtm_execute_file% /t REG_SZ /d RUNASADMIN
echo COMPLETE
