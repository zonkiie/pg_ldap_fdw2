include "helper_functions.h"

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

void free_cstr(char ** str)
{
	if(*str == NULL) return;
	free(*str);
	*str = NULL;
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

char * quote_string(const char *str, quote_strings * quot)
{
	_cleanup_cstr_ char * quoted_array_delimiter;
	_cleanup_cstr_ char * quoted_attribute_delimiter;
	asprintf(&quoted_array_delimiter, "\\%s", quot->array_delimiter);
	asprintf(&quoted_attribute_delimiter, "\\%s", quot->attribute_delimiter);
	
	_cleanup_carr_ char ** step = (char**)calloc(10, sizeof(char*));
	step[0] = str_replace(str, quot->array_delimiter, quoted_array_delimiter);
	step[1] = str_replace(step[0], "\"", "\"\"\"\"");
	step[2] = str_replace(step[1], "\n", "\\n");
	step[3] = str_replace(step[2], "\r", "\\r");
	step[4] = str_replace(step[3], quot->attribute_delimiter, quoted_attribute_delimiter);
	return strdup(step[4]);
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

