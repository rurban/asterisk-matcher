/*
  * Asterisk -- A telephony toolkit for Linux.
  *
  * PBX matcher test.
  *
  * Written by Reini Urban <rurban at inode.at>,
  *
  * Most parts are in the public domain,
  * except code from Mark Spencer.
  */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/* derived from code by Steffen Offermann 1991 (public domain)
    http://www.cs.umu.se/~isak/Snippets/xstrcmp.c
*/
int ast_extension_patmatch(const char *pattern, const char *data)
{
     printf(" >>> %s =~ /%s/\n", data, pattern);
     switch (toupper(*pattern))
	{
	case '\0':
	    printf(" !>>> %s => %s\n", data, !*data ? "OK" : "FAIL");
	    return !*data;
	
	case ' ':
	case '-':
	    /* Ignore these characters in the pattern */
	    return *data && ast_extension_patmatch(pattern+1, data);

	case '.' : /* wildcard */
	    if (! *(pattern+1) )
		return *data;    /* abort earlier to speed it up */
	    else
		return ast_extension_patmatch(pattern+1, data) ||
                  (*data && ast_extension_patmatch(pattern, data+1));

/*	
	case '?' :
	    return *data && ast_extension_patmatch(pattern+1, data+1);
*/	
	case 'X':
	    return ((*data >= '0') && (*data <= '9')) && 
              ast_extension_patmatch(pattern+1, data+1);
	
	case 'Z':
	    return ((*data >= '1') && (*data <= '9')) && 
              ast_extension_patmatch(pattern+1, data+1);
	
	case 'N':
	    return ((*data >= '2') && (*data <= '9')) && 
              ast_extension_patmatch(pattern+1, data+1);
	
	case '[':
	    /* Begin Mark Spencer CODE */
	    {
		int i,border=0;
		char *where;
		int match=0;
		pattern++;
		where=strchr(pattern,']');
		if (where)
		    border=(int)(where-pattern);
		if (!where || border > strlen(pattern)) {
		    printf("Wrong [%s] pattern usage\n", pattern);
		    return 0;
		}
		for (i=0; i<border; i++) {
		    if (i+2<border) {
			if (*data==pattern[i])
			    return 1;
		        else if (pattern[i+1]=='-') {
			    if (*data >= pattern[i] && *data <= pattern[i+2]) {
				return ast_extension_patmatch(where+1, data+1);
			    } else {
				i+=2;
				continue;
			    }
			}
		    }
		}
		pattern+=border;
		break;
	    }
	    /* End Mark Spencer CODE */
	
	default  :
	    return (toupper(*pattern) == toupper(*data)) && 
              ast_extension_patmatch(pattern+1, data+1);
	}
}

#if 0
int ast_extension_match(char *pattern, char *data)
{
	int match;
	if (!*pattern) {
	    ast_log(LOG_WARNING, "ast_extension_match: empty pattern\n");
	    return 0;
	}
	if (!*data) {
	    ast_log(LOG_WARNING, "ast_extension_match: empty data\n");
	    return 0;
	}
	if (pattern[0] != '_') {
	    match = (strcmp(pattern, data) == 0);
	    ast_log(LOG_DEBUG, "ast_extension_match %s == /%s/", data, pattern);
	    return (strcmp(pattern, data) == 0);
	} else {
	    ast_log(LOG_DEBUG, "ast_extension_match %s =~ /%s/", data, pattern);
	    match = ast_extension_patmatch(data,pattern+1);
	}
	ast_log(LOG_DEBUG, " => %d\n", match);
	return match;
}
#endif

int testmatch(char *pattern, char *data, int should)
{
     int match;

     if (pattern[0] != '_') {
	match = (strcmp(pattern, data) == 0);
	printf("%s == /%s/ => %d", data, pattern, match);
     } else {
	match = ast_extension_patmatch(pattern+1,data);
	printf("%s =~ /%s/ => %d", data, pattern, match);
     }
     if (should == match) {
	printf(" OK\n");
     } else {
	printf(" FAIL\n");
	exit;
     }
     return match;
}

int main (int argc, char *argv[]) {
     char data1[] = "0316890002";
     char data2[] = "03168900028500";

     testmatch("0316890002",data1,1);
     testmatch("_0N.",data1,1);
     testmatch("_0N.0",data1,0);
     testmatch("_0N. 8500",data1,0);
     testmatch("_0N. 8500",data2,1);
     testmatch("_0[2-9]. 8500",data2,1);
     testmatch("_[a-z]o[0-9a-z].","voicemail",1);
}
