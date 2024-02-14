/*
    Network Next. Copyright © 2017 - 2024 Network Next, Inc.

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following
    conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
       and the following disclaimer in the documentation and/or other materials provided with the distribution.

    3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
       products derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
    OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "next_autodetect.h"

#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC || NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

#include "next_config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
#define strtok_r strtok_s
#endif

bool next_autodetect_google( char * output, size_t output_size )
{
    FILE * file;
    char buffer[1024*10];

    // are we running in google cloud?
#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    file = popen( "/bin/ls /usr/bin | grep google_ 2>/dev/null", "r" );
    if ( file == NULL )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: could not run ls" );
        return false;
    }

#elif NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    file = _popen( "dir \"C:\\Program Files (x86)\\Google\\Cloud SDK\\google-cloud-sdk\\bin\" | findstr gcloud", "r" );
    if ( file == NULL )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: could not run dir" );
        return false;
    }

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

    bool in_gcp = false;
    while ( fgets( buffer, sizeof(buffer), file ) != NULL )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: running in google cloud" );
        in_gcp = true;
        break;
    }

#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    pclose( file );

#elif NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    _pclose( file );

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

    // we are not running in google cloud :(

    if ( !in_gcp )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: not in google cloud" );
        return false;
    }

    // we are running in google cloud, which zone are we in?

    char zone[256];
    zone[0] = '\0';

#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    file = popen( "curl -s \"http://metadata.google.internal/computeMetadata/v1/instance/zone\" -H \"Metadata-Flavor: Google\" --max-time 10 -vs 2>/dev/null", "r" );
    if ( !file )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: could not run curl" );
        return false;
    }

#elif NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    file = _popen( "powershell Invoke-RestMethod -Uri http://metadata.google.internal/computeMetadata/v1/instance/zone -Headers @{'Metadata-Flavor' = 'Google'} -TimeoutSec 10", "r" );
    if ( !file )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: could not run powershell Invoke-RestMethod" );
        return false;
    }

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

    while ( fgets( buffer, sizeof(buffer), file ) != NULL )
    {
        size_t length = strlen( buffer );
        if ( length < 10 )
        {
            continue;
        }

        if ( buffer[0] != 'p' ||
             buffer[1] != 'r' ||
             buffer[2] != 'o' ||
             buffer[3] != 'j' ||
             buffer[4] != 'e' ||
             buffer[5] != 'c' ||
             buffer[6] != 't' ||
             buffer[7] != 's' ||
             buffer[8] != '/' )
        {
            continue;
        }

        bool found = false;
        size_t index = length - 1;
        while ( index > 10 && length  )
        {
            if ( buffer[index] == '/' )
            {
                found = true;
                break;
            }
            index--;
        }

        if ( !found )
        {
            continue;
        }

        next_copy_string( zone, buffer + index + 1, sizeof(zone) );

        size_t zone_length = strlen(zone);
        index = zone_length - 1;
        while ( index > 0 && ( zone[index] == '\n' || zone[index] == '\r' ) )
        {
            zone[index] = '\0';
            index--;
        }

        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: google zone is \"%s\"", zone );

        break;
    }

#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    pclose( file );

#elif NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    _pclose( file );

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

    // we couldn't work out which zone we are in :(

    if ( zone[0] == '\0' )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: could not detect google zone" );
        return false;
    }

    // look up google zone -> network next datacenter via mapping in google cloud storage "google.txt" file

#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    char cmd[1024];
    snprintf( cmd, sizeof(cmd), "curl -s \"https://storage.googleapis.com/%s/google.txt?ts=%x\" --max-time 10 -vs 2>/dev/null", NEXT_CONFIG_BUCKET_NAME, uint32_t(time(NULL)) );
    file = popen( cmd, "r" );
    if ( !file )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: could not run curl" );
        return false;
    }

#elif NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    char cmd[1024];
    snprintf( cmd, sizeof(cmd), "powershell Invoke-RestMethod -Uri \"https://storage.googleapis.com/%s/google.txt?ts=%x\" -TimeoutSec 10", NEXT_CONFIG_BUCKET_NAME, uint32_t(time(NULL)) );
    file = _popen( cmd, "r" );
    if ( !file )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: could not run powershell Invoke-RestMethod" );
        return false;
    }

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

    bool found = false;

    while ( fgets( buffer, sizeof(buffer), file ) != NULL )
    {
        const char * separators = ",\n\r";
        char * rest = buffer;
        char * google_zone = strtok_r( buffer, separators, &rest );
        if ( google_zone == NULL )
        {
            continue;
        }

        char * google_datacenter = strtok_r( NULL, separators, &rest );
        if ( google_datacenter == NULL )
        {
            continue;
        }

        if ( strcmp( zone, google_zone ) == 0 )
        {
            next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: \"%s\" -> \"%s\"", zone, google_datacenter );
            next_copy_string( output, google_datacenter, output_size );
            found = true;
            break;
        }
    }

#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    pclose( file );

#elif NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    _pclose( file );

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

    return found;
}

bool next_autodetect_amazon( char * output, size_t output_size )
{
    FILE * file;
    char buffer[1024*10];

    // Get the AZID from instance metadata
    // This is necessary because only AZ IDs are the same across different AWS accounts
    // See https://docs.aws.amazon.com/ram/latest/userguide/working-with-az-ids.html for details

    char azid[256];
    azid[0] = '\0';

#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    file = popen( "curl -s \"http://169.254.169.254/latest/meta-data/placement/availability-zone-id\" --max-time 2 -vs 2>/dev/null", "r" );
    if ( !file )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: could not run curl" );
        return false;
    }

#elif NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    file = _popen ( "powershell Invoke-RestMethod -Uri http://169.254.169.254/latest/meta-data/placement/availability-zone-id -TimeoutSec 2", "r" );
    if ( !file )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: could not run powershell Invoke-RestMethod" );
        return false;
    }

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

    while ( fgets( buffer, sizeof(buffer), file ) != NULL )
    {
        if ( strstr( buffer, "-az" ) == NULL )
        {
            continue;
        }

        next_copy_string( azid, buffer, sizeof(azid) );

        size_t azid_length = strlen( azid );
        size_t index = azid_length - 1;
        while ( index > 0 && ( azid[index] == '\n' || azid[index] == '\r' ) )
        {
            azid[index] = '\0';
            index--;
        }

        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: azid is \"%s\"", azid );

        break;
    }

#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    pclose( file );

#elif NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    _pclose( file );

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

    // we are probably not in AWS :(

    if ( azid[0] == '\0' )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: not in AWS" );
        return false;
    }

    // look up AZID -> network next datacenter via mapping in google cloud storage "amazon.txt" file

#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    char cmd[1024];
    snprintf( cmd, sizeof(cmd), "curl -s \"https://storage.googleapis.com/%s/amazon.txt?ts=%x\" --max-time 10 -vs 2>/dev/null", NEXT_CONFIG_BUCKET_NAME, uint32_t(time(NULL)) );
    file = popen( cmd, "r" );
    if ( !file )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: could not run curl" );
        return false;
    }

#elif NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    char cmd[1024];
    snprintf( cmd, sizeof(cmd), "powershell Invoke-RestMethod -Uri \"https://storage.googleapis.com/%s/amazon.txt?ts=%x\" -TimeoutSec 10", NEXT_CONFIG_BUCKET_NAME, uint32_t(time(NULL)) );
    file = _popen ( cmd, "r" );
    if ( !file )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: could not run powershell Invoke-RestMethod" );
        return false;
    }

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

    bool found = false;

    while ( fgets( buffer, sizeof(buffer), file ) != NULL )
    {
        const char * separators = ",\n\r";
        char* rest = buffer;
        char * amazon_zone = strtok_r( buffer, separators, &rest );
        if ( amazon_zone == NULL )
        {
            continue;
        }

        char * amazon_datacenter = strtok_r( NULL, separators, &rest );
        if ( amazon_datacenter == NULL )
        {
            continue;
        }

        if ( strcmp( azid, amazon_zone ) == 0 )
        {
            next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: \"%s\" -> \"%s\"", azid, amazon_datacenter );
            next_copy_string( output, amazon_datacenter, output_size );
            found = true;
            break;
        }
    }

#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    pclose( file );

#elif NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    _pclose( file );

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

    return found;
}

// --------------------------------------------------------------------------------------------------------------

#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define ANICHOST        "whois.arin.net"
#define LNICHOST        "whois.lacnic.net"
#define RNICHOST        "whois.ripe.net"
#define PNICHOST        "whois.apnic.net"
#define BNICHOST        "whois.registro.br"
#define AFRINICHOST     "whois.afrinic.net"

const char *ip_whois[] = { LNICHOST, RNICHOST, PNICHOST, BNICHOST, AFRINICHOST, NULL };

bool next_whois( const char * address, const char * hostname, int recurse, char ** buffer, size_t & bytes_remaining )
{
    struct addrinfo *hostres, *res;
    char *nhost;
    int i, s;
    size_t c;

    struct addrinfo hints;
    int error;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = 0;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    error = getaddrinfo(hostname, "nicname", &hints, &hostres);
    if ( error != 0 )
    {
        return 0;
    }

    for (res = hostres; res; res = res->ai_next) {
        s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (s < 0)
            continue;
        if (connect(s, res->ai_addr, res->ai_addrlen) == 0)
            break;
        close(s);
    }

    freeaddrinfo(hostres);

    if (res == NULL)
        return 0;

    FILE * sfi = fdopen( s, "r" );
    FILE * sfo = fdopen( s, "w" );
    if ( sfi == NULL || sfo == NULL )
        return 0;

    if (strcmp(hostname, "de.whois-servers.net") == 0) {
#ifdef __APPLE__
        fprintf(sfo, "-T dn -C UTF-8 %s\r\n", address);
#else
        fprintf(sfo, "-T dn,ace -C US-ASCII %s\r\n", address);
#endif
    } else {
        fprintf(sfo, "%s\r\n", address);
    }
    fflush(sfo);

    nhost = NULL;

    char buf[10*1024];

    while ( fgets(buf, sizeof(buf), sfi) )
    {
        size_t len = strlen(buf);

        if ( len < bytes_remaining )
        {
            memcpy( *buffer, buf, len );
            bytes_remaining -= len;
            *buffer += len;
        }

        if ( nhost == NULL )
        {
            if ( recurse && strcmp(hostname, ANICHOST) == 0 )
            {
                for (c = 0; c <= len; c++)
                {
                    buf[c] = tolower((int)buf[c]);
                }
                for (i = 0; ip_whois[i] != NULL; i++)
                {
                    if (strstr(buf, ip_whois[i]) != NULL)
                    {
                        int result = asprintf( &nhost, "%s", ip_whois[i] );  // note: nhost is allocated here
                        if ( result == -1 )
                        {
                            nhost = NULL;
                        }
                        break;
                    }
                }
            }
        }
    }

    close( s );
    fclose( sfo );
    fclose( sfi );

    bool result = true;

    if ( nhost != NULL)
    {
        result = next_whois( address, nhost, 0, buffer, bytes_remaining );
        free( nhost );
    }

    return result;
}

bool next_autodetect_multiplay( const char * input_datacenter, const char * address, char * output, size_t output_size )
{
    FILE * file;

    // are we in a multiplay datacenter?

    if ( strlen( input_datacenter ) <= 10 ||
         input_datacenter[0] != 'm' || 
         input_datacenter[1] != 'u' || 
         input_datacenter[2] != 'l' || 
         input_datacenter[3] != 't' || 
         input_datacenter[4] != 'i' || 
         input_datacenter[5] != 'p' || 
         input_datacenter[6] != 'l' || 
         input_datacenter[7] != 'a' || 
         input_datacenter[8] != 'y' || 
         input_datacenter[9] != '.' )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: not in multiplay" );
        return false;
    }

    // is this a non-autodetect multiplay datacenter? ("multiplay.[cityname].[number]")

    const int length = strlen( input_datacenter );

    int num_periods = 0;

    for ( int i = 0; i < length; ++i )
    {
        if ( input_datacenter[i] == '.' )
        {
            num_periods++;
            if ( num_periods > 1 )
            {
                strcpy( output, input_datacenter );
                return true;
            }
        }
    }

    // capture the city name using form multiplay.[cityname]

    const char * city = input_datacenter + 10;

    // try to read in cache of whois in whois.txt first

    bool have_cached_whois = false;
    char whois_buffer[1024*64];
    memset( whois_buffer, 0, sizeof(whois_buffer) );
    FILE * f = fopen( "whois.txt", "r");
    if ( f )
    {
        fseek( f, 0, SEEK_END );
        size_t fsize = ftell( f );
        fseek( f, 0, SEEK_SET );
        if ( fsize > sizeof(whois_buffer) - 1 )
        {
            fsize = sizeof(whois_buffer) - 1;
        }
        if ( fread( whois_buffer, fsize, 1, f ) == 1 )
        {
            next_printf( NEXT_LOG_LEVEL_INFO, "server successfully read cached whois.txt" );
            have_cached_whois = true;
        }
        fclose( f );
    }

    // if we couldn't read whois.txt, run whois locally and store the result to whois.txt

    if ( !have_cached_whois )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server running whois locally" );
        char * whois_output = &whois_buffer[0];
        size_t bytes_remaining = sizeof(whois_buffer) - 1;
        next_whois( address, ANICHOST, 1, &whois_output, bytes_remaining );
           FILE * f = fopen( "whois.txt", "w" );
           if ( f )
           {
               next_printf( NEXT_LOG_LEVEL_INFO, "server cached whois result to whois.txt" );
               fputs( whois_buffer, f );
               fflush( f );
               fclose( f );
           }
    }

    // check against multiplay seller mappings

    bool found = false;
    char multiplay_line[1024];
    char multiplay_buffer[64*1024];
    multiplay_buffer[0] = '\0';
    char cmd[1024];
    snprintf( cmd, sizeof(cmd), "curl -s \"https://storage.googleapis.com/%s/multiplay.txt?ts=%x\" --max-time 10 -vs 2>/dev/null", NEXT_CONFIG_BUCKET_NAME, uint32_t(time(NULL)) );
    file = popen( cmd, "r" );
    if ( !file )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: could not run curl" );
        return false;
    }
    while ( fgets( multiplay_line, sizeof(multiplay_line), file ) != NULL ) 
    {
        strcat( multiplay_buffer, multiplay_line );

        if ( found )
            continue;

        const char * separators = ",\n\r\n";

        char * substring = strtok( multiplay_line, separators );
        if ( substring == NULL )
        {
            continue;
        }

        char * seller = strtok( NULL, separators );
        if ( seller == NULL )
        {
            continue;
        }

        next_printf( NEXT_LOG_LEVEL_DEBUG, "checking for seller \"%s\" with substring \"%s\"", seller, substring );

        if ( strstr( whois_buffer, substring ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "found seller %s", seller );
            snprintf( output, output_size, "%s.%s", seller, city );
            found = true;
        }
    }
    pclose( file );

    // could not autodetect multiplay :(

    if ( !found )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "could not autodetect multiplay datacenter :(" );
        next_printf( NEXT_LOG_LEVEL_NONE, "-------------------------\n%s-------------------------\n", multiplay_buffer );
        const char * separators = "\n\r\n";
        char * line = strtok( whois_buffer, separators );
        while ( line )
        {
            next_printf( NEXT_LOG_LEVEL_NONE, "%s", line );
            line = strtok( NULL, separators );
        }
        next_printf( NEXT_LOG_LEVEL_NONE, "-------------------------\n" );
        return false;
    }

    return found;
}

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

bool next_autodetect_datacenter( const char * input_datacenter, const char * public_address, char * output, size_t output_size )
{
#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC
    
    // we need curl to do any autodetect. bail if we don't have it

    next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: looking for curl" );

    int result = system( "curl -s >/dev/null 2>&1" );

    if ( result < 0 )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: curl not found" );
        return false;
    }

    next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: curl exists" );

#elif NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    // we need access to powershell and Invoke-RestMethod to do any autodetect. bail if we don't have it

    next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: looking for powershell Invoke-RestMethod" );

    int result = system( "powershell Invoke-RestMethod -? > NUL 2>&1" );

    if ( result > 0 )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: powershell Invoke-RestMethod not found" );
        return false;
    }

    next_printf( NEXT_LOG_LEVEL_INFO, "server autodetect datacenter: powershell Invoke-RestMethod exists" );

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

    // google cloud

    bool google_result = next_autodetect_google( output, output_size );
    if ( google_result )
    {
        return true;
    }

    // amazon

    bool amazon_result = next_autodetect_amazon( output, output_size );
    if ( amazon_result )
    {
        return true;
    }

    // multiplay

#if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    bool multiplay_result = next_autodetect_multiplay( input_datacenter, public_address, output, output_size );
    if ( multiplay_result )
    {
        return true;
    }

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC

    (void) input_datacenter;
    (void) public_address;
    (void) output_size;

    return false;
}

#else // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC || NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS

int next_autodetect_dummy;

#endif // #if NEXT_PLATFORM == NEXT_PLATFORM_LINUX || NEXT_PLATFORM == NEXT_PLATFORM_MAC || NEXT_PLATFORM == NEXT_PLATFORM_WINDOWS
