#include "helper_functions.h"

struct         timeval  zerotime = {.tv_sec = 0L, .tv_usec = 0L};

int str_split(char ***dest, char *str, char *separator)
{
	int el_count = substr_count(str, separator) + 1;
	*dest = (char**)malloc((el_count + 1) * sizeof(char*));
	memset((*dest), 0, (el_count + 1));
	char *walker = strstr(str, separator), *trailer = str;
	int index = 0;
	while(true)
	{
		if(walker == NULL)
		{
			(*dest)[index++] = strdup(trailer);
			break;
		}
		(*dest)[index++] = strndup(trailer, walker - trailer);
		trailer = walker + strlen(separator);
		walker = strstr(trailer, separator);
	}
	(*dest)[index] = NULL;
	return index;
}

int str_join(char **targetstr, char **array, char *joinstr)
{
	if(array == NULL)
	{
		*targetstr = NULL;
		return 0;
	}
	int i = 0;
	int size = 0;
	int sl = strlen(joinstr);
	(*targetstr) = (char*)calloc(strlen(array[0]) + 1, 1);
	strcpy((*targetstr), array[0]);
	while(array[i + 1] != NULL)
	{
		(*targetstr) = (char*)realloc(*targetstr, strlen(*targetstr) + strlen(array[i + 1]) + sl + 1);
		strcat(*targetstr, joinstr);
		strcat(*targetstr, array[i + 1]);
		i++;
	}
	return(size);
}


char * str_replace(const char *str, const char *search, const char *replace)
{
	if(str == NULL) return NULL;
	if(!strcmp(str, "")) return strdup("");
	char *retstr = strdup(""), *walker, *trailer = (char*)str;
	while(true)
	{
		if((walker = strstr(trailer, search)) == NULL)
		{
			_cleanup_cstr_ char * savestr = strdup(retstr);
			free(retstr);
			asprintf(&retstr, "%s%s", savestr, trailer);
			break;
		}
		_cleanup_cstr_ char * part = strndup(trailer, walker - trailer);
		_cleanup_cstr_ char * saveptr = strdup(retstr);
		free(retstr);
		asprintf(&retstr, "%s%s%s", saveptr, part, replace);
		trailer = walker + strlen(search);
	}
	return retstr;
}

char *trim(char *string, char *trimchars)
{
	if(!trimchars || strlen(trimchars) == 0) return strdup(string);
	// ltrim
	int start = 0;
	while(char_charlist(string[start], trimchars)) start++;
	// rtrim
	int copylen = strlen(string + start);
	while(copylen > 1 && char_charlist((string + start)[copylen - 1], trimchars)) copylen--;
	return strndup((string + start), copylen);
}

bool char_charlist(char c, char *charlist)
{
	for(int i = 0; i < strlen(charlist); i++)
	{
		if(charlist[i] == c) return true;
	}
	return false;
}

int substr_count(char *str, char *substr)
{
	if(str == NULL || !strcmp(str, "")) return 0;
	char * found = str;
	int count = 0;
	while(found = strstr(found + strlen(substr), substr)) count++;
	return count;
}

void free_cstr(char ** str)
{
	if(*str == NULL) return;
	free(*str);
	*str = NULL;
}

void free_pstr(char ** str)
{
	if(*str == NULL) return;
	pfree(*str);
	*str = NULL;
}

void free_options(LdapFdwOptions * options)
{
	free_pstr(options->uri);
	free_pstr(options->username);
	free_pstr(options->password);
	free_pstr(options->basedn);
	free_pstr(options->filter);
}

void reassign_cstr(char **str, const char * value)
{
	free_cstr(str);
	*str = strdup(value);
}

int get_carr_size(char ** carr)
{
	if(carr == NULL) return 0;
    int i = 0;
    for(; carr[i] != NULL; i++);
    return i;
}

/** Free c array strings and null memory */
void free_carr_n(char ***carr)
{
    if(carr == NULL || *carr == NULL) return;
	int size = get_carr_size(*carr);
	for(int i = size - 1; i >= 0; i--) 
	{
		free((*carr)[i]);
		(*carr)[i] = NULL;
	}
	free(*carr);
	*carr = NULL;
}

void free_file(FILE** file)
{
	if(*file == NULL) return;
	fflush(*file);
	fclose(*file);
	*file = NULL;
}

bool in_array(char ** array, char * value)
{
	for(int i = 0; array[i] != NULL; i++)
	{
		if(!strcmp(array[i], value)) return true;
	}
	return false;
}

/// returns true if value is added, false if value is already in array
/// array must be big enough that one further value can be added.
/// value is copied to the new position.
bool add_to_unique_array(char *** array, char * value)
{
	int i = 0;
	for(; (*array)[i] != NULL; i++)
	{
		if(!strcmp((*array)[i], value)) return false;
	}
	(*array)[i] = strdup(value);
	return true;
}

