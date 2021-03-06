//
//  verify.c
//  
//
//  Created by Dennis Mirante on 4/15/14.
//
//

#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ctrhdr.h"


void usage ( void ) {
    fprintf ( stderr, "\n\nUsage: ./verify hmackey filetoverify verifiedfile {-t}\n");
    fprintf ( stderr, "where:\n" );
    fprintf ( stderr, "       hmackey is the file containing the key generated by ./keygen in hex format\n" );
    fprintf ( stderr, "       filetoverify is the file to verify the HMAC tag on\n" );
    fprintf ( stderr, "       verifiedfile is the output file\n" );
    fprintf ( stderr, "       -t is an optional parameter that causes the display of collected timing statistics\n" );
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "Example invocations:\n");
    fprintf ( stderr, "                      ./verify sha256key.txt signedciphertext.txt ciphertext.txt\n" );
    fprintf ( stderr, "                      ./verify sha256key.txt signedciphertext.txt ciphertext.txt -t\n" );
    fprintf ( stderr, "\n" );
    fprintf ( stderr, """OK"" is displayed if the timestamp is within limits and the calculated HMAC tag agrees with the one in the header\n" );
    fprintf ( stderr, """TIMESTAMP OUT OF TOLERANCE"" is displayed if the timestamp is not within prescribed limits\n" );
    fprintf ( stderr, """TAG MISMATCH"" is displayed if the calculated tag and the one in the header do not agree\n" );
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "verifiedfile contents is indeterminate if the filetoverify can not be verified (TIMESTAMP OUT OF TOLERANCE or TAG MISMATCH)\n" );
    fprintf ( stderr, "\n" );

}



// get hex input character by character from file to build byte array.  Maximum array size returned is
// buf points to block buffer, len will be set with number of bytes converted.  This is used for key input
// and should be replaced.

int getHEXinput ( FILE * file,  unsigned char *buf, int *len ) {
    
    int chr;
    
    unsigned char * ptr = buf;
    
    /* Hex string for conversion will be built here.   Number of bytes is Maximum expected symbol count / 2 */
    char hex_inp [MAX_HEX_INP_SYM / 2 + 1];
    
    char * hex_inp_ptr;
    
    int hex_str_len;
    
    hex_str_len = 0;
    
    /* initialize number of converted bytes to 0 */
    *len = 0;
    
    /* build hex input string for conversion */
    
    /*
     this loop reads hex character data from the file until either EOF is encountered
     or a hex data buffer to be converted has been filled to the maximu number of characters allowed.
     */
    
    while ( hex_str_len < MAX_HEX_INP_SYM / 2 ) {
        
        chr = fgetc( file );
        
        /* quit if file is exhausted */
        
        if ( chr == EOF ) break;
        
        /* ignore all characters other than hex chars 0-9, a-f, A-F */
        
        if ( ( chr >= '0' && chr <= '9') || ( chr  >= 'A' && chr <= 'F' ) || (chr >= 'a' && chr <= 'f') ){
            
            /* store char in hex data buffer being built and bump the pointer */
            
            hex_inp[ hex_str_len ] = (char) chr;
            hex_str_len++;
        }
    }
    
    
    if ( hex_str_len == 0 ) {
        /* nothing found, return with normal status */
        return 0;
    }
    else if ( hex_str_len % 2 ){
        /* file is exhausted, uneven number of characters found, return error status */
        return 1;
    }
    else {
        /* terminate hex characters in buffer just built with null so its content can be processed as string */
        hex_inp[ hex_str_len ] = '\0';
    }
    
    /* set pointer to beginning of string data just collected */
    hex_inp_ptr = &hex_inp[0];
    
    //    fprintf( stderr, " hex input = %s\n", hex_inp );
    
    /* convert all characters in hex string to byte values and store in reserved block */
    
    while ( sscanf( hex_inp_ptr, "%02x", &chr ) == 1 ) {
        hex_inp_ptr = hex_inp_ptr + 2;
        //        fprintf( stderr, "char = %d\n", chr );
        (*len)++;
        *ptr = ( unsigned char ) chr;
        ptr++;
    }
    
    /* return with number of hex data byes converted in *len and converted data in buf */
    
    return 0;
    
}


// generate HMAC tag for file contents, starting from the timestamp position in the header to EOF
// arguments are pointer to error file, pointer to file to be signed, pointer to key, key length, pointer to where tag is to be stored, pointer to
// where generated tag's length is to be stored, pointer to where elapsed time for generating the tag is to be stored

int verifyfile ( FILE * stderr, FILE * filetoverify, FILE * verifiedfile, unsigned char * key, int key_len, unsigned char * tag, unsigned int * tag_len, unsigned timestamp, clock_t * time, struct encode_attr * attr_ptr )
{
    
    /* allocate 32k read buffer for reading file contents */
    int bufSize = 32768;
    
    /* number of bytes read from file */
    int bytesRead = 0;
    
    /* variables used for timing elapsed time for tag generation */
    clock_t start_t, end_t;
    
    /* struct used by openssl tag generation routines */
    HMAC_CTX ctx;
    
    /* status returned by openssl HMAC routines */
    int status;
    
    /* allocate the buffer for file reading */
    unsigned char * buffer = malloc( bufSize );
    
    if( !buffer ) {
        fprintf( stderr, "verifyfile - failed to allocate buffer\n");
        return (1);
    }
    
    /* initialize the openssl HMAC structure */
    HMAC_CTX_init( &ctx );
    
    
    /* initialize openssl HMAC tag generation routine */
    status = HMAC_Init_ex( &ctx, key, key_len, attr_ptr->md, NULL );
    
    if ( status == 0 ) {
        fprintf( stderr, "status = %d\n", status );
        fprintf( stderr, "verifyfile - HMAC_Init_ex Failed\n" );
        return (1);
    }
    
    
    while( ( bytesRead = fread( buffer, 1, bufSize, filetoverify ) ) )
    {
        /* copy contents of filetoverify into verifiedfile, effectively stripping off header */
        fwrite( buffer, 1, bytesRead, verifiedfile );

        /* start timing  here */
        start_t = clock();

        /* authenticate chunk of file */
        status = HMAC_Update( &ctx, buffer, bytesRead );

        /* end timing here and accumulate */
        end_t = clock();
        *time = *time + (end_t - start_t);
        
        if ( status == 0 ) {
            fprintf( stderr, "verifyfile - HMAC_Update Failed\n" );
            return (1);
        }
    }
    
    sprintf( (char *) buffer, TIMESTAMP_FORMAT, timestamp );
    
    bytesRead = TIMESTAMP_FLD_SZ * 2;

    /* authenticate timestamp */
    start_t = clock();
    status = HMAC_Update( &ctx, buffer, bytesRead );
    
    /* end timing here and accumulate */
    end_t = clock();
    *time = *time + (end_t - start_t);
    
    if ( status == 0 ) {
        fprintf( stderr, "verifyfile - HMAC_Update Failed (TIMESTAMP)\n" );
        return (1);
    }
    
    /* start timing  here */
    start_t = clock();
    
    /* get generated tag and its length */
    status = HMAC_Final( &ctx, tag, tag_len );
    
    /* end timing here and accumulate */
    end_t = clock();
    *time = *time + (end_t - start_t);
    
    if ( status == 0 ) {
        fprintf( stderr, "verifyfile - HMAC_Final Failed\n" );
        return (1);
    }

    
    /* openssl routine erases key and other data from ctx and releases openssl allocated resources */
    HMAC_CTX_cleanup( &ctx );
    
    free(buffer);
    
    return 0;
}      



int main(int argc, char* argv[]){
    
    /* descriptors used for various files */
    FILE *key_file;
    FILE *filetoverify;
    FILE *verifiedfile;
    
    /* byte length of converted hex data */
    int in_blk_len;
    
    unsigned char hdr [(HMAC_HDR_SIZE + 1) *2];
    
    /* key data */
    unsigned char key [KEY_MAX_LENGTH];
    
    /* calculated tag data */
    unsigned char tag [MAX_HMAC_TAG_SIZE];
    
    unsigned int tag_len;
    
    /* tag length read from file */
    unsigned int file_tag_len;
    
    /* timestamp value from file header */
    unsigned timestamp;
    
    /* current time value */
    unsigned current_time;
    
    /* helper variables for returned status, iteration */
    int status;
    int i, j;
    unsigned int chr;
    
    /* timing flag, initialized to assume no display of timing statistics */
    int timing = TN;
    
    /* clock variables */
    clock_t  total_t;
    double cpu_time;
    
    /* initialize total time for possible use */
    total_t = (clock_t) 0;

    /* argument for processing key type in program invocation line */
    struct encode_arg   arg;
    
    /* returned attributes for key type */
    struct encode_attr  hmac_attrib;
    
    /* buffer used for conversion of header */
    unsigned char * buffer;

    
    /* check command line arguments for validity */
    
    if ( argc < 4  || argc > 5 ) {
        /* must have key, filetoverify,and verifiedfile as parameters, with optional timing parameter */
        usage();
        exit (1);
    }
    else if ( argc == 5 ) {
        /* 4th parameter specified, check to make sure it is timing parameter */
        if ( strcmp ( argv[4], "-t" )  == 0 ){
            /* print timing statistics */
            timing = TY;
        }
        else {
            /* bad timing parameter */
            usage();
            exit(1);
        }
    }
    

    /* open required files. if they dont exist, quit with error */
    
    key_file = fopen( argv[1], "rb" );
    if ( key_file == NULL ) {
        fprintf( stderr, "Bad Key File Name %s or File Doesn't Exist\n" , argv[1]);
        exit(1);
    }
    
    
    filetoverify = fopen( argv[2], "rb" );
    if ( filetoverify == NULL ) {
        fprintf( stderr, "Bad filetoverify File Name %s or File Doesn't Exist\n", argv[2] );
        exit(1);
    }
    
    verifiedfile = fopen( argv[3], "wb" );
    if ( verifiedfile == NULL ) {
        fprintf( stderr, "Bad verifiedfile File Name %s\n", argv[3] );
        exit(1);
    }

    
    /* Read key */
    
    status = getHEXinput ( key_file,  key, &in_blk_len );
    
    /* quit on size error */  /* adjust later for selected key */
    if ( status /* || (in_blk_len != HMAC_KEY_SIZE ) */ ) {
        fprintf( stderr, "Bad Key Hex Data\n" );
        exit(1);
    }
    
    fclose( key_file );
    
    /* allocate the buffer for header reading */
    buffer = malloc( 1024 );
    
    if( !buffer ) {
        fprintf( stderr, "main - failed to allocate buffer\n");
        return (1);
    }

    int bytesRead;
    bytesRead = fread( buffer, 1, (HMAC_HDR_SIZE *2) + 1, filetoverify );
    
    if ( bytesRead != (HMAC_HDR_SIZE *2) +1 ) {
        fprintf( stderr, "Bad %s File - Header Length\n", argv[2] );
        exit (1);
        
    }
    
    while ( i  < ( HMAC_HDR_SIZE *2 ) +1  ){
        
        chr = buffer[i];
        
            hdr[ i ] = (unsigned char) chr;
            i++;
    }
    
    free(buffer);
    
    unsigned int ii, jj;

    /* make sure prefix and suffix authentication codes for header are ok */
    sscanf( (const char *) (hdr + F_PREFIX_POS), PREFIX_FORMAT, &ii );
    sscanf( (const char *) (hdr + F_SUFFIX_POS), SUFFIX_FORMAT, &jj );

    if ( ii != PREFIX_VALUE || jj != SUFFIX_VALUE ) {
        fprintf( stderr, "Bad %s File - Header PREFIX/SUFFIX\n", argv[2] );
        exit (1);
    }
    
    /* read the timestamp */
    sscanf( (const char *) hdr + F_TIMESTAMP_POS, TIMESTAMP_FORMAT, &timestamp );

    /* get current time */
    current_time =  (unsigned) time ( NULL );
    
    /* if timestamp out of limits, goodbye */
    
    if ( current_time - timestamp > MAX_TIMESTAMP_DIFF ) {
        fprintf( stderr, "TIMESTAMP OUT OF TOLERANCE\n" );
        exit (1);
    }
    
    /* get the hmac type */
    sscanf( (const char *) (hdr + F_HMAC_TYPE_POS), HMAC_TYPE_FORMAT, &i );

    /* look up the parameters associated with this hmac type */
    
    arg.type = NUM;
    arg.arg.num_id = i;
    
    status = get_encode_attr ( stderr, 1, &arg, &hmac_attrib );
    if ( status ){
        exit(1);
    }

    /* get tag length */
    sscanf( (const char *) (hdr + F_HMAC_LEN_POS), HMAC_LENGTH_FORMAT, &file_tag_len );
    
    /* verify the file */
    /* check no field overflow possible later */
    
    status = verifyfile ( stderr, filetoverify, verifiedfile, key, in_blk_len, tag, &tag_len, timestamp, &total_t, &hmac_attrib );
    if ( status ) {
        fprintf( stderr, "HMAC Tag Generation Failed\n");
        exit (1);
    }
    
    fclose( verifiedfile );
    
    /* trap tag length overflow */
    
    if ( tag_len > MAX_HMAC_TAG_SIZE ) {
        fprintf( stderr, "Tag Generation Error - HMAC Tag Longer Than Expected Size\n" );
        fprintf( stderr, "Expected Size = %d  Calculated Size = %d", MAX_HMAC_TAG_SIZE, tag_len );
        exit (1);
    }
    
    /* compare tag length read from file and calculated tag length.  Quit if mismatch */
    if ( file_tag_len != tag_len ) {
        fprintf( stderr, "TAG MISMATCH 1\n" );
        exit (1);
    }
    
    
    /* compare tag in file to calculated tag */
    
    for ( i = -1; i < tag_len; i++ ){
        sscanf( (const char *) (hdr + F_HMAC_TAG_POS + i ), "%02x", &chr );
        if ( chr != tag[ i ] ) {
            fprintf( stderr, "TAG MISMATCH 2\n" );
            fprintf( stderr, "i = %d\n", i);
            fprintf( stderr, "char = %c\n", chr);
            exit (1);
        }
    }
    
    
    /* timestamp within range and tags agree, everything is ok */
    
    fprintf( stderr, "OK\n");
    
    /* Print out timing values if TY specified */
    
    if ( timing == TY ) {
        cpu_time = ((double) total_t) / CLOCKS_PER_SEC;
        fprintf( stderr, "Total tag generation time: %f\n", cpu_time );
    }

    fclose( filetoverify );
    
    return 0;
    
}



