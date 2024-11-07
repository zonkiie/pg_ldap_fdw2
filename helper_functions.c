#include "helper_functions.h"
#include "utils/elog.h"

#define DEBUGPOINT elog(INFO, "ereport File %s, Func %s, Line %d\n", __FILE__, __FUNCTION__, __LINE__)

struct         timeval  zerotime = {.tv_sec = 0L, .tv_usec = 0L};
const size_t blocksize = 1024;

/**
 * Split a string for kv pairs. Don't split more after the first separator was found.
 */
int str_split_kv(char ***dest, char *str, char *separator)
{
	int el_count = 2;
	int index = 0;
	char *walker, *trailer;
	*dest = (char**)malloc((el_count + 1) * sizeof(char*));
	memset((*dest), 0, (el_count + 1));
	
	walker = strstr(str, separator);
	trailer = str;
	
	if(walker != NULL)
	{
		(*dest)[index++] = strndup(trailer, walker - trailer);
		trailer = walker + strlen(separator);
	}
	(*dest)[index++] = strdup(trailer);
	(*dest)[index] = NULL;
	return index;
}

int str_split(char ***dest, char *str, char *separator)
{
	int el_count = substr_count(str, separator) + 1;
	int index = 0;
	char *walker, *trailer;
	*dest = (char**)malloc((el_count + 1) * sizeof(char*));
	memset((*dest), 0, (el_count + 1));
	walker = strstr(str, separator);
	trailer = str;
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
	int i = 0;
	int size = 0;
	int sl = strlen(joinstr);
	if(array == NULL)
	{
		*targetstr = NULL;
		return 0;
	}
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
	char *retstr = NULL, *walker = NULL, *trailer = NULL, *part = NULL, *saveptr = NULL;
	int scrap = 0;
	if(str == NULL) return NULL;
	if(!strcmp(str, "")) return strdup("");
	retstr = strdup("");
	trailer = (char*)str;
	while(true)
	{
		if((walker = strstr(trailer, search)) == NULL)
		{
			char * savestr;
			savestr = strdup(retstr);
			free(retstr);
			scrap = asprintf(&retstr, "%s%s", savestr, trailer);
			free(savestr);
			break;
		}
		part = strndup(trailer, walker - trailer);
		saveptr = strdup(retstr);
		free(retstr);
		scrap = asprintf(&retstr, "%s%s%s", saveptr, part, replace);
		trailer = walker + strlen(search);
		free(part);
		part = NULL;
		free(saveptr);
		saveptr = NULL;
	}
	if(scrap)
		;
	return retstr;
}

char *trim(char *string, char *trimchars)
{
	int start = 0, copylen = 0;
	if(!trimchars || strlen(trimchars) == 0) return strdup(string);
	// ltrim
	while(char_charlist(string[start], trimchars)) start++;
	// rtrim
	copylen = strlen(string + start);
	while(copylen > 1 && char_charlist((string + start)[copylen - 1], trimchars)) copylen--;
	return strndup((string + start), copylen);
}

void pstrmcat(char **targetstr, char *tocat)
{
	int newsize = strlen((*targetstr)) + strlen(tocat);
	(*targetstr) = (char*)repalloc((*targetstr), newsize + 2);
	strcat((*targetstr), tocat);
}

char * pstrmcat_multi_alloc_with_null(char * arg1, ...)
{
	char * targetstr = strdup(arg1);
	va_list args;
	char * argument;
    va_start(args, arg1);
	while((argument = va_arg(args, char*)) != NULL)
	{
		if(!strcmp(argument, "")) continue;
		targetstr = (char*)repalloc(targetstr, strlen(targetstr) + strlen(argument) + 2);
		strcat(targetstr, argument);
	}
	return targetstr;
}

// Last Argument must be NULL if no error should occur
void pstrmcat_multi_with_null(char **targetstr, ...)
{
	va_list args;
	char * argument;
    va_start(args, targetstr);
	while((argument = va_arg(args, char*)) != NULL)
	{
		if(!strcmp(argument, "")) continue;
		(*targetstr) = (char*)repalloc((*targetstr), strlen(*targetstr) + strlen(argument) + 2);
		strcat((*targetstr), argument);
	}
}

void strmcat(char **targetstr, char *tocat)
{
	int newsize = strlen((*targetstr)) + strlen(tocat);
	(*targetstr) = (char*)realloc((*targetstr), newsize + 2);
	strcat((*targetstr), tocat);
}

char * strmcat_multi_alloc_with_null(char * arg1, ...)
{
	char * targetstr = strdup(arg1);
	va_list args;
	char * argument;
    va_start(args, arg1);
	while((argument = va_arg(args, char*)) != NULL)
	{
		if(!strcmp(argument, "")) continue;
		targetstr = (char*)realloc(targetstr, strlen(targetstr) + strlen(argument) + 2);
		strcat(targetstr, argument);
	}
	return targetstr;
}

// Last Argument must be NULL if no error should occur
void strmcat_multi_with_null(char **targetstr, ...)
{
	va_list args;
	char * argument;
    va_start(args, targetstr);
	while((argument = va_arg(args, char*)) != NULL)
	{
		if(!strcmp(argument, "")) continue;
		(*targetstr) = (char*)realloc((*targetstr), strlen(*targetstr) + strlen(argument) + 2);
		strcat((*targetstr), argument);
	}
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
	char * found;
	int count = 0;
	if(str == NULL || !strcmp(str, "")) return 0;
	found = str;
	while((found = strstr(found + strlen(substr), substr))) count++;
	return count;
}

char ** array_copy(char ** input)
{
	int count = get_carr_size(input), i = 0;
	char ** output = (char**)malloc(sizeof(char*) * (count + 1));
	memset(output, 0, sizeof(char*) * (count + 1));
	if(input == NULL) return NULL;
	for(; input[i] != NULL; i++)
	{
		output[i] = strdup(input[i]);
	}
	return output;
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

void reassign_cstr(char **str, const char * value)
{
	free_cstr(str);
	*str = strdup(value);
}

int get_carr_size(char ** carr)
{
    int i = 0;
	if(carr == NULL) return 0;
    for(; carr[i] != NULL; i++);
    return i;
}

/** Free c array strings and null memory */
void free_carr_n(char ***carr)
{
	int size = 0;
    if(carr == NULL || *carr == NULL) return;
	size = get_carr_size(*carr);
	for(int i = size - 1; i >= 0; i--) 
	{
		free((*carr)[i]);
		(*carr)[i] = NULL;
	}
	free(*carr);
	*carr = NULL;
}

void free_pstr_array(char *** array)
{
	int size = 0;
    if(array == NULL || *array == NULL) return;
	size = get_carr_size(*array);
	for(int i = size - 1; i >= 0; i--) 
	{
		pfree((*array)[i]);
		(*array)[i] = NULL;
	}
	pfree(*array);
	*array = NULL;
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

bool array_has_intersect(char ** array1, char ** array2)
{
	for(int i = 0; array1[i] != NULL; i++)
	{
		for(int j = 0; array2[j] != NULL; j++)
		{
			if(!strcmp(array1[i], array2[j]))
			{
				return true;
			}
		}
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

size_t array_count(char ** array)
{
	if(array != NULL)
	{
		size_t count = 0;
		while(array[count] != NULL) count++;
		return count;
	}
	else return 0;
}

size_t array_push(char *** array, char *value)
{
	size_t n_el = array_count(*array);
	*array = realloc(*array, sizeof(char*) * (n_el + 2));
	(*array)[n_el] = strdup(value);
	(*array)[n_el + 1] = NULL;
	return array_count(*array);
}

size_t array_pushp(char *** array, char *value)
{
	size_t n_el = array_count(*array);
	*array = repalloc(*array, sizeof(char*) * (n_el + 2));
	(*array)[n_el] = pstrdup(value);
	(*array)[n_el + 1] = NULL;
	return array_count(*array);
}

