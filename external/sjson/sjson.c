/**
 * @file sjson.c
 * @version 1.0
 * @author Tristan Lee <tristan.lee@qq.com>
 * @brief A streaming JSON parser library in C
 *
 * By direct operations on buffer memory when parsing or generating JSON string,
 * SJSON don't need extra memory for any internal data structure, small and
 * efficient enough for embedded projects.
 *
 * Change Logs:
 * Date			Author		Notes
 * 2015-01-22	Tristan		the initial version, add parsing interfaces
 * 2015-01-23   Tristan		add interfaces comments
 * 2015-08-04	Tirstan		add JSON string generating interfaces
 * 2015-08-25	Tristan		modify Double number string generating code
 * 2016-01-18	Tristan		add String escaping handle code
 * 2017-11-21	Tristan		fix compiler warning on '\/' char
 *
 */

#include <minos/types.h>
#include <minos/string.h>
#include <sjson/sjson.h>

#define _FABS(d)	((double)(d)>0.0 ? (double)(d) : (double)-(d))

static char *skip_value(char *value);

static char *skip(char *in) {while (in && *in && (unsigned char)*in<=32) in++; return in;}

static unsigned parse_hex4(const char *str)
{
	unsigned h=0;
	if (*str>='0' && *str<='9')
		h+=(*str)-'0';
	else if (*str>='A' && *str<='F')
		h+=10+(*str)-'A';
	else if (*str>='a' && *str<='f')
		h+=10+(*str)-'a'; else return 0;

	h=h<<4;str++;

	if (*str>='0' && *str<='9')
		h+=(*str)-'0';
	else if (*str>='A' && *str<='F')
		h+=10+(*str)-'A';
	else if (*str>='a' && *str<='f')
		h+=10+(*str)-'a';
	else
		return 0;

	h=h<<4;str++;

	if (*str>='0' && *str<='9')
		h+=(*str)-'0';
	else if (*str>='A' && *str<='F')
		h+=10+(*str)-'A';
	else if (*str>='a' && *str<='f')
		h+=10+(*str)-'a';
	else
		return 0;

	h=h<<4;str++;

	if (*str>='0' && *str<='9')
		h+=(*str)-'0';
	else if (*str>='A' && *str<='F')
		h+=10+(*str)-'A';
	else if (*str>='a' && *str<='f')
		h+=10+(*str)-'a';
	else
		return 0;

	return h;
}

/* Parse the input text into an unescaped cstring, and populate out. */
static const unsigned char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static char *parse_string(char *str, char *out, int outLen)
{
	char *ptr=str+1;char *ptr2;int len=0;unsigned uc,uc2;
	if (*str!='\"') {return 0;}	/* not a string! */

	while (*ptr!='\"' && *ptr && ++len) if (*ptr++ == '\\') ptr++;	/* Skip escaped quotes. */

	if (!*ptr) return 0; /* malformed */
	/* This is how long we need for the string, roughly. */
	if (len+1 > outLen) return 0;

	ptr=str+1;ptr2=out;
	while (*ptr!='\"' && *ptr)
	{
		if (*ptr!='\\') *ptr2++=*ptr++;
		else
		{
			ptr++;
			switch (*ptr)
			{
				case 'b': *ptr2++='\b';	break;
				case 'f': *ptr2++='\f';	break;
				case 'n': *ptr2++='\n';	break;
				case 'r': *ptr2++='\r';	break;
				case 't': *ptr2++='\t';	break;
				case 'u':	 /* transcode utf16 to utf8. */
					uc=parse_hex4(ptr+1);ptr+=4;	/* get the unicode char. */

					if ((uc>=0xDC00 && uc<=0xDFFF) || uc==0)	break;	/* check for invalid.	*/

					if (uc>=0xD800 && uc<=0xDBFF)	/* UTF16 surrogate pairs.	*/
					{
						if (ptr[1]!='\\' || ptr[2]!='u')	break;	/* missing second-half of surrogate.	*/
						uc2=parse_hex4(ptr+3);ptr+=6;
						if (uc2<0xDC00 || uc2>0xDFFF)		break;	/* invalid second-half of surrogate.	*/
						uc=0x10000 + (((uc&0x3FF)<<10) | (uc2&0x3FF));
					}

					len=4;if (uc<0x80) len=1;else if (uc<0x800) len=2;else if (uc<0x10000) len=3; ptr2+=len;

					switch (len) {
						case 4: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 3: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 2: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 1: *--ptr2 =(uc | firstByteMark[len]);
					}
					ptr2+=len;
					break;
				default:  *ptr2++=*ptr; break;
			}
			ptr++;
		}
	}
	*ptr2=0;
	if (*ptr=='\"') ptr++;

	return ptr;
}

/* Parse the input text to generate a number, and populate the result out. */
static char *parse_number(char *num, double *out)
{
	double n=0,sign=1,scale=0;

	if (*num=='-') sign=-1,num++;	/* Has sign? */
	if (*num=='0') num++;			/* is zero */
	if (*num>='1' && *num<='9')	do	n=(n*10.0)+(*num++ -'0');	while (*num>='0' && *num<='9');	/* Number? */
	if (*num=='.' && num[1]>='0' && num[1]<='9') {num++;		do	n=(n*10.0)+(*num++ -'0'),scale++; while (*num>='0' && *num<='9');}	/* Fractional part? */

	while (scale--) n = n*0.1; /* number.fraction */

	n=sign*n;	/* number = +/- number.fraction */

	*out = n;
	return num;
}

static char *skip_string(char *str)
{
	char *ptr;

	str = skip(str);
	if (*str!='\"') {return 0;}	/* not a string! */
	ptr=str+1;

	while (*ptr!='\"' && *ptr) if (*ptr++ == '\\') ptr++;	/* Skip escaped quotes. */

	/* Skip last mark */
	if (*ptr == '\"') return ptr+1;
	return 0;
}

static char *skip_number(char *num)
{
	num = skip(num);

	if (*num=='-') num++;	/* Has sign? */
	while (*num>='0' && *num<='9')	num++;	/* Number? */
	if (*num=='.' && num[1]>='0' && num[1]<='9') {num++; do	num++; while (*num>='0' && *num<='9');}	/* Fractional part? */
	if (*num=='e' || *num=='E')		/* Exponent? */
	{	num++;if (*num=='+') num++;	else if (*num=='-') num++;		/* With sign? */
		while (*num>='0' && *num<='9') num++;	/* Number? */
	}

	return num;
}

static char *skip_object(char *value)
{
	value = skip(value);

	if (*value!='{') {return 0;}	/* not an object! */

	value=skip(value+1);
	if (*value=='}') return value+1;	/* empty array. */

	value = skip(skip_string(skip(value)));
	if (!value) return 0;
	if (*value!=':') {return 0;}	/* fail! */

	value=skip(skip_value(skip(value+1)));	/* skip any spacing, get the value. */
	if (!value) return 0;

	while (*value==',')
	{
		value=skip(skip_string(skip(value+1)));
		if (!value) return 0;
		if (*value!=':') {return 0;}	/* fail! */
		value=skip(skip_value(skip(value+1)));	/* skip any spacing, get the value. */
		if (!value) return 0;
	}

	if (*value=='}') return value+1;	/* end of array */
	return 0;	/* malformed. */
}

static char *skip_array(char *value)
{
	value = skip(value);

	if (*value!='[') {return 0;}	/* not an array! */

	value=skip(value+1);
	if (*value==']') return value+1;	/* empty array. */

	value=skip(skip_value(skip(value)));	/* skip any spacing, get the value. */
	if (!value) return 0;

	while (*value==',')
	{
		value=skip(skip_value(skip(value+1)));
		if (!value) return 0;	/* memory fail */
	}

	if (*value==']') return value+1;	/* end of array */
	return 0;	/* malformed. */
}

static char *skip_value(char *value)
{
	value = skip(value);

	if (!value)						return 0;	/* Fail on null. */
	if (!strncmp(value,"null",4))	{ return value+4; }
	if (!strncmp(value,"false",5))	{ return value+5; }
	if (!strncmp(value,"true",4))	{ return value+4; }
	if (*value=='\"')				{ return skip_string(value); }
	if (*value=='-' || (*value>='0' && *value<='9'))	{ return skip_number(value); }
	if (*value=='[')				{ return skip_array(value); }
	if (*value=='{')				{ return skip_object(value); }

	return 0;	/* failure. */
}

static int _esc_strlen(const char *str)
{
    const char *sc;
    int len = 0;

    for (sc = str; *sc != '\0'; ++sc)
    {
        switch (*sc)
		{
			case '\b':
			case '\f':
			case '\n':
			case '\r':
			case '\t':
            case '\"':
            case '\\':
            case '/':
                len++;
                break;
        }
    }

    return (sc - str) + len;
}

static char *_esc_strcpy(char *dst, const char *src)
{
    char *d = dst;
    const char *s = src;
    int loopEnd = 0;

    do
    {
        switch (*s)
		{
            case '\b': { *d++ = '\\'; *d++ = 'b'; s++; break; }
            case '\f': { *d++ = '\\'; *d++ = 'f'; s++; break; }
            case '\n': { *d++ = '\\'; *d++ = 'n'; s++; break; }
            case '\r': { *d++ = '\\'; *d++ = 'r'; s++; break; }
            case '\t': { *d++ = '\\'; *d++ = 't'; s++; break; }
            case '\"': { *d++ = '\\'; *d++ = '\"'; s++; break; }
            case '\\': { *d++ = '\\'; *d++ = '\\'; s++; break; }
            case '/': { *d++ = '\\'; *d++ = '/'; s++; break; }
            default:
                if ((*d++ = *s++) == 0) loopEnd = 1;
                break;
        }
    } while (!loopEnd);

    return (dst);
}

static int _esc_strncmp(const char *cs, const char *ct, unsigned count)
{
    register signed char __res = 0;
    int loopEnd = 0;

    do
    {
        switch (*ct)
		{
			case '\b':
                if ((__res = cs[0] - '\\') != 0 || (__res = cs[1] - 'b') != 0 ) loopEnd = 1;
                cs += 2; ct++; count--;
                break;
			case '\f':
                if ((__res = cs[0] - '\\') != 0 || (__res = cs[1] - 'f') != 0 ) loopEnd = 1;
                cs += 2; ct++; count--;
                break;
			case '\n':
                if ((__res = cs[0] - '\\') != 0 || (__res = cs[1] - 'n') != 0 ) loopEnd = 1;
                cs += 2; ct++; count--;
                break;
			case '\r':
                if ((__res = cs[0] - '\\') != 0 || (__res = cs[1] - 'r') != 0 ) loopEnd = 1;
                cs += 2; ct++; count--;
                break;
			case '\t':
                if ((__res = cs[0] - '\\') != 0 || (__res = cs[1] - 't') != 0 ) loopEnd = 1;
                cs += 2; ct++; count--;
                break;
            case '\"':
                if ((__res = cs[0] - '\\') != 0 || (__res = cs[1] - '\"') != 0 ) loopEnd = 1;
                cs += 2; ct++; count--;
                break;
            case '\\':
                if ((__res = cs[0] - '\\') != 0 || (__res = cs[1] - '\\') != 0 ) loopEnd = 1;
                cs += 2; ct++; count--;
                break;
            case '/':
                if ((__res = cs[0] - '\\') != 0 || (__res = cs[1] - '/') != 0 ) loopEnd = 1;
                cs += 2; ct++; count--;
                break;
            default:
                if ((__res = *cs - *ct) != 0 || !*cs) loopEnd = 1;
                cs++; ct++; count--;
                break;
        }
    } while (count && (!loopEnd));

    return __res;
}

/**
 *  @brief Check if the input string is valid JSON string.
 *
 *	@param [in] json JSON string
 *  @return 1: valid JSON string, 0: invalid JSON string
 */
int sjson_is_valid(char *json)
{
	char *objValue, *arrayValue;

    objValue = skip_object(json);
    arrayValue = skip_array(json);

	if(objValue || arrayValue) return 1;
	return 0;
}

/**
 *  @brief Minify JSON string, remove all unnecessary whitespace characters.
 *
 *	@param [in] json JSON string
 *  @return void
 */
void sjson_minify(char *json)
{
	char *into=json;
	while (*json)
	{
		if (*json==' ') json++;
		else if (*json=='\t') json++;	// Whitespace characters.
		else if (*json=='\r') json++;
		else if (*json=='\n') json++;
		else if (*json=='/' && json[1]=='/')  while (*json && *json!='\n') json++;	// double-slash comments, to end of line.
		else if (*json=='/' && json[1]=='*') {while (*json && !(*json=='*' && json[1]=='/')) json++;json+=2;}	// multiline comments.
		else if (*json=='\"'){*into++=*json++;while (*json && *json!='\"'){if (*json=='\\') *into++=*json++;*into++=*json++;}*into++=*json++;} // string literals, which are \" sensitive.
		else *into++=*json++;			// All other characters.
	}
	*into=0;	// and null-terminate.
}

/**
 *  @brief Get value type
 *
 *	@param [in] value JSON value
 *  @return Return value type
 */
int sjson_type(char *value)
{
	value = skip(value);

	if (!value)						return -1;	/* Fail on null. */
	if (!strncmp(value,"null",4))	{ return SJSON_NULL; }
	if (!strncmp(value,"false",5))	{ return SJSON_FALSE; }
	if (!strncmp(value,"true",4))	{ return SJSON_TRUE; }
	if (*value=='\"')				{ return SJSON_STRING; }
	if (*value=='-' || (*value>='0' && *value<='9'))	{ return SJSON_NUMBER; }
	if (*value=='[')				{ return SJSON_ARRAY; }
	if (*value=='{')				{ return SJSON_OBJECT; }

	return -1;	/* failure. */
}

/**
 *  @brief Compare object name with the given name string.
 *
 *	@param [in] obj JSON object
 *  @param [in] name Name string
 *  @return 0: equal, non-zero: not equal
 */
int sjson_obj_strcmp(const char *obj, const char *name)
{
	const char *ptr=obj+1; int len=0;
	if (*obj!='\"') {return -1;}	/* not a string! */

	while (*ptr!='\"' && *ptr && ++len) if (*ptr++ == '\\') {ptr++; len++;}	/* Skip escaped quotes. */

	if (!*ptr) return -1; /* malformed */
    if (_esc_strlen(name) != len) return -1;

	return _esc_strncmp(obj+1, name, strlen(name));
}

/**
 *  @brief Parse value of SJSON_STRING type.
 *
 *	@param [in] value JSON value
 *  @param [out] out Output string buffer
 *  @param [in] len Output string buffer length
 *  @return Return pointer after value
 */
char *sjson_parse_string(char *value, char *out, int len)
{
    if (sjson_type(value) != SJSON_STRING) return 0;
	return parse_string(skip(value), out, len);
}

/**
 *  @brief Parse value of SJSON_NUMBER type
 *
 *	@param [in] value JSON value
 *  @param [out] out Output number
 *  @return Return pointer after value
 */
char *sjson_parse_num(char *value, double *out)
{
    if (sjson_type(value) != SJSON_NUMBER) return 0;
	return parse_number(skip(value), out);
}

/**
 *  @brief Move pointer to next object.
 *
 *	@param [in] obj JSON object
 *  @param [out] type Object value type
 *  @param [out] end Object value end pointer
 *  @return Return next object pointer
 *
 *  @note Out parameters (type and end) can be simply ignore by passing in 0 pointer.
 */
char *sjson_obj_to_next(char *obj, int *type, char **end)
{
	char *value;

	if (!obj) return 0; /* Fail on null. */
	obj = skip(obj);

	value = skip(skip_string(obj));
	if (!value) return 0; /* malformed */
	if (*value!=':') {return 0;}	/* fail! */

	value=skip(skip_value(skip(value+1)));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
	if (*value!=',') {return 0;}	/* fail! */

	obj=skip(value+1);
	if (*obj=='}') return 0;	/* array end. */
	value = sjson_obj_to_value(obj, type, end); /* get type and end */
	if (!value) return 0; /* malformed */

	return obj;
}

/**
 *  @brief Move pointer to value from object.
 *
 *	@param [in] obj JSON object
 *  @param [out] type Object value type
 *  @param [out] end Object value end pointer
 *  @return Return object value pointer
 *
 *  @note Out parameters (type and end) can be simply ignore by passing in 0 pointer.
 */
char *sjson_obj_to_value(char *obj, int *type, char **end)
{
	char *value;

	if (!obj) return 0; /* Fail on null. */
	obj = skip(obj);

	value = skip(skip_string(obj));
	if (!value) return 0; /* malformed */
	if (*value!=':') {return 0;}	/* fail! */

	if (type)
	{
		*type = sjson_type(skip(value+1));
		if (*type < 0) return 0; /* error type */
	}
	if (end)
	{
		*end = skip_value(skip(value+1));
		if (!*end) return 0; /* error value */
	}

	return skip(value+1);
}

/**
 *  @brief Locate value with the given index into search array.
 *
 *	@param [in] value JSON value within which to search
 *	@param [in] idx Index into search array
 *  @param [out] type Object value type
 *  @param [out] end Object value end pointer
 *  @return Return value pointer if any identified by the given index
 *
 *  @note Out parameters (type and end) can be simply ignore by passing in 0 pointer.
 */
char *sjson_to_array_value(char *value, unsigned idx, int *type, char **end)
{
	if (!value) return 0; /* Fail on null. */
	value = skip(value);

	if (*value!='[') return 0;	/* not an array! */

	value=skip(value+1);
	if (*value==']') return 0;	/* empty array. */

	while (idx>0)
	{
		idx--;
		value = skip(skip_value(value));
		if (!value) return 0; /* malformed. */
		if (*value != ',') return 0; /* probably end */
		value=skip(value+1);
		if (*value==']') return 0;	/* empty array. */
	}

	if (type)
	{
		*type = sjson_type(value);
		if (*type < 0) return 0; /* error type */
	}
	if (end)
	{
		*end = skip_value(value);
		if (!*end) return 0; /* error value */
	}
	return value;
}

/**
 *  @brief Locate object with the given index into search value.
 *
 *	@param [in] value JSON value within which to search
 *	@param [in] idx Index into search value
 *  @param [out] type Object value type
 *  @param [out] end Object value end pointer
 *  @return Return object pointer if any identified by the given index
 *
 *  @note Out parameters (type and end) can be simply ignore by passing in 0 pointer.
 */
char *sjson_to_idxobj(char *value, unsigned idx, int *type, char **end)
{
	char *obj;

	if (!value) return 0; /* Fail on null. */
	value = skip(value);

	if (*value!='{') {return 0;}	/* not an object! */

	obj=value=skip(value+1);
	if (*value=='}') return 0;	/* empty array. */

	value = sjson_obj_to_value(obj, type, end); /* get type and end in case of idx 0*/
	if (!value) return 0; /* malformed */

	while (idx>0)
	{
		idx--;
		obj = sjson_obj_to_next(obj, type, end);
		if (!obj) return 0; /* no next */
	}

	return obj;
}

/**
 *  @brief Locate object value with the given index into search value.
 *
 *	@param [in] value JSON value within which to search
 *	@param [in] idx Index into search value
 *  @param [out] type Object value type
 *  @param [out] end Object value end pointer
 *  @return Return object value pointer if any identified by the given index
 *
 *  @note Out parameters (type and end) can be simply ignore by passing in 0 pointer.
 */
char *sjson_to_idxobjValue(char *value, unsigned idx, int *type, char **end)
{
	char *obj;

	obj = sjson_to_idxobj(value, idx, 0, 0);
	if (!obj) return 0; /* not found */

	return sjson_obj_to_value(obj, type, end);
}

/**
 *  @brief Locate object with the given name, non-recursive mode.
 *
 *	@param [in] value JSON value within which to search
 *	@param [in] name Search Name
 *  @param [out] type Object value type
 *  @param [out] end Object value end pointer
 *  @return Return object pointer if any identified by the given Name
 *
 *  @note Out parameters (type and end) can be simply ignore by passing in 0 pointer.
 */
char *sjson_to_obj(char *value, const char *name, int *type, char **end)
{
	char *obj;

	if (!value || !name || !*name) return 0;
	value = skip(value);

	obj = sjson_to_idxobj(value, 0, 0, 0);
	if (!obj) return 0; /* malformed */

	while (obj)
	{
		if (!sjson_obj_strcmp(obj, name))
		{
			value = sjson_obj_to_value(obj, type, end); /* get type and end */
			if (!value) return 0; /* malformed */
			return obj;
		}

		obj = sjson_obj_to_next(obj, 0, 0);
	}

	return 0;	/* obj not found */
}

/**
 *  @brief Locate object value with the given name, non-recursive mode.
 *
 *	@param [in] value JSON value within which to search
 *	@param [in] name Search Name
 *  @param [out] type Object value type
 *  @param [out] end Object value end pointer
 *  @return Return object value pointer if any identified by the given Name
 *
 *  @note Out parameters (type and end) can be simply ignore by passing in 0 pointer.
 */
char *sjson_to_objValue(char *value, const char *name, int *type, char **end)
{
	char *obj;

	obj = sjson_to_obj(value, name, 0, 0);
	if (!obj) return 0; /* not found */

	return sjson_obj_to_value(obj, type, end);
}

/**
 *  @brief Locate object value with the given path.
 *
 *	@param [in] value JSON value within which to search
 *	@param [in] path Search path
 *  @param [out] type Object value type
 *  @param [out] end Object value end pointer
 *  @return Return object value pointer if any identified by the given path
 *
 *  @note The path is a slash-separated list of object names. The name "*" is
 *  considered a wildcard for any object name in one level. For examples,
 *  "foo/one/two", "*\/name", "foo/array[0]/name", "*[0]/name", "[1]/name"
 *  and so forth.
 *  Out parameters (type and end) can be simply ignore by passing in 0 pointer.
 */
char *sjson_to_path_value(char *value, const char *path, int *type, char **end)
{
	int i, n;
	char *obj, *out;
	const char *name;

	if (!value || !path || !*path) return 0;
	value = skip(value);

	if (*value!='{' && *value!='[') {return 0;}	/* not an object or array! */

	obj=skip(value+1);
	if (*obj=='}') return 0;	/* empty object. */
	if (*obj==']') return 0;    /* empty array. */

	// wildcard match
	if (path[0] == '*' && (path[1] == '/' || path[1] == '['))
	{
		path++;
		if (*path == '/') path++;

		while (obj) // process all children
		{
			out = sjson_to_path_value(sjson_obj_to_value(obj, 0, 0), path, type, end);
			if (out) return out;
			obj = sjson_obj_to_next(obj, 0, 0);
		}

		return 0;
	}

	name = path;
	// get through object name
	for (i=0; path[i]!='/' && path[i]!='[' && path[i]; i++);

	if (path[i] == '/')
	{
		path = &path[i+1];
		value = sjson_to_objValue(value, name, 0, 0);
		if (!value) return 0; // not found
		return sjson_to_path_value(value, path, type, end);
	}

	if (path[i] == '[')
	{
		n = 0;
		path = &path[i+1];
		// try to find array subfix
		if (*path < '0' || *path > '9') return 0; // not valid number
		while (*path >= '0' && *path <= '9')
		{
			n = n*10 + (*path - '0');
			path++;
		}
		if (*path++ != ']') return 0; // path syntex error
		if (*path != '/' && *path != 0) return 0; // path syntex error

		if (*name != '[') value = sjson_to_objValue(value, name, 0, 0);
		if (!value) return 0; // not found
		value = sjson_to_array_value(value, n, type, end);
		if (!*path) return value; // path ends
		path++; // pass char '/'
		return sjson_to_path_value(value, path, type, end);
	}

	// path ends
	if (!path[i]) return sjson_to_objValue(value, name, type, end);

	return 0;
}

/**
 *  @brief Locate object value with the given name in recursive mode.
 *
 *	@param [in] value JSON value within which to search
 *	@param [in] name Search Name
 *  @param [out] type Object value type
 *  @param [out] end Object value end pointer
 *  @return Return object value pointer if any identified by the given Name
 *
 *  @note Out parameters (type and end) can be simply ignore by passing in 0 pointer.
 */
char *sjson_find_obj_value(char *value, const char *name, int *type, char **end)
{
	char *obj;

	if (!value || !name || !*name) return 0;
	value = skip(value);

	obj = sjson_to_idxobj(value, 0, 0, 0);
	if (!obj) return 0; /* malformed */

	value = sjson_to_objValue(value, name, type, end);
	if (value) return value; /* object value found */

	while (obj)
	{
		value = sjson_obj_to_value(obj, 0, 0);
		value = sjson_find_obj_value(value, name, type, end);
		if (value) return value; /* object value found */

		obj = sjson_obj_to_next(obj, 0, 0);
	}

	return 0;
}


char *sjson_create_root_obj(char *buf, char *end)
{
    char *ptr = buf;

    if (buf + 3 > end) return 0;

    *ptr++ = '{';
    *ptr++ = '}';
    *ptr++ = 0;

    return buf;
}

int sjson_obj_size(char *value)
{
    int size = 0;

	if (!value) return 0; /* Fail on null. */
	value = skip(value); /* skip any spacing */
	if (*value!='{') {return 0;}	/* not an object! */
	value = skip(value+1); /* skip '{' */

	while (*value != '}')
	{
	value = skip(skip_string(value));
	if (!value) return 0; /* malformed */
	if (*value!=':') {return 0;}	/* fail! */

	value=skip(skip_value(skip(value+1)));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') value=skip(value+1);

        size++;
	}

	return size;	/* malformed. */
}

char *sjson_obj_add_string(char *buf, char *end, const char *name, const char* valueStr)
{
    int totLen,objLen,nameLen,valueStrLen;
    char *value = 0;
    int comma = 0;

	if (!buf) return 0; /* Fail on null. */
    if (!skip_object(buf)) return 0; /* check if valid JSON string */

	value = skip(buf); /* skip any spacing */
	value = skip(value+1); /* skip '{' */

	while (*value != '}')
	{
	value = skip(skip_string(value));
	if (!value) return 0; /* malformed */
	if (*value!=':') {return 0;}	/* fail! */

	value=skip(skip_value(skip(value+1)));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') {value=skip(value+1);} else {comma = 1;}
	}

    nameLen = _esc_strlen(name);
    valueStrLen = _esc_strlen(valueStr);
    objLen = nameLen + 2 + 1 + valueStrLen + 2; /* 2byte for '\"', 1byte for ':' */
    if (comma) objLen += 1; /* 1 byte for ',' */
    totLen = strlen(buf) + objLen + 1; /* 1 byte for '\0' */
    if (buf + totLen > end) return 0; /* buffer length not enough */

    memmove(value+objLen, value, strlen(value)+1); /* 1 byte for '\0' */
    if (comma) *value++ = ',';
	*value++ = '\"';
    _esc_strcpy(value, name);
	value += nameLen;
	*value++ = '\"';
    *value++ = ':';
	*value++ = '\"';
    _esc_strcpy(value, valueStr);
	value += valueStrLen;
	*value++ = '\"';

	return buf;
}

char *sjson_obj_add_num(char *buf, char *end, const char *name, double d)
{
    int totLen,objLen,nameLen,numLen;
    char *value = 0;
    int comma = 0;
    int frac = 0;
	int i = (int)d;
	char str[64]; /* 64 should be enough for number storage */

	if (!buf) return 0; /* Fail on null. */
    if (!skip_object(buf)) return 0; /* check if valid JSON string */
    if (d>INT_MAX || d<INT_MIN) return 0; /* don't handle large number */

	value = skip(buf); /* skip any spacing */
	value = skip(value+1); /* skip '{' */

	while (*value != '}')
	{
	value = skip(skip_string(value));
	if (!value) return 0; /* malformed */
	if (*value!=':') {return 0;}	/* fail! */

	value=skip(skip_value(skip(value+1)));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') {value=skip(value+1);} else {comma = 1;}
	}

	frac = (int)(_FABS(d-i)*1000000);
	if (frac)
	{
		char *fmt;
		/* Cause libc heap limitation on some embedded platform,
		 * we don't use %f for double handling */
		i = (int)_FABS(d);
		fmt = d<0.0 ? "-%d.%06d" : "%d.%06d"; // in case of i=0
		numLen = sprintf(str, fmt, i, frac);
	}
	else
	{
		numLen = sprintf(str, "%d", i);
	}

    nameLen = _esc_strlen(name);
    objLen = nameLen + 2 + 1 + numLen; /* 2byte for '\"', 1byte for ':' */
    if (comma) objLen += 1; /* 1 byte for ',' */
    totLen = strlen(buf) + objLen + 1; /* 1 byte for '\0' */
    if (buf + totLen > end) return 0; /* buffer length not enough */

    memmove(value+objLen, value, strlen(value)+1); /* 1 byte for '\0' */
    if (comma) *value++ = ',';
	*value++ = '\"';
    _esc_strcpy(value, name);
	value += nameLen;
	*value++ = '\"';
    *value++ = ':';
    memcpy(value, str, numLen);

	return buf;
}

char *sjson_obj_add_true(char *buf, char *end, const char *name)
{
    int totLen,objLen,nameLen;
    char *value = 0;
    int comma = 0;

	if (!buf) return 0; /* Fail on null. */
    if (!skip_object(buf)) return 0; /* check if valid JSON string */

	value = skip(buf); /* skip any spacing */
	value = skip(value+1); /* skip '{' */

	while (*value != '}')
	{
	value = skip(skip_string(value));
	if (!value) return 0; /* malformed */
	if (*value!=':') {return 0;}	/* fail! */

	value=skip(skip_value(skip(value+1)));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') {value=skip(value+1);} else {comma = 1;}
	}

    nameLen = _esc_strlen(name);
    objLen = nameLen + 2 + 1 + 4; /* 2byte for '\"', 1byte for ':',  4byte for "true"*/
    if (comma) objLen += 1; /* 1 byte for ',' */
    totLen = strlen(buf) + objLen + 1; /* 1 byte for '\0' */
    if (buf + totLen > end) return 0; /* buffer length not enough */

    memmove(value+objLen, value, strlen(value)+1); /* 1 byte for '\0' */
    if (comma) *value++ = ',';
	*value++ = '\"';
    _esc_strcpy(value, name);
	value += nameLen;
	*value++ = '\"';
    *value++ = ':';
	value[0] = 't';
	value[1] = 'r';
	value[2] = 'u';
	value[3] = 'e';

	return buf;
}

char *sjson_obj_add_false(char *buf, char *end, const char *name)
{
    int totLen,objLen,nameLen;
    char *value = 0;
    int comma = 0;

	if (!buf) return 0; /* Fail on null. */
    if (!skip_object(buf)) return 0; /* check if valid JSON string */

	value = skip(buf); /* skip any spacing */
	value = skip(value+1); /* skip '{' */

	while (*value != '}')
	{
	value = skip(skip_string(value));
	if (!value) return 0; /* malformed */
	if (*value!=':') {return 0;}	/* fail! */

	value=skip(skip_value(skip(value+1)));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') {value=skip(value+1);} else {comma = 1;}
	}

    nameLen = _esc_strlen(name);
    objLen = nameLen + 2 + 1 + 5; /* 2byte for '\"', 1byte for ':',  5byte for "false"*/
    if (comma) objLen += 1; /* 1 byte for ',' */
    totLen = strlen(buf) + objLen + 1; /* 1 byte for '\0' */
    if (buf + totLen > end) return 0; /* buffer length not enough */

    memmove(value+objLen, value, strlen(value)+1); /* 1 byte for '\0' */
    if (comma) *value++ = ',';
	*value++ = '\"';
    _esc_strcpy(value, name);
	value += nameLen;
	*value++ = '\"';
    *value++ = ':';
	value[0] = 'f';
	value[1] = 'a';
	value[2] = 'l';
	value[3] = 's';
    value[4] = 'e';

	return buf;
}

char *sjson_obj_add_null(char *buf, char *end, const char *name)
{
    int totLen,objLen,nameLen;
    char *value = 0;
    int comma = 0;

	if (!buf) return 0; /* Fail on null. */
    if (!skip_object(buf)) return 0; /* check if valid JSON string */

	value = skip(buf); /* skip any spacing */
	value = skip(value+1); /* skip '{' */

	while (*value != '}')
	{
	value = skip(skip_string(value));
	if (!value) return 0; /* malformed */
	if (*value!=':') {return 0;}	/* fail! */

	value=skip(skip_value(skip(value+1)));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') {value=skip(value+1);} else {comma = 1;}
	}

    nameLen = _esc_strlen(name);
    objLen = nameLen + 2 + 1 + 4; /* 2byte for '\"', 1byte for ':',  4byte for "null"*/
    if (comma) objLen += 1; /* 1 byte for ',' */
    totLen = strlen(buf) + objLen + 1; /* 1 byte for '\0' */
    if (buf + totLen > end) return 0; /* buffer length not enough */

    memmove(value+objLen, value, strlen(value)+1); /* 1 byte for '\0' */
    if (comma) *value++ = ',';
	*value++ = '\"';
    _esc_strcpy(value, name);
	value += nameLen;
	*value++ = '\"';
    *value++ = ':';
	value[0] = 'n';
	value[1] = 'u';
	value[2] = 'l';
	value[3] = 'l';

	return buf;
}

char *sjson_obj_add_obj(char *buf, char *end, const char *name)
{
    int totLen,objLen,nameLen;
    char *value = 0;
    int comma = 0;

	if (!buf) return 0; /* Fail on null. */
    if (!skip_object(buf)) return 0; /* check if valid JSON string */

	value = skip(buf); /* skip any spacing */
	value = skip(value+1); /* skip '{' */

	while (*value != '}')
	{
	value = skip(skip_string(value));
	if (!value) return 0; /* malformed */
	if (*value!=':') {return 0;}	/* fail! */

	value=skip(skip_value(skip(value+1)));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') {value=skip(value+1);} else {comma = 1;}
	}

    nameLen = _esc_strlen(name);
    objLen = nameLen + 2 + 1 + 2; /* 2byte for '\"', 1byte for ':',  2byte for "{}"*/
    if (comma) objLen += 1; /* 1 byte for ',' */
    totLen = strlen(buf) + objLen + 1; /* 1 byte for '\0' */
    if (buf + totLen > end) return 0; /* buffer length not enough */

    memmove(value+objLen, value, strlen(value)+1); /* 1 byte for '\0' */
    if (comma) *value++ = ',';
	*value++ = '\"';
    _esc_strcpy(value, name);
	value += nameLen;
	*value++ = '\"';
    *value++ = ':';
	value[0] = '{';
	value[1] = '}';

	return value;
}

char *sjson_obj_add_array(char *buf, char *end, const char *name)
{
    int totLen,objLen,nameLen;
    char *value = 0;
    int comma = 0;

	if (!buf) return 0; /* Fail on null. */
    if (!skip_object(buf)) return 0; /* check if valid JSON string */

	value = skip(buf); /* skip any spacing */
	value = skip(value+1); /* skip '{' */

	while (*value != '}')
	{
	value = skip(skip_string(value));
	if (!value) return 0; /* malformed */
	if (*value!=':') {return 0;}	/* fail! */

	value=skip(skip_value(skip(value+1)));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') {value=skip(value+1);} else {comma = 1;}
	}

    nameLen = _esc_strlen(name);
    objLen = nameLen + 2 + 1 + 2; /* 2byte for '\"', 1byte for ':',  2byte for "[]"*/
    if (comma) objLen += 1; /* 1 byte for ',' */
    totLen = strlen(buf) + objLen + 1; /* 1 byte for '\0' */
    if (buf + totLen > end) return 0; /* buffer length not enough */

    memmove(value+objLen, value, strlen(value)+1); /* 1 byte for '\0' */
    if (comma) *value++ = ',';
	*value++ = '\"';
    _esc_strcpy(value, name);
	value += nameLen;
	*value++ = '\"';
    *value++ = ':';
	value[0] = '[';
	value[1] = ']';

	return value;
}

int sjson_array_size(char *value)
{
    int size = 0;

	if (!value) return 0; /* Fail on null. */
	value = skip(value); /* skip any spacing */
    if (*value!='[') {return 0;}	/* not an array! */
	value = skip(value+1); /* skip '[' */

	while (*value != ']')
	{
	value=skip(skip_value(value));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') value=skip(value+1);

        size++;
	}

	return size;	/* malformed. */
}

char *sjson_array_add_string(char *buf, char *end, const char* valueStr)
{
    int totLen,objLen,valueStrLen;
    char *value = 0;
    int comma = 0;

	if (!buf) return 0; /* Fail on null. */
    if (!skip_array(buf)) return 0; /* check if valid array */

	value = skip(buf); /* skip any spacing */
	value = skip(value+1); /* skip '[' */

	while (*value != ']')
	{
	value=skip(skip_value(value));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') {value=skip(value+1);} else {comma = 1;}
	}

    valueStrLen = _esc_strlen(valueStr);
    objLen = valueStrLen + 2; /* 2byte for '\"' */
    if (comma) objLen += 1; /* 1 byte for ',' */
    totLen = strlen(buf) + objLen + 1; /* 1 byte for '\0' */
    if (buf + totLen > end) return 0; /* buffer length not enough */

    memmove(value+objLen, value, strlen(value)+1); /* 1 byte for '\0' */
    if (comma) *value++ = ',';
	*value++ = '\"';
    _esc_strcpy(value, valueStr);
	value += valueStrLen;
	*value++ = '\"';

	return buf;
}

char *sjson_array_add_num(char *buf, char *end, double d)
{
    int totLen, objLen, numLen;
    char *value = 0;
    int comma = 0;
    int frac = 0;
	int i = (int)d;
	char str[64]; /* 64 should be enough for number storage */

	if (!buf) return 0; /* Fail on null. */
    if (!skip_array(buf)) return 0; /* check if valid array */
    if (d>INT_MAX || d<INT_MIN) return 0; /* don't handle large number */

	value = skip(buf); /* skip any spacing */
	value = skip(value+1); /* skip '[' */

	while (*value != ']')
	{
	value=skip(skip_value(value));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') {value=skip(value+1);} else {comma = 1;}
	}

	frac = (int)(_FABS(d-i)*1000000);
	if (frac)
	{
		char *fmt;
		/* Cause libc heap limitation on some embedded platform,
		 * we don't use %f for double handling */
		i = (int)_FABS(d);
		fmt = d<0.0 ? "-%d.%06d" : "%d.%06d"; // in case of i=0
		numLen = sprintf(str, fmt, i, frac);
	}
	else
	{
		numLen = sprintf(str, "%d", i);
	}

	objLen = numLen;
    if (comma) objLen += 1; /* 1 byte for ',' */
    totLen = strlen(buf) + objLen + 1; /* 1 byte for '\0' */
    if (buf + totLen > end) return 0; /* buffer length not enough */

    memmove(value+objLen, value, strlen(value)+1); /* 1 byte for '\0' */
	if (comma) *value++ = ',';
	memcpy(value, str, numLen);

	return buf;
}

char *sjson_array_add_true(char *buf, char *end)
{
    int totLen,objLen;
    char *value = 0;
    int comma = 0;

	if (!buf) return 0; /* Fail on null. */
    if (!skip_array(buf)) return 0; /* check if valid array */

	value = skip(buf); /* skip any spacing */
	value = skip(value+1); /* skip '[' */

	while (*value != ']')
	{
	value=skip(skip_value(value));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') {value=skip(value+1);} else {comma = 1;}
	}

    objLen = 4; /* 4byte for "true" */
    if (comma) objLen += 1; /* 1 byte for ',' */
    totLen = strlen(buf) + objLen + 1; /* 1 byte for '\0' */
    if (buf + totLen > end) return 0; /* buffer length not enough */

    memmove(value+objLen, value, strlen(value)+1); /* 1 byte for '\0' */
    if (comma) *value++ = ',';
	value[0] = 't';
	value[1] = 'r';
	value[2] = 'u';
	value[3] = 'e';

	return buf;
}

char *sjson_array_add_false(char *buf, char *end)
{
    int totLen,objLen;
    char *value = 0;
    int comma = 0;

	if (!buf) return 0; /* Fail on null. */
    if (!skip_array(buf)) return 0; /* check if valid array */

	value = skip(buf); /* skip any spacing */
	value = skip(value+1); /* skip '[' */

	while (*value != ']')
	{
	value=skip(skip_value(value));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') {value=skip(value+1);} else {comma = 1;}
	}

    objLen = 5; /* 5byte for "false" */
    if (comma) objLen += 1; /* 1 byte for ',' */
    totLen = strlen(buf) + objLen + 1; /* 1 byte for '\0' */
    if (buf + totLen > end) return 0; /* buffer length not enough */

    memmove(value+objLen, value, strlen(value)+1); /* 1 byte for '\0' */
    if (comma) *value++ = ',';
	value[0] = 'f';
	value[1] = 'a';
	value[2] = 'l';
	value[3] = 's';
    value[4] = 'e';

	return buf;
}

char *sjson_array_add_null(char *buf, char *end)
{
    int totLen,objLen;
    char *value = 0;
    int comma = 0;

	if (!buf) return 0; /* Fail on null. */
    if (!skip_array(buf)) return 0; /* check if valid array */

	value = skip(buf); /* skip any spacing */
	value = skip(value+1); /* skip '[' */

	while (*value != ']')
	{
	value=skip(skip_value(value));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') {value=skip(value+1);} else {comma = 1;}
	}

    objLen = 4; /* 4byte for "null"*/
    if (comma) objLen += 1; /* 1 byte for ',' */
    totLen = strlen(buf) + objLen + 1; /* 1 byte for '\0' */
    if (buf + totLen > end) return 0; /* buffer length not enough */

    memmove(value+objLen, value, strlen(value)+1); /* 1 byte for '\0' */
    if (comma) *value++ = ',';
	value[0] = 'n';
	value[1] = 'u';
	value[2] = 'l';
	value[3] = 'l';

	return buf;
}

char *sjson_array_add_obj(char *buf, char *end)
{
    int totLen,objLen;
    char *value = 0;
    int comma = 0;

	if (!buf) return 0; /* Fail on null. */
    if (!skip_array(buf)) return 0; /* check if valid array */

	value = skip(buf); /* skip any spacing */
	value = skip(value+1); /* skip '[' */

	while (*value != ']')
	{
	value=skip(skip_value(value));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') {value=skip(value+1);} else {comma = 1;}
	}

    objLen = 2; /* 2byte for "{}" */
    if (comma) objLen += 1; /* 1 byte for ',' */
    totLen = strlen(buf) + objLen + 1; /* 1 byte for '\0' */
    if (buf + totLen > end) return 0; /* buffer length not enough */

    memmove(value+objLen, value, strlen(value)+1); /* 1 byte for '\0' */
    if (comma) *value++ = ',';
	value[0] = '{';
	value[1] = '}';

	return value;
}

char *sjson_array_add_array(char *buf, char *end)
{
    int totLen,objLen;
    char *value = 0;
    int comma = 0;

	if (!buf) return 0; /* Fail on null. */
    if (!skip_array(buf)) return 0; /* check if valid array */

	value = skip(buf); /* skip any spacing */
	value = skip(value+1); /* skip '[' */

	while (*value != ']')
	{
	value=skip(skip_value(value));	/* skip any spacing, skip the value. */
	if (!value) return 0; /* malformed */
		if (*value == ',') {value=skip(value+1);} else {comma = 1;}
	}

    objLen = 2; /* 2byte for "[]" */
    if (comma) objLen += 1; /* 1 byte for ',' */
    totLen = strlen(buf) + objLen + 1; /* 1 byte for '\0' */
    if (buf + totLen > end) return 0; /* buffer length not enough */

    memmove(value+objLen, value, strlen(value)+1); /* 1 byte for '\0' */
    if (comma) *value++ = ',';
	value[0] = '[';
	value[1] = ']';

	return value;
}
