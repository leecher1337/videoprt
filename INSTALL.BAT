if not exist %windir%\system32\drivers\vidprt0.sys (
copy %windir%\system32\drivers\videoprt.sys %windir%\system32\drivers\vidprt0.sys
copy %windir%\system32\dllcache\videoprt.sys %windir%\system32\dllcache\vidprt0.sys
copy %windir%\ServicePackFiles\i386\videoprt.sys %windir%\ServicePackFiles\i386\vidprt0.sys
)
copy videoprt.sys %windir%\ServicePackFiles\i386\videoprt.sys
copy videoprt.sys %windir%\system32\drivers\videoprt.sys
copy videoprt.sys %windir%\system32\dllcache\videoprt.sys
