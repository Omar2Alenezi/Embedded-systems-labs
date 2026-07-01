//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

#include "sd_card.h"

#include "event_log.h"
#include "date_and_time.h"
#include "pc_serial_com.h"

#include "FATFileSystem.h"
#include "SDBlockDevice.h"

#include "platform/mbed_retarget.h"

//=====[Declaration of private defines]========================================

#define SPI3_MOSI   PC_12
#define SPI3_MISO   PC_11
#define SPI3_SCK    PC_10
#define SPI3_CS     PA_4_ALT0

//=====[Declaration and initialization of public global objects]===============

SDBlockDevice  sd( SPI3_MOSI, SPI3_MISO, SPI3_SCK, SPI3_CS );
FATFileSystem  sdCardFileSystem( "sd", &sd );

//=====[Implementations of public functions]===================================

bool sdCardInit()
{
    pcSerialComStringWrite( "Looking for a filesystem in the SD card... \r\n" );
    sdCardFileSystem.mount( &sd );

    DIR *sdCardListOfDirectories = opendir( "/sd/" );
    if( sdCardListOfDirectories != NULL ) {
        pcSerialComStringWrite( "Filesystem found in the SD card. \r\n" );
        closedir( sdCardListOfDirectories );
        return true;
    } else {
        pcSerialComStringWrite( "Filesystem not mounted. \r\n" );
        pcSerialComStringWrite( "Insert an SD card and " );
        pcSerialComStringWrite( "reset the NUCLEO board.\r\n" );
        return false;
    }
}

bool sdCardWriteFile( const char* fileName, const char* writeBuffer )
{
    char fileNameSD[SD_CARD_FILENAME_MAX_LENGTH + 4] = "";

    strcat( fileNameSD, "/sd/" );
    strcat( fileNameSD, fileName );

    FILE *sdCardFilePointer = fopen( fileNameSD, "a" );

    if( sdCardFilePointer != NULL ) {
        fprintf( sdCardFilePointer, "%s", writeBuffer );
        fclose( sdCardFilePointer );
        return true;
    } else {
        return false;
    }
}

bool sdCardReadFile( const char* fileName, char* readBuffer, int maxLen )
{
    char fileNameSD[SD_CARD_FILENAME_MAX_LENGTH + 4] = "";

    strcat( fileNameSD, "/sd/" );
    strcat( fileNameSD, fileName );

    FILE *sdCardFilePointer = fopen( fileNameSD, "r" );

    if( sdCardFilePointer == NULL ) {
        readBuffer[0] = '\0';
        return false;
    }

    int bytesRead = 0;
    int ch;

    // Read byte-by-byte; stop at buffer limit (leave room for null terminator)
    while( bytesRead < maxLen - 1 ) {
        ch = fgetc( sdCardFilePointer );
        if( ch == EOF ) break;
        readBuffer[bytesRead++] = (char)ch;
    }
    readBuffer[bytesRead] = '\0';

    fclose( sdCardFilePointer );
    return ( bytesRead > 0 );


    
}

int sdCardReadFileLineCount( const char* fileName )
{
    char fileNameSD[SD_CARD_FILENAME_MAX_LENGTH + 4] = "";
    strcat( fileNameSD, "/sd/" );
    strcat( fileNameSD, fileName );

    FILE *sdCardFilePointer = fopen( fileNameSD, "r" );
    if ( sdCardFilePointer == NULL ) {
        return 0;
    }

    int  lineCount   = 0;
    int  ch;
    int  lastCh      = '\n';
    bool sawAnyChar  = false;

    while ( ( ch = fgetc( sdCardFilePointer ) ) != EOF ) {
        sawAnyChar = true;
        if ( ch == '\n' ) {
            lineCount++;
        }
        lastCh = ch;
    }

    // Count a trailing partial line if the file doesn't end with '\n'
    if ( sawAnyChar && lastCh != '\n' ) {
        lineCount++;
    }

    fclose( sdCardFilePointer );
    return lineCount;
}

bool sdCardReadFileLine( const char* fileName, int lineIndex, char* readBuffer, int maxLen )
{
    if ( readBuffer == NULL || maxLen <= 0 || lineIndex < 0 ) {
        return false;
    }

    char fileNameSD[SD_CARD_FILENAME_MAX_LENGTH + 4] = "";
    strcat( fileNameSD, "/sd/" );
    strcat( fileNameSD, fileName );

    FILE *sdCardFilePointer = fopen( fileNameSD, "r" );
    if ( sdCardFilePointer == NULL ) {
        readBuffer[0] = '\0';
        return false;
    }

    int  currentLine = 0;
    int  bytesRead   = 0;
    int  ch;
    bool foundLine   = false;

    while ( ( ch = fgetc( sdCardFilePointer ) ) != EOF ) {
        if ( currentLine == lineIndex ) {
            if ( ch == '\n' ) {
                foundLine = true;
                break;
            }
            if ( ch != '\r' && bytesRead < maxLen - 1 ) {
                readBuffer[bytesRead++] = (char)ch;
            }
        } else if ( ch == '\n' ) {
            currentLine++;
        }
    }

    // Target was the last line and the file has no trailing newline
    if ( !foundLine && currentLine == lineIndex && bytesRead > 0 ) {
        foundLine = true;
    }

    readBuffer[bytesRead] = '\0';
    fclose( sdCardFilePointer );

    return foundLine;
}