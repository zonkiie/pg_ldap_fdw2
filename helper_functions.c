include "helper_functions.h"

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

int substr_count(char *str, char *substr)
{
	if(str == NULL || !strcmp(str, "")) return 0;
	char * found = str;
	int count = 0;
	while(found = strstr(found + strlen(substr), substr)) count++;
	return count;
}


