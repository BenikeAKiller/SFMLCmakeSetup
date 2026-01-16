@echo off
REM Create output folder
if not exist OGG_for_quiz mkdir OGG_for_quiz

REM Convert all WAV files to OGG
for %%f in (*.wav) do (
    ffmpeg -i "%%f" -ar 44100 -ac 2 -c:a libvorbis -q:a 6 -joint_stereo 1 "OGG_for_quiz\%%~nf.ogg"
)

echo Conversion complete! Check the OGG_for_quiz folder.
pause
