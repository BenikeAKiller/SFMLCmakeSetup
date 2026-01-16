@echo off
setlocal enabledelayedexpansion

:: 1. Create Folder
if not exist "FLAC_for_quiz" mkdir "FLAC_for_quiz"

echo Scanning for WAV files to convert to FLAC...
echo.

:: 2. Loop through all WAV files in current folder
for %%f in (*.wav) do (
    echo Converting: %%f
    :: Use high compression flac (level 8) to save space while remaining lossless
    ffmpeg -i "%%f" -compression_level 8 "FLAC_for_quiz\%%~nf.flac" -y
)

echo.
echo Conversion complete! Files are in the 'FLAC_for_quiz' folder.
echo You can now delete your original WAVs if you wish.
pause