cd "C:\Projects\crawl\crawl-ref\source\MSVC\\..\rltiles"
echo Generating main.png...
"C:\Projects\crawl\crawl-ref\source\tilegen.exe" dc-main.txt
echo Generating dngn.png...
"C:\Projects\crawl\crawl-ref\source\tilegen.exe" dc-dngn.txt
echo Generating floor
"C:\Projects\crawl\crawl-ref\source\tilegen.exe" dc-floor.txt
echo Generating wall
"C:\Projects\crawl\crawl-ref\source\tilegen.exe" dc-wall.txt
echo Generating feat
"C:\Projects\crawl\crawl-ref\source\tilegen.exe" dc-feat.txt
echo Generating player.png...
"C:\Projects\crawl\crawl-ref\source\tilegen.exe" dc-player.txt
echo Generating gui.png...
"C:\Projects\crawl\crawl-ref\source\tilegen.exe" dc-gui.txt
echo Generating icons..
"C:\Projects\crawl\crawl-ref\source\tilegen.exe" dc-icons.txt
copy *.png ..\dat\tiles\