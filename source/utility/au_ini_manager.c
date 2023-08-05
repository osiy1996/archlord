#include "utility/au_ini_manager.h"

#include "core/file_system.h"
#include "core/malloc.h"
#include "core/string.h"

#include "utility/au_md5.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

#define CRLF			"\r\n"
#define CRLF_LENGTH		2
#define HASH_KEY_STRING	"1111"

static void strip_trailing_spaces(char * str)
{
	size_t len = strlen(str);
	size_t tmp = len ? len - 1 : 0;
	char c;
	while (tmp &&
		((c = str[tmp]) == '\r' || c == '\n' || 
			c == '\t' || c == ' ')) {
		str[tmp] = '\0';
		tmp--;
		len--;
	}
}

static size_t print_compact_float(char * str, float value)
{
	size_t len;
	sprintf(str, "%f", value);
	len = strlen(str);
	for (size_t i = len - 1; i > 0; i--) {
		if (str[i] == '0') {
			str[i] = '\0';
		}
		else
			if (str[i] == '.') {
				str[i] = '\0';
				break;
			}
			else break;
	}
	len = strlen(str);
	return len;
}

struct au_ini_mgr_ctx * au_ini_mgr_create()
{
	struct au_ini_mgr_ctx * ctx = alloc(sizeof(*ctx));
	memset(ctx, 0, sizeof(*ctx));
	ctx->process_mode = AU_INI_MGR_PROCESS_MODE_TXT;
	ctx->part_count = 256;
	ctx->part_indices = alloc(
		ctx->part_count * sizeof(*ctx->part_indices));
	au_ini_mgr_clear_all_section_keys(ctx);
	return ctx;
}

void au_ini_mgr_destroy(struct au_ini_mgr_ctx * ctx)
{
	uint32_t i;
	if (ctx->path_name) {
		dealloc(ctx->path_name);
		ctx->path_name = NULL;
	}
	if (ctx->sections) {
		for (i = 0; i < ctx->section_count; i++) {
			if (ctx->sections[i].keys)
				dealloc(ctx->sections[i].keys);
		}
		dealloc(ctx->sections);
		ctx->sections = NULL;
		ctx->section_count = 0;
	}
	if (ctx->binary_sections) {
		for (i = 0; i < ctx->binary_section_count; i++) {
			if (ctx->binary_sections[i].keys)
				dealloc(ctx->binary_sections[i].keys);
		}
		dealloc(ctx->binary_sections);
		ctx->binary_sections = NULL;
		ctx->binary_section_count = 0;
	}
	if (ctx->part_indices) {
		dealloc(ctx->part_indices);
		ctx->part_indices = NULL;
		ctx->part_count = 0;
	}
	if (ctx->key_table) {
		for (uint32_t i = 0; i < ctx->key_table_count; i++)
			dealloc(ctx->key_table[i]);
		ctx->key_table = NULL;
		ctx->key_table_count = 0;
	}
}

const char * au_ini_mgr_get_key_name_table(
	struct au_ini_mgr_ctx * ctx,
	uint32_t index)
{
	assert(index < ctx->key_table_count);
	return ctx->key_table[index];
}

uint32_t au_ini_mgr_get_key_index(
	struct au_ini_mgr_ctx * ctx,
	const char * str)
{
	char ** table;
	uint32_t i;
	for (uint32_t i = 0; i < ctx->key_table_count; i++) {
		if (strcmp(str, ctx->key_table[i]) == 0)
			return i;
	}
	/* Key not found, add a new key to table and returns 
	 * its index. */
	table = alloc(
		((size_t)ctx->key_table_count + 1) * sizeof(*table));
	for (i = 0; i < ctx->key_table_count; i++)
		table[i] = ctx->key_table[i];
	dealloc(ctx->key_table);
	ctx->key_table = table;
	table[ctx->key_table_count] = _strdup(str);
	return ctx->key_table_count++;
}

void au_ini_mgr_set_path(
	struct au_ini_mgr_ctx * ctx,
	const char * path)
{
	if (ctx->path_name)
		dealloc(ctx->path_name);
	ctx->path_name = _strdup(path);
}

void au_ini_mgr_set_process_mode(
	struct au_ini_mgr_ctx * ctx,
	enum au_ini_mgr_process_mode mode)
{
	ctx->process_mode = mode;
}

boolean au_ini_mgr_read_file(
	struct au_ini_mgr_ctx * ctx,
	uint32_t index,
	boolean decrypt)
{
	if (!ctx->path_name || !ctx->path_name[0])
		return FALSE;
	au_ini_mgr_clear_all_section_keys(ctx);
	if (ctx->type & AU_INI_MGR_TYPE_PART_INDEX)
		return au_ini_mgr_from_file(ctx, index, decrypt);
	else
		return au_ini_mgr_from_file(ctx, 0, decrypt);
}

boolean au_ini_mgr_write_file(
	struct au_ini_mgr_ctx * ctx,
	uint32_t index,
	boolean encrypt)
{
	FILE * file = NULL;
	char dummy[AU_INI_MGR_MAX_NAME + AU_INI_MGR_MAX_KEYVALUE + 1];
	boolean	use_key_index = FALSE;
	if (ctx->type & AU_INI_MGR_TYPE_PART_INDEX) {
		fpos_t cur_pos = 0;
		file = fopen(ctx->path_name, "rb");
		if (file) {
			char path[512];
			uint32_t i;
			FILE * output;
			size_t file_size;
			char * buf = NULL;
			size_t bufcount = 0;
			if (!get_file_size(ctx->path_name, &file_size)) {
				fclose(file);
				return FALSE;
			}
			if (!au_ini_mgr_read_part_indices(ctx, file)) {
				fclose(file);
				return FALSE;
			}
			if (index >= ctx->part_count) {
				fclose(file);
				return FALSE;
			}
			if (!make_path(path, sizeof(path), "%s.tmp", 
					ctx->path_name)) {
				fclose(file);
				return FALSE;
			}
			output = fopen(path, "wb");
			if (!output) {
				fclose(file);
				return FALSE;
			}
			if (!au_ini_mgr_write_part_indices(ctx, output)) {
				fclose(output);
				fclose(file);
				return FALSE;
			}
			for (i = 0; i < ctx->part_count; i++) {
				uint32_t size;
				uint32_t offset = (uint32_t)ftell(output);
				if (i + 1 == ctx->part_count) {
					size = (uint32_t)(file_size - 
						ctx->part_indices[i]);
				}
				else {
					size = ctx->part_indices[i + 1] - 
						ctx->part_indices[i];
				}
				if (i == index) {
					uint32_t j;
					for (j = 0; j < ctx->section_count; j++) {
						uint32_t k;
						uint32_t key_count = 
							au_ini_mgr_get_key_count(ctx, j);
						snprintf(dummy, sizeof(dummy), "[%s]\r\n", 
							ctx->sections[j].section_name);
						fputs(dummy, output);
						for (k = 0; k < key_count; k++) {
							const char * key_name =
								au_ini_mgr_get_key_name_table(ctx, 
									ctx->sections[j].keys[k].key_index);
							snprintf(dummy, sizeof(dummy), "%s=%s\r\n", 
								key_name, 
								ctx->sections[j].keys[k].key_value);
							fputs(dummy, output);
						}
					}
				}
				else if (size) {
					if (size > bufcount){ 
						buf = reallocate(buf, size);
						bufcount = size;
					}
					if (fseek(file, ctx->part_indices[i], 
							SEEK_SET) != 0 ||
						fread(buf, size, 1, file) != 1 ||
						fwrite(buf, size, 1, output) != 1) {
						dealloc(buf);
						fclose(output);
						fclose(file);
						return FALSE;
					}
				}
				ctx->part_indices[i] = offset;
			}
			dealloc(buf);
			if (!au_ini_mgr_write_part_indices(ctx, output)) {
				fclose(output);
				fclose(file);
				return FALSE;
			}
			fclose(output);
			fclose(file);
			if (!replace_file(ctx->path_name, path))
				return FALSE;
			return TRUE;
		}
		else {
			uint32_t i;
			if (index >= ctx->part_count)
				return FALSE;
			file = fopen(ctx->path_name, "w");
			if (!file)
				return FALSE;
			if (!au_ini_mgr_write_part_indices(ctx, file)) {
				fclose(file);
				return FALSE;
			}
			fgetpos(file, &cur_pos);
			ctx->part_indices[index] = (uint32_t)cur_pos;
			for (i = 0; i < ctx->section_count; i++) {
				uint32_t j;
				uint32_t key_count = 
					au_ini_mgr_get_key_count(ctx, i);
				snprintf(dummy, sizeof(dummy), "[%s]\n", 
					ctx->sections[i].section_name);
				fputs(dummy, file);
				for (j = 0; j < key_count; j++) {
					const char * key_name =
						au_ini_mgr_get_key_name_table(ctx, 
							ctx->sections[i].keys[j].key_index);
					snprintf(dummy, sizeof(dummy), "%s=%s\n", 
						key_name, ctx->sections[i].keys[j].key_value);
					fputs(dummy, file);
				}
			}
			fgetpos(file, &cur_pos);
			for (i = index + 1; i < ctx->part_count; i++)
				ctx->part_indices[i] = (uint32_t)cur_pos;
			if (!au_ini_mgr_write_part_indices(ctx, file)) {
				fclose(file);
				return FALSE;
			}
			fclose(file);
			return TRUE;
		}
		/*
		if (index)
			file = fopen(ctx->path_name, "rt");
		if (file) {
			boolean r = au_ini_mgr_read_part_indices(ctx, file);
			fclose(file);
			if (!r)
				return FALSE;
		}
		else {
			file = fopen(ctx->path_name, "wt");
			if (!file)
				return FALSE;
			fclose(file);
			memset(ctx->part_indices, 0, ctx->part_count * sizeof(int));
		}
		file = fopen(ctx->path_name, "r+t");
		if (!file)
			return FALSE;
		au_ini_mgr_write_part_indices(ctx, file);
		fseek(file, 0, SEEK_END);
		fgetpos(file, &cur_pos);
		for (uint32_t i = 0; i < ctx->section_count; i++) {
			uint32_t key_count = au_ini_mgr_get_key_count(ctx, i);
			snprintf(dummy, sizeof(dummy), "[%s]\n", 
				ctx->sections[i].section_name);
			fputs(dummy, file);
			for (uint32_t j = 0; j < key_count; j++) {
				const char * key_name =
					au_ini_mgr_get_key_name_table(ctx, 
						ctx->sections[i].keys[j].key_index);
				snprintf(dummy, sizeof(dummy), "%s=%s\n", 
					key_name, ctx->sections[i].keys[j].key_value);
				fputs(dummy, file);
			}
		}
		ctx->part_indices[index] = (uint32_t)cur_pos;
		au_ini_mgr_write_part_indices(ctx, file);
		fclose(file);
		*/
		return TRUE;
	}
	file = fopen(ctx->path_name, "wt");
	if (!file)
		return FALSE;
	use_key_index = (ctx->type & AU_INI_MGR_TYPE_KEY_INDEX) ? 
		TRUE : FALSE;
	if (use_key_index) {
		for (uint32_t i = 0; i < ctx->key_table_count; i++) {
			const char * key_name = au_ini_mgr_get_key_name_table(
				ctx, i);
			snprintf(dummy, sizeof(dummy), "%u=%s\n", i, key_name);
			fputs(dummy, file);
		}
	}
	for (uint32_t i = 0; i < ctx->section_count; i++) {
		struct au_ini_mgr_section * section = &ctx->sections[i];
		uint32_t key_count = au_ini_mgr_get_key_count(ctx, i);
		snprintf(dummy, sizeof(dummy), "[%s]\n",
			section->section_name);
		fputs(dummy, file);
		for (uint32_t j = 0; j < key_count; j++) {
			if (use_key_index) {
				snprintf(dummy, sizeof(dummy), "%u=%s\n",
					section->keys[j].key_index, section->keys[j].key_value);
				fputs(dummy, file);
			}
			else {
				const char * key_name = 
					au_ini_mgr_get_key_name_table(ctx, 
						section->keys[j].key_index);
				snprintf(dummy, sizeof(dummy), "%s=%s\n",
					key_name, section->keys[j].key_value);
				fputs(dummy, file);
			}
		}
	}
	fclose(file);
	if (!encrypt)
		return TRUE;
	return au_ini_mgr_encrypt_save(ctx);
}

static uint32_t adv_by_crlf(const char * buffer, uint32_t offset)
{
	for (uint32_t i = 0; i < CRLF_LENGTH; i++) {
		if (!buffer[offset])
			return offset;
		if (buffer[offset] != CRLF[i])
			return offset;
		offset++;
	}
	return offset;
}

static uint32_t read_line_from_buffer(
	char * dst, 
	size_t dst_max_count, 
	const char * buffer)
{
	char c;
	char * d = dst;
	char * e = dst + dst_max_count;
	const char * s = buffer;
	if (!dst_max_count)
		return 0;
	if (dst_max_count == 1) {
		dst[0] = '\0';
		return 0;
	}
	while (d + 1 < e && (c = *s++) != '\n' && c != '\0') {
		if (c == '\r' && *s == '\n')
			continue;
		*d++ = c;
	}
	*d = '\0';
	return (uint32_t)(s - buffer);
}

static void midstr(
	char * dst, 
	size_t dst_max_count, 
	const char * str, 
	char token)
{
	const char * cursor;
	size_t n;
	if (!dst_max_count)
		return;
	cursor = strchr(str, token);
	if (!cursor) {
		strlcpy(dst, str, dst_max_count);
		return;
	}
	n = MIN(dst_max_count - 1, (size_t)(cursor - str));
	memcpy(dst, str, n);
	dst[n] = '\0';
}

boolean au_ini_mgr_from_memory(
	struct au_ini_mgr_ctx * ctx,
	void * data,
	uint32_t data_size,
	boolean decrypt)
{
	char line[AU_INI_MGR_MAX_NAME + AU_INI_MGR_MAX_KEYVALUE + 1];
	size_t line_len;
	uint32_t file_section_count = 0;
	static uint32_t file_key_count[AU_INI_MGR_MAX_SECTIONCOUNT];
	size_t offset_bytes = 0;
	size_t offset = 0;
	char * buffer = NULL;
	size_t tmp;
	uint32_t key_index_count = 0;
	boolean use_key_index = FALSE;
	if (decrypt) {
		if (!au_md5_crypt(data, data_size, "1111", 4))
			return FALSE;
	}
	buffer = (char *)alloc((size_t)data_size + 1);
	memcpy(buffer, data, data_size);
	buffer[data_size] = '\0';
	memset(file_key_count, 0, sizeof(file_key_count));
	while (offset_bytes < data_size) {
		offset = read_line_from_buffer(line, sizeof(line),
			buffer + offset_bytes);
		offset_bytes += offset;
		if (line[0] == '[') {
			assert(file_section_count < AU_INI_MGR_MAX_SECTIONCOUNT);
			if (file_section_count < AU_INI_MGR_MAX_SECTIONCOUNT)
				file_section_count++;
		}
		else if (strchr(line, '=')) {
			if (file_section_count)
				file_key_count[file_section_count - 1]++;
			else
				key_index_count++;
		}
	}
	ctx->section_count = file_section_count;
	ctx->sections = alloc(
		ctx->section_count * sizeof(*ctx->sections));
	memset(ctx->sections, 0,
		ctx->section_count * sizeof(*ctx->sections));
	ctx->half_section_count = ctx->section_count / 2;
	ctx->is_section_count_odd = (ctx->section_count % 2) ? 1 : 0;
	// TODO: Allocate all keys in one block.
	for (uint32_t i = 0; i < ctx->section_count; i++) {
		ctx->sections[i].key_count = 0;
		if (file_key_count[i]) {
			ctx->sections[i].keys = alloc(
				file_key_count[i] * sizeof(*ctx->sections[i].keys));
		}
		else {
			ctx->sections[i].keys = NULL;
		}
	}
	if (key_index_count)
		use_key_index = TRUE;
	offset_bytes = 0;
	file_section_count = 0;
	memset(file_key_count, 0, sizeof(file_key_count));
	while (offset_bytes < data_size) {
		char c;
		const char * cursor;
		char key_name[AU_INI_MGR_MAX_NAME] = "";
		char key_value[AU_INI_MGR_MAX_KEYVALUE] = "";
		offset = read_line_from_buffer(line, sizeof(line),
			buffer + offset_bytes);
		assert(offset);
		offset_bytes += offset;
		line_len = strlen(line);
		tmp = line_len ? line_len - 1 : 0;
		while (tmp &&
			((c = line[tmp]) == '\r' || c == '\n' || c == '\t' || c == ' ')) {
			line[tmp] = '\0';
			tmp--;
			line_len--;
		}
		if (line[0] == '[') {
			midstr(ctx->sections[file_section_count].section_name, 
				AU_INI_MGR_MAX_NAME, line + 1, ']');
			file_section_count++;
		}
		cursor = strchr(line, '=');
		if (!cursor)
			continue;
		midstr(key_name, sizeof(key_name), line, '=');
		strcpy_s(key_value, sizeof(key_value), cursor + 1);
		if (file_section_count) {
			uint32_t sidx = file_section_count - 1;
			uint32_t kidx = file_key_count[sidx];
			struct au_ini_mgr_section * section =
				&ctx->sections[sidx];
			if (use_key_index) {
				section->keys[kidx].key_index = strtoul(
					key_name, NULL, 10);
			}
			else {
				section->keys[kidx].key_index =
					au_ini_mgr_get_key_index(ctx, key_name);
			}
			strcpy(section->keys[kidx].key_value, key_value);
			section->key_count++;
			file_key_count[sidx]++;
		}
		else if (use_key_index) {
			if (key_name[0] && key_value[0]) {
				uint32_t key_index = strtoul(key_name, NULL, 10);
				uint32_t key_index_r = au_ini_mgr_get_key_index(
					ctx, key_value);
				assert(key_index == key_index_r);
			}
		}
	}
	dealloc(buffer);
	return TRUE;
}

boolean au_ini_mgr_from_file(
	struct au_ini_mgr_ctx * ctx,
	uint32_t index,
	boolean decrypt)
{
	void * data;
	size_t data_size = 0;
	boolean r;
	if (ctx->type & AU_INI_MGR_TYPE_PART_INDEX) {
		FILE * file = fopen(ctx->path_name, "rb");
		int start;
		int end;
		if (!file)
			return FALSE;
		if (!au_ini_mgr_read_part_indices(ctx, file) ||
			index >= ctx->part_count) {
			fclose(file);
			return FALSE;
		}
		start = ctx->part_indices[index];
		if (fseek(file, start, SEEK_SET) != 0) {
			fclose(file);
			return FALSE;
		}
		if (index + 1 < ctx->part_count) {
			end = ctx->part_indices[index + 1];
		}
		else {
			fseek(file, 0, SEEK_END);
			end = ftell(file);
			fseek(file, start, SEEK_SET);
		}
		data_size = (size_t)(end - start);
		data = alloc(data_size);
		fread(data, 1, data_size, file);
		fclose(file);
	}
	else {
		if (!get_file_size(ctx->path_name, &data_size))
			return FALSE;
		data = alloc(data_size);
		if (!load_file(ctx->path_name, data, data_size)) {
			dealloc(data);
			return FALSE;
		}
	}
	r = au_ini_mgr_from_memory(ctx,
		data, (uint32_t)data_size, decrypt);
	dealloc(data);
	return r;
}

static boolean println_to_buf(
	void *			buffer, 
	size_t			buffer_size, 
	void **			cursor,
	const char *	fmt, ...)
{
	va_list ap;
	size_t len;
	void * cur = *cursor;
	size_t remain = buffer_size - ((uintptr_t)cur - (uintptr_t)buffer);
	va_start(ap, fmt);
	len = vsnprintf(NULL, 0, fmt, ap);
	if (len + 2 > remain) {
		va_end(ap);
		return FALSE;
	}
	len = vsnprintf((char *)cur, remain, fmt, ap);
	va_end(ap);
	*(char *)((uintptr_t)cur + len) = '\r';
	*(char *)((uintptr_t)cur + len + 1) = '\n';
	*cursor = (void *)((uintptr_t)cur + len + 2);
	return TRUE;
}

boolean au_ini_mgr_write_to_memory(
	struct au_ini_mgr_ctx * ctx,
	void * buffer, 
	size_t buffer_size, 
	uint32_t * written_count,
	boolean encrypt)
{
	boolean use_key_index = FALSE;
	void * cursor = buffer;
	uint32_t count = 0;
	if (ctx->type & AU_INI_MGR_TYPE_PART_INDEX)
		return FALSE;
	use_key_index = (ctx->type & AU_INI_MGR_TYPE_KEY_INDEX) ? 
		TRUE : FALSE;
	if (use_key_index) {
		for (uint32_t i = 0; i < ctx->key_table_count; i++) {
			const char * key_name = au_ini_mgr_get_key_name_table(
				ctx, i);
			if (!println_to_buf(buffer, buffer_size, &cursor,
					"%u=%s", i, key_name))
				return FALSE;
		}
	}
	for (uint32_t i = 0; i < ctx->section_count; i++) {
		struct au_ini_mgr_section * section = &ctx->sections[i];
		if (!println_to_buf(buffer, buffer_size, &cursor,
			"[%s]", section->section_name))
			return FALSE;
		for (uint32_t j = 0; j < au_ini_mgr_get_key_count(ctx, i); j++) {
			if (use_key_index) {
				if (!println_to_buf(buffer, buffer_size, &cursor, "%u=%s", 
					section->keys[j].key_index, section->keys[j].key_value))
					return FALSE;
			}
			else {
				const char * key_name =
					au_ini_mgr_get_key_name_table(ctx, 
						section->keys[j].key_index);
				if (!println_to_buf(buffer, buffer_size, &cursor, "%s=%s", 
					key_name, section->keys[j].key_value))
					return FALSE;
			}
		}
	}
	count = (uint32_t)((uintptr_t)cursor - (uintptr_t)buffer);
	if (encrypt && !au_md5_crypt(buffer, count, HASH_KEY_STRING, 4))
		return FALSE;
	*written_count = count;
	return TRUE;
}

void au_ini_mgr_clear_all_section_keys(
	struct au_ini_mgr_ctx * ctx)
{
	if (ctx->sections) {
		for (uint32_t i = 0; i < ctx->section_count; i++) {
			if (ctx->sections[i].keys)
				dealloc(ctx->sections[i].keys);
		}
		dealloc(ctx->sections);
		ctx->sections = NULL;
	}
	ctx->section_count = 0;
	if (ctx->binary_sections) {
		for (uint32_t i = 0; i < ctx->binary_section_count; i++) {
			if (ctx->binary_sections[i].keys)
				dealloc(ctx->binary_sections[i].keys);
		}
		dealloc(ctx->binary_sections);
	}
	ctx->binary_section_count = 0;
	ctx->half_section_count = 0;
	ctx->is_section_count_odd = 0;
}

const char * au_ini_mgr_get_section_name(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id)
{
	if (section_id >= ctx->section_count)
		return NULL;
	return ctx->sections[section_id].section_name;
}

uint32_t au_ini_mgr_get_key_count(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id)
{
	if (section_id >= ctx->section_count)
		return 0;
	if (au_ini_mgr_is_process_mode(ctx, AU_INI_MGR_PROCESS_MODE_TXT))
		return ctx->sections[section_id].key_count;
	else if (au_ini_mgr_is_process_mode(ctx, AU_INI_MGR_PROCESS_MODE_BIN))
		return ctx->binary_sections[section_id].key_count;
	return 0;
}

const char * au_ini_mgr_get_key_name(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id, 
	uint32_t key_id)
{
	if (section_id > ctx->section_count)
		return NULL;
	if (key_id > au_ini_mgr_get_key_count(ctx, section_id))
		return NULL;
	return au_ini_mgr_get_key_name_table(ctx, 
		ctx->sections[section_id].keys[key_id].key_index);
}

uint32_t au_ini_mgr_find_section(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name)
{
	if (au_ini_mgr_is_process_mode(ctx, AU_INI_MGR_PROCESS_MODE_TXT)) {
		for (uint32_t i = 0; i < ctx->section_count; i++) {
			if (strcmp(section_name, ctx->sections[i].section_name) == 0)
				return i;
		}
	}
	else if (au_ini_mgr_is_process_mode(ctx, AU_INI_MGR_PROCESS_MODE_BIN)) {
		for (uint32_t i = 0; i < ctx->binary_section_count; i++) {
			if (strcmp(section_name, ctx->binary_sections[i].section_name) == 0)
				return i;
		}
	}
	return UINT32_MAX;
}

uint32_t au_ini_mgr_find_key_by_name(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name, 
	const char * key_name)
{
	uint32_t section_id = au_ini_mgr_find_section(ctx, section_name);
	if (section_id == UINT32_MAX)
		return UINT32_MAX;
	return au_ini_mgr_find_key(ctx, section_id, key_name);
}

uint32_t au_ini_mgr_find_key(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id, 
	const char * key_name)
{
	uint32_t key_index;
	if (section_id >= ctx->section_count)
		return UINT32_MAX;
	key_index = au_ini_mgr_get_key_index(ctx, key_name);
	if (au_ini_mgr_is_process_mode(ctx, AU_INI_MGR_PROCESS_MODE_TXT)) {
		struct au_ini_mgr_section * section =
			&ctx->sections[section_id];
		for (uint32_t i = 0; i < section->key_count; i++) {
			if (section->keys[i].key_index == key_index)
				return i;
		}
		return UINT32_MAX;
	}
	else if (au_ini_mgr_is_process_mode(ctx, AU_INI_MGR_PROCESS_MODE_BIN)) {
		struct au_ini_mgr_section_bin * section = 
			&ctx->binary_sections[section_id];
		for (uint32_t i = 0; i < section->key_count; i++) {
			if (section->keys[i].key_index == key_index)
				return i;
		}
		return UINT32_MAX;
	}
	return UINT32_MAX;
}

uint32_t au_ini_mgr_add_section(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name)
{
	uint32_t find_index = au_ini_mgr_find_section(ctx, 
		section_name);
	if (find_index != UINT32_MAX)
		return UINT32_MAX;
	if (au_ini_mgr_is_process_mode(ctx, 
			AU_INI_MGR_PROCESS_MODE_TXT)) {
		if (ctx->section_count >= AU_INI_MGR_MAX_SECTIONCOUNT)
			return UINT32_MAX;
		ctx->sections = reallocate(ctx->sections, 
			((size_t)ctx->section_count + 1) * sizeof(*ctx->sections));
		ctx->half_section_count = ctx->section_count / 2;
		ctx->is_section_count_odd =
			(ctx->section_count % 2) ? TRUE : FALSE;
		ctx->sections[ctx->section_count].key_count = 0;
		ctx->sections[ctx->section_count].keys = NULL;
		strcpy_s(ctx->sections[ctx->section_count].section_name, 
			AU_INI_MGR_MAX_NAME, section_name);
		return ctx->section_count++;
	}
	else if (au_ini_mgr_is_process_mode(ctx, 
			AU_INI_MGR_PROCESS_MODE_BIN)) {
		if (ctx->binary_section_count >= AU_INI_MGR_MAX_SECTIONCOUNT)
			return UINT32_MAX;
		ctx->binary_sections = reallocate(ctx->binary_sections, 
			((size_t)ctx->binary_section_count + 1) * sizeof(*ctx->binary_sections));
		ctx->half_section_count = ctx->binary_section_count / 2;
		ctx->is_section_count_odd = (ctx->binary_section_count % 2) ? TRUE : FALSE;
		ctx->binary_sections[ctx->binary_section_count].key_count = 0;
		ctx->binary_sections[ctx->binary_section_count].keys = NULL;
		strlcpy(ctx->binary_sections[ctx->binary_section_count].section_name, 
			section_name, AU_INI_MGR_MAX_NAME);
		return ctx->binary_section_count++;
	}
	return UINT32_MAX;
}

const char * au_ini_mgr_get_value(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id,
	uint32_t key_id)
{
	if (section_id >= ctx->section_count || 
		key_id >= au_ini_mgr_get_key_count(ctx, section_id))
		return "";
	return ctx->sections[section_id].keys[key_id].key_value;
}

const char * au_ini_mgr_get_value_by_name(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name,
	const char * key_name)
{
	uint32_t section_id = au_ini_mgr_find_section(ctx, 
		section_name);
	uint32_t key_id;
	if (section_id == UINT32_MAX)
		return "";
	key_id = au_ini_mgr_find_key(ctx, section_id, key_name);
	if (key_id == UINT32_MAX)
		return "";
	return ctx->sections[section_id].keys[key_id].key_value;
}

int32_t au_ini_mgr_get_i32(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id,
	uint32_t key_id)
{
	const char * value = au_ini_mgr_get_value(ctx, section_id,
		key_id);
	if (!value)
		return 0;
	return strtol(value, NULL, 10);
}

int64_t au_ini_mgr_get_i64(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id,
	uint32_t key_id)
{
	const char * value = au_ini_mgr_get_value(
		ctx, section_id, key_id);
	if (!value)
		return 0;
	return strtoll(value, NULL, 10);
}

double au_ini_mgr_get_f64(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id,
	uint32_t key_id)
{
	const char * value = au_ini_mgr_get_value(
		ctx, section_id, key_id);
	if (!value)
		return 0.0;
	return strtod(value, NULL);
}

boolean au_ini_mgr_get_str_by_name(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name,
	const char * key_name,
	char * dst,
	uint32_t dst_length)
{
	const char * value = au_ini_mgr_get_value_by_name(
		ctx, section_name, key_name);
	if (!value) {
		strlcpy(dst, "", dst_length);
		return FALSE;
	}
	strlcpy(dst, value, dst_length);
	return TRUE;
}

boolean au_ini_mgr_get_str(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id,
	uint32_t key_id,
	char * dst,
	uint32_t dst_length)
{
	const char * value = au_ini_mgr_get_value(
		ctx, section_id, key_id);
	if (!value) {
		strlcpy(dst, "", dst_length);
		return FALSE;
	}
	strlcpy(dst, value, dst_length);
	return TRUE;
}

boolean au_ini_mgr_set_value(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name,
	const char * key_name,
	const char * value)
{
	union au_ini_mgr_key_bin_data data = { 0 };
	strlcpy(data.str, value, sizeof(data.str));
	return au_ini_mgr_set_value_by_name(ctx, section_name, 
		key_name, AU_INI_MGR_KEY_TYPE_STRING, data);
}

boolean au_ini_mgr_set_i32(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name,
	const char * key_name,
	int32_t value)
{
	union au_ini_mgr_key_bin_data data = { 0 };
	data.i = value;
	return au_ini_mgr_set_value_by_name(ctx, section_name, 
		key_name, AU_INI_MGR_KEY_TYPE_INT, data);
}

boolean au_ini_mgr_set_i64(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name,
	const char * key_name,
	int64_t value)
{
	union au_ini_mgr_key_bin_data data = { 0 };
	data.i64 = value;
	return au_ini_mgr_set_value_by_name(ctx, section_name, 
		key_name, AU_INI_MGR_KEY_TYPE_INT64, data);
}

boolean au_ini_mgr_set_f32(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name,
	const char * key_name,
	float value)
{
	union au_ini_mgr_key_bin_data data = { 0 };
	data.f = value;
	return au_ini_mgr_set_value_by_name(ctx, section_name, 
		key_name, AU_INI_MGR_KEY_TYPE_FLOAT, data);
}

boolean au_ini_mgr_set_mode(
	struct au_ini_mgr_ctx * ctx,
	enum au_ini_mgr_mode mode)
{
	if (mode >= AU_INI_MGR_MODE_COUNT)
		return FALSE;
	ctx->mode = mode;
	return TRUE;
}

boolean au_ini_mgr_set_type(
	struct au_ini_mgr_ctx * ctx,
	enum au_ini_mgr_type type)
{
	if (type >= AU_INI_MGR_TYPE_COUNT)
		return FALSE;
	ctx->type = type;
	return TRUE;
}

boolean au_ini_mgr_read_part_indices_buffer(
	struct au_ini_mgr_ctx * ctx,
	char * buffer)
{
	char line[400];
	uint32_t offset_bytes = 0;
	uint32_t offset = 0;
	if (!read_line_from_buffer(line, sizeof(line), buffer))
		return FALSE;
	strip_trailing_spaces(line);
	ctx->part_count = strtoul(line, NULL, 10);
	if (ctx->part_indices)
		dealloc(ctx->part_indices);
	ctx->part_indices = alloc(ctx->part_count * sizeof(uint32_t));
	for (uint32_t i = 0; i < ctx->part_count; i++) {
		offset = read_line_from_buffer(line, sizeof(line),
			buffer + offset_bytes);
		assert(offset);
		offset_bytes += offset;
		strip_trailing_spaces(line);
		ctx->part_indices[i] = strtoul(line, NULL, 10);
	}
	return TRUE;
}

boolean au_ini_mgr_read_part_indices(
	struct au_ini_mgr_ctx * ctx,
	FILE * file)
{
	if (!fscanf(file, "%10u\n", &ctx->part_count))
		return FALSE;
	if (ctx->part_indices)
		dealloc(ctx->part_indices);
	ctx->part_indices = (uint32_t *)alloc(ctx->part_count * sizeof(*ctx->part_indices));
	for (uint32_t i = 0; i < ctx->part_count; i++) {
		if (!fscanf(file, "%10u\n", &ctx->part_indices[i]))
			return FALSE;
	}
	return TRUE;
}

boolean au_ini_mgr_write_part_indices(
	struct au_ini_mgr_ctx * ctx,
	FILE * file)
{
	uint32_t i;
	if (fseek(file, 0, SEEK_SET) != 0)
		return FALSE;
	fprintf(file, "%10u\r\n", ctx->part_count);
	for (i = 0; i < ctx->part_count; i++)
		fprintf(file, "%10u\r\n", ctx->part_indices[i]);
	return TRUE;
}

boolean au_ini_mgr_encrypt_save(struct au_ini_mgr_ctx * ctx)
{
	size_t size = 0;
	void * data;
	FILE * file;
	boolean r;
	if (!get_file_size(ctx->path_name, &size))
		return FALSE;
	data = alloc(size);
	if (!load_file(ctx->path_name, data, size)) {
		dealloc(data);
		return FALSE;
	}
	if (!au_md5_crypt(data, size, HASH_KEY_STRING, 4)) {
		dealloc(data);
		return FALSE;
	}
	file = fopen(ctx->path_name, "wb");
	if (!file) {
		dealloc(data);
		return FALSE;
	}
	r = (fwrite(data, size, 1, file) == 1);
	fclose(file);
	dealloc(data);
	return r;
}

boolean au_ini_mgr_set_value_by_id(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id,
	uint32_t key_id,
	enum au_ini_mgr_key_type type,
	union au_ini_mgr_key_bin_data data)
{
	if (section_id >= ctx->section_count ||
		key_id >= au_ini_mgr_get_key_count(ctx, section_id))
		return FALSE;
	if (au_ini_mgr_is_process_mode(ctx, 
			AU_INI_MGR_PROCESS_MODE_TXT)) {
		struct au_ini_mgr_key * key =
			&ctx->sections[section_id].keys[key_id];
		key->type = type;
		switch (type) {
		case AU_INI_MGR_KEY_TYPE_INT:
			snprintf(key->key_value, sizeof(key->key_value), "%d", data.i);
			break;
		case AU_INI_MGR_KEY_TYPE_INT64:
			snprintf(key->key_value, sizeof(key->key_value), "%lld", data.i64);
			break;
		case AU_INI_MGR_KEY_TYPE_FLOAT:
			print_compact_float(key->key_value, data.f);
			break;
		case AU_INI_MGR_KEY_TYPE_STRING:
			snprintf(key->key_value, sizeof(key->key_value), "%s", data.str);
			break;
		default:
			return FALSE;
		}
	}
	else if (au_ini_mgr_is_process_mode(ctx, 
			AU_INI_MGR_PROCESS_MODE_BIN)) {
		struct au_ini_mgr_key_bin * key =
			&ctx->binary_sections[section_id].keys[key_id];
		key->type = type;
		key->data = data;
	}
	return TRUE;
}

boolean au_ini_mgr_set_value_by_name(
	struct au_ini_mgr_ctx * ctx,
	const char * section_name,
	const char * key_name,
	enum au_ini_mgr_key_type type,
	union au_ini_mgr_key_bin_data data)
{
	uint32_t section_id = au_ini_mgr_find_section(
		ctx, section_name);
	uint32_t key_id;
	if (section_id == UINT32_MAX) {
		section_id = au_ini_mgr_add_section(ctx, section_name);
		if (section_id == UINT32_MAX)
			return FALSE;
	}
	if (ctx->mode != AU_INI_MGR_MODE_NAME_OVERWRITE)
		key_id = au_ini_mgr_find_key(ctx, section_id, key_name);
	else
		key_id = UINT32_MAX;
	if (key_id == UINT32_MAX) {
		key_id = au_ini_mgr_add_key(ctx, section_id, 
			key_name, type, data);
		if (key_id == UINT32_MAX)
			return FALSE;
		return TRUE;
	}
	return au_ini_mgr_set_value_by_id(ctx, section_id, key_id, 
		type, data);
}


uint32_t au_ini_mgr_add_key(
	struct au_ini_mgr_ctx * ctx,
	uint32_t section_id,
	const char * key_name,
	enum au_ini_mgr_key_type type,
	union au_ini_mgr_key_bin_data data)
{
	uint32_t key_id;
	if (section_id >= ctx->section_count)
		return UINT32_MAX;
	if (ctx->mode != AU_INI_MGR_MODE_NAME_OVERWRITE) {
		key_id = au_ini_mgr_find_key(ctx, section_id, key_name);
		if (key_id != UINT32_MAX)
			return UINT32_MAX;
	}
	if (au_ini_mgr_is_process_mode(ctx,
			AU_INI_MGR_PROCESS_MODE_TXT)) {
		struct au_ini_mgr_section * section =
			&ctx->sections[section_id];
		key_id = section->key_count;
		section->keys = reallocate(section->keys,
			++section->key_count * sizeof(*section->keys));
		section->keys[key_id].key_index = 
			au_ini_mgr_get_key_index(ctx, key_name);
		au_ini_mgr_set_value_by_id(ctx, section_id, key_id, 
			type, data);
		return key_id;
	}
	else if (au_ini_mgr_is_process_mode(ctx, 
			AU_INI_MGR_PROCESS_MODE_BIN)) {
		struct au_ini_mgr_section_bin * section = 
			&ctx->binary_sections[section_id];
		key_id = section->key_count;
		section->keys = reallocate(section->keys,
			++section->key_count * sizeof(*section->keys));
		section->keys[key_id].key_index = 
			au_ini_mgr_get_key_index(ctx, key_name);
		au_ini_mgr_set_value_by_id(ctx, section_id, key_id, 
			type, data);
		return key_id;
	}
	return UINT32_MAX;
}

size_t au_ini_mgr_print_compact(char * dst, const char * fmt, ...)
{
	va_list			ap;
	uint32_t		sum = 0;
	const char *	cur_fmt = fmt;
	char *			cur_str = dst;
	boolean			arg = FALSE;
	va_start(ap, fmt);
	while (*cur_fmt != '\0') {
		if (arg) {
			switch (*cur_fmt) {
			case 'f':
			case 'F': {
				float value = (float)(va_arg(ap, double));
				cur_str += print_compact_float(cur_str, value);
				break;
			}
			case 'd':
			case 'D': {
				int value = va_arg(ap, int);
				cur_str += sprintf(cur_str, "%d", value);
				break;
			}
			case 's':
			case 'S': {
				const char * value = va_arg(ap, const char *);
				cur_str += sprintf(cur_str, "%s", value);
				break;
			}
			case 'x':
			case 'X': {
				int value = va_arg(ap, int);
				cur_str += sprintf(cur_str, "%x", value);
				break;
			}
			}
			arg = FALSE;
		}
		else {
			if (*cur_fmt == '%') {
				arg = TRUE;
			}
			else {
				arg = FALSE;
				*cur_str = *cur_fmt;
				cur_str++;
				*cur_str = '\0';
			}
		}
		cur_fmt++;
	}
	va_end(ap);
	return strlen(dst);
}
