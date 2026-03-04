npx markdown-folder-to-html .\docs\guide

xcopy /E /I /Y .\docs\_guide ..\soundshed-guitar-web\docs\
rd /S /Q .\docs\_guide

