/*
 * Asterisk -- A telephony toolkit for Linux.
 *
 * PBX regex matcher test.
 *
 * Written by Reini Urban <rurban@inode.at>
 * Given to the public domain.
 */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h> 

/* this is just for standalone testing */
#include <stdarg.h>
#define LOG_WARNING 3
#define LOG_DEBUG   0

extern void ast_log(int level, const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fflush(stdout);
}
/* end of standalone test functions */

int  patmatch_groupcounter = 0;
char patmatch_group[80] = "";

/* derived from code by Steffen Offermann 1991 (public domain)
   http://www.cs.umu.se/~isak/Snippets/xstrcmp.c
*/
int ast_extension_patmatch(const char *pattern, char *data) 
{
    int i,border=0;
    char *where;
    static char prev = '\0';
    static char groupdata[80] = "";
    static char *group = patmatch_group;
    int groupcounter = patmatch_groupcounter;

    ast_log(LOG_DEBUG, " >>> \"%s\" =~ /%s/\n", data, pattern);
    switch (toupper(*pattern))
	{
	case '\0':
	    ast_log(LOG_DEBUG, " !>>> \"%s\" => %s\n", data, !*data ? "OK" : "FAIL");
	    return !*data;
	    
	case ' ':
	case '-':
	    /* Ignore these characters in the pattern */
	    return *data && ast_extension_patmatch(pattern+1, data);

	case '.' : /* wildcard as '*' in glob(). Match any sequence of characters. 0 or more */
	    prev = *pattern;
	    if (! *(pattern+1) ) 
		return 1; /* return *data; => match one or more */
	    else
		return ast_extension_patmatch(pattern+1, data) || (*data && ast_extension_patmatch(pattern, data+1));

	/* wildcard character: Match any char */
	case '?' :
	    prev = *pattern;
	    return *data && ast_extension_patmatch(pattern+1, data+1);

	case 'X': /* 0-9 */
	    prev = *pattern;
	    return ((*data >= '0') && (*data <= '9')) && ast_extension_patmatch(pattern+1, data+1);
	    
	case 'Z': /* 1-9 */
	    prev = *pattern;
	    return ((*data >= '1') && (*data <= '9')) && ast_extension_patmatch(pattern+1, data+1);
	    
	case 'N': /* 2-9 */
	    prev = *pattern;
	    return ((*data >= '2') && (*data <= '9')) && ast_extension_patmatch(pattern+1, data+1);

	case '{': /* quantifier {n[,m]} */
	  {
	    char *comma;
	    int cpos;
	    where=strchr(pattern,'}');
	    if (where) {
		border=(int)(where-pattern);
		comma = strchr(pattern,',');
	    }
	    if (!where || border > strlen(pattern)) {
		ast_log(LOG_WARNING, "Wrong %s pattern usage\n", pattern);
		return 0;
	    } else {
		char tmp[8];
		int from, to;
		if (comma)
		    cpos = (int)(comma-pattern);
		else 
		    cpos = border;
		strncpy(tmp,pattern+1,cpos-1);
		tmp[cpos-1] = '\0';
		from = atoi(tmp);
		if (comma) {
		    if (border-cpos > 1) { /* {f,t} */
			strncpy(tmp,comma+1,border-cpos);
			tmp[border-cpos+1] = '\0';
			to = atoi(tmp);
		    } else { /* {f,} */
			to = strlen(data); /* may fail if after the group are more pattern chars */
			if (*(pattern+border+1)) {
			    to = to - strlen(pattern+border+1) + 1;
			}
		    }
		} else {     /* {f} */
		    if (from == 0) {
			ast_log(LOG_WARNING, "Invalid {0} pattern quantifier %s\n", pattern);
			return 0;
		    }
		    to = from;
		}
		if (from < 0 || to <= 0 || to < from) {
		    ast_log(LOG_WARNING, "Invalid pattern quantifier %s\n", pattern);
		    return 0;
		}

		if (*group) { 	/* check for repeated pattern{n,m} in previous group */
		    int i;
		    for (i=0; i< strlen(group); i++) {
			data--;
		    }
		    ast_log(LOG_DEBUG, ">>> check for repeated pattern{%d,%d} of group '%s' in data '%s'\n", from, to, group, data);
		    strcat(group,".");
		} else {
		    ast_log(LOG_DEBUG, ">>> check for repeated pattern{%d,%d} in previous character '%c'\n", from, to, prev);
		    data--;
		    group[0] = prev;
		    group[1] = '.';
		    group[2] = '\0';
		}
		*tmp = prev;
		for (i=to; i>=from; i--) {
		    if (ast_extension_patmatch_repeated(group,data,i)) break;
		}
		prev = *tmp;
		if (i >= from || !from) { /* if found */
		    ast_log(LOG_DEBUG, " >>>> found '%s' in data '%s' after %d runs\n", group, data, i);
		    char name[16];
		    data = data + (i * (strlen(group)- 1)) - 1;
		    int l = strlen(groupdata) - strlen(data);
		    /* data = data-i+from-1; */		/* possible failure here! */
		    if (prev == ')') {			/* grouping => capture */
			*(group+strlen(group)-1) = '\0';
			groupdata[l+1] = '\0';
			ast_log(LOG_DEBUG, "  >>>>> end of group '%s', data: %s\n", group, groupdata);
			/* capture the found data in variables $1, $2, ... */
			sprintf(name,"%d",++groupcounter);
#ifdef AST_PBX_MAX_STACK
			pbx_builtin_setvar_helper(NULL,name,groupdata);
#endif
			ast_log(LOG_DEBUG, "  >>>>> global variable $%s set to '%s'\n", name, groupdata);
		    }
		}
		*group = '\0';
		prev = '\0';
		if (i >= from) {        /* found: continue */
		    ast_log(LOG_DEBUG, " >>>> found in round %d from %d\n", i, to);
		    if (*data) {
			if (*(pattern+border+1)) /* if the tail check fails, try the other rounds */
			    if (ast_extension_patmatch(pattern+border+1, data+1))
				return 1;
			    else return (ast_extension_patmatch_repeated(group, data, i--) && 
					 ast_extension_patmatch(pattern+border+1, data+i));
			else
			    return ast_extension_patmatch(pattern+border+1, data+1);
		    }
		    else 
			return 1;
		} else if (from == 0) { /* not found, but special case from=0: no match needed */
		    ast_log(LOG_DEBUG, " >>>> not found, but no match needed and data exhausted\n");
		    if (*data)
			return ast_extension_patmatch(pattern+border+1, data+1);
		    else 
			return 1;
		} else                /* not found */
		    return 0;
	    }
	  }
	  /* unreachable code */
	    
	case '(': /* grouping */
	    prev = *pattern;
	    if (*group) {
		ast_log(LOG_WARNING, "Unexpected subgroup ( in pattern %s\n", pattern);
		return 0;
	    }
	    where=strchr(pattern,')');
	    if (where)
		border=(int)(where-pattern);
	    if (!where || border > strlen(pattern)) {
		ast_log(LOG_WARNING, "Wrong (%s) pattern usage\n", pattern);
		return 0;
	    }
	    strncpy(group,pattern+1,border-1);
	    group[border-1] = '\0';
	    strcpy(groupdata,data);
	    ast_log(LOG_DEBUG, ">>> group '%s' stored, data: '%s'\n", group, groupdata);
	    if (strchr(pattern,'|')) { /* alternations */
		char *s, *scopy, *sep, *sepcopy;
		s = scopy = (char *) malloc(strlen(pattern));
		sepcopy   = (char *) malloc(strlen(pattern));
		strcpy(s,group);
		while (sep = strsep(&s,"|")) {
		    strcpy(sepcopy,sep);
		    strcat(sepcopy,pattern+border+1);
		    ast_log(LOG_DEBUG, "  >>>> alternative '%s' =~ /%s/\n", sepcopy, data);
		    if (ast_extension_patmatch(sepcopy, data)) break;
		    if (!*data) { 
			sep = NULL; break; 
		    }
		}
		free(scopy);
		if (sep) { /* found */
		    free(sepcopy);
		    return 1;
		} else {
		    free(sepcopy);
		    return 0;
		}
	    } else {
		return ast_extension_patmatch(pattern+1, data);
	    }

	case ')': /* group end */
	    prev = *pattern;
	    if (!*group) {
		ast_log(LOG_WARNING, "Unexpected ) in pattern %s\n", pattern);
		return 0;
	    } else {
		if (pattern[1] != '{') { /* capture without quantifiers */
		    char name[16];
		    int l = strlen(groupdata) - strlen(data);
		    groupdata[l-1] = '\0';
		    *(group+strlen(group)-1) = '\0';
		    ast_log(LOG_DEBUG, ">>> end of group '%s', data: %s\n", group, groupdata);
		    /* capture the found data in variables $1, $2, ... */
		    sprintf(name,"%d",++groupcounter);
#ifdef AST_PBX_MAX_STACK
		    pbx_builtin_setvar_helper(NULL,name,groupdata);
#endif
		    ast_log(LOG_DEBUG, ">>> global variable $%s set to '%s'\n", name, groupdata);
		    *group = '\0';
		}
	    }
	    return ast_extension_patmatch(pattern+1, data);

	case '|': /* alternation */
	    if (!*group) {
		ast_log(LOG_WARNING, "Need group for | in %s\n", pattern);
		return 0;
	    }
#if 0
	    {
		char *s, *sep;
		s = strdup(group);
		while (sep = strsep(&s,"|")) {
		    ast_log(LOG_DEBUG, "  >>>> alternative %d\n", sep);
		    if (ast_extension_patmatch(sep, data)) break;
		    if (!*data) { 
			*sep = '\0'; break; 
		    }
		}
		if (*sep) { /* found */
		    free(s);
		    return ast_extension_patmatch(pattern+1, data+1);
		} else {
		    free(s);
		    return 0;
		}
	    }
#endif

	case '[': /* Character ranges: [0-9a-zA-Z] */
	    prev = *pattern;
	    pattern++;
	    where=strchr(pattern,']');
	    if (where)
		border=(int)(where-pattern);
	    if (!where || border > strlen(pattern)) {
		ast_log(LOG_WARNING, "Wrong [%s] pattern usage\n", pattern);
		return 0;
	    }
	    if (*pattern == '^') { /* Negation like [^...] */
		for (i=1; i<border; i++) {
		    if (*data==pattern[i])
			return 0;
		    else if ((pattern[i+1]=='-') && (i+2<border)) {
			if (*data >= pattern[i] && *data <= pattern[i+2]) {
			    return 0;
			} else {
			    i+=2;
			    continue;
			}
		    }
		}
		return ast_extension_patmatch(where+1, data+1);
	    } else {
		for (i=0; i<border; i++) {
		    if (i+2<border) {
			if (*data==pattern[i])
			    return ast_extension_patmatch(where+1, data+1);
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
	    }
	    break;
	    
	default  :
	    prev = *pattern;
	    return (toupper(*pattern) == toupper(*data)) && ast_extension_patmatch(pattern+1, data+1);
	}
    return 0;
}

/* try exactly num repetitions, from high to from */
int ast_extension_patmatch_repeated(const char *pattern, char *data, const int num) 
{
    int i;
    ast_log(LOG_DEBUG, "  >>> try %d repetitions of '%s' in data '%s'\n", num, pattern, data);
    if (num <= 0) return 0;
    for (i=1; i<=num; i++) {
	ast_log(LOG_DEBUG, "  >>>> round %d with data %s\n", i, data);
	if (!ast_extension_patmatch(pattern, data)) return 0;
	data = data + strlen(pattern) - 1;
    }
    return 1;
}


int ast_extension_match(char *pattern, char *data)
{
	int match;
	patmatch_groupcounter = 0;
	*patmatch_group = '\0';
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
	    ast_log(LOG_DEBUG, "ast_extension_match %s == /%s/ => %d\n", data, pattern, match);
	} else {
	    match = ast_extension_patmatch(pattern+1,data);
	    ast_log(LOG_DEBUG, "ast_extension_match %s =~ /%s/ => %d\n", data, pattern+1, match);
	}
	return match;
}

int testmatch(char *pattern, char *data, int should)
{
    int match;

    match = ast_extension_match(pattern, data);
    if (should == match) {
	printf("OK\n");
    } else {
	printf("FAIL\n");
	exit(-1);
    }
    return match;
}

int main (int argc, char *argv[]) {
    char data1[] = "0316890002";
    char data2[] = "03168900028500";

    /* plain strcmp */
    testmatch("0316890002",data1,1);
    /* simple regex with ending . */
    testmatch("_0N.",data1,1);
    /* not terminating . */
    testmatch("_0N.0",data1,0);
#if 1
    testmatch("_0N. 8500",data1,0);
    testmatch("_0N. 8500",data2,1);
    testmatch("_0[2-9]. 8500",data2,1);
    /* ranges */
    testmatch("_[a-z]o[0-9a-z].","voicemail",1);
    testmatch("_[0]o[0-9a-z].","voicemail",0);

    /* negation */
    testmatch("_[^0-9]o.","voicemail",1);
    testmatch("_[^x]o.","voicemail",1);
    testmatch("_[^v]o.","voicemail",0);
    testmatch("_[^a-z]o.","voicemail",0);
#endif
    /* quantifiers */
    testmatch("_0316890{2}N","0316890002",0);
    testmatch("_0316890{3}N","0316890002",1);
    testmatch("_0316890{1,}N","0316890002",1);
    testmatch("_0316890{1,3}N","0316890002",1);
    testmatch("_0316890{4,5}N","0316890002",0);
    /* grouping and capturing */
    testmatch("_031689(0XX)N","0316890002",1);
    testmatch("_031689(0X9)N","0316890002",0);
    testmatch("_031689(X){1,3}N","0316890002",1);
    testmatch("_031689(X){4,3}N","0316890002",0);
    testmatch("_031689(X){3}N","0316890002",1);
    testmatch("_031689(X){4}","0316890002",1);
    testmatch("_031689(X){5}","0316890002",0);
    testmatch("_031689(0){3}N","0316890002",1);
    testmatch("_031689(X){4}N","0316890002",0);
    testmatch("_031689(X){4}N","03168900021",0);
    testmatch("_031689(X){4}Z","03168900021",1);
    testmatch("_031689(XX){2}Z","03168900021",1);
    /* alternation */
    testmatch("_(032|02)N.","0316890002",0);
#if 1
    testmatch("_(031N|0)N.","0316890002",1); /* not yet supported */
#endif
}

