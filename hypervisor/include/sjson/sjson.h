#ifndef __SJSON_H__
#define __SJSON_H__

/* SJSON Types: */
#define SJSON_FALSE			0
#define SJSON_TRUE			1
#define SJSON_NULL			2
#define SJSON_NUMBER			3
#define SJSON_STRING			4
#define SJSON_ARRAY			5
#define SJSON_OBJECT			6

int sjson_is_valid(char *json);
void sjson_minify(char *json);
int sjson_type(char *value);
int sjson_obj_strcmp(const char *obj, const char *name);
char *sjson_parse_string(char *value, char *out, int len);
char *sjson_parse_num(char *value, double *out);

char *sjson_obj_to_next(char *obj, int *type, char **end);
char *sjson_obj_to_value(char *obj, int *type, char **end);

char *sjson_to_idxobj(char *value, unsigned idx, int *type, char **end);
char *sjson_to_idxobjValue(char *value, unsigned idx, int *type, char **end);
char *sjson_to_obj(char *value, const char *name, int *type, char **end);
char *sjson_to_objValue(char *value, const char *name, int *type, char **end);
char *sjson_to_array_value(char *value, unsigned idx, int *type, char **end);
char *sjson_to_path_value(char *value, const char *path, int *type, char **end);
char *sjson_find_obj_value(char *value, const char *name, int *type, char **end);

char *sjson_create_root_obj(char *buf, char *end);
int sjson_obj_size(char *value);
char *sjson_obj_add_string(char *buf, char *end, const char *name, const char* valueStr);
char *sjson_obj_add_num(char *buf, char *end, const char *name, double d);
char *sjson_obj_add_true(char *buf, char *end, const char *name);
char *sjson_obj_add_false(char *buf, char *end, const char *name);
char *sjson_obj_add_null(char *buf, char *end, const char *name);
char *sjson_obj_add_obj(char *buf, char *end, const char *name);
char *sjson_obj_add_array(char *buf, char *end, const char *name);
int sjson_array_size(char *value);
char *sjson_array_add_string(char *buf, char *end, const char* valueStr);
char *sjson_array_add_num(char *buf, char *end, double d);
char *sjson_array_add_true(char *buf, char *end);
char *sjson_array_add_false(char *buf, char *end);
char *sjson_array_add_null(char *buf, char *end);
char *sjson_array_add_obj(char *buf, char *end);
char *sjson_array_add_array(char *buf, char *end);

#endif // __SJSON_H__
