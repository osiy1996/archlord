#include "utility/au_table.h"

#include "core/file_system.h"
#include "core/log.h"
#include "core/malloc.h"
#include "core/string.h"

#include <assert.h>
#include <stdlib.h>

#define MAX_VALUE_SIZE 256

struct au_table {
	file file;
	char path[1024];
	char line_buf[8096];
	char * cursor;
	char column_names[AU_TABLE_MAX_COLUMN_COUNT][AU_TABLE_MAX_COLUMN_SIZE];
	uint32_t column_ids[AU_TABLE_MAX_COLUMN_COUNT];
	boolean column_parse_empty[AU_TABLE_MAX_COLUMN_COUNT];
	uint32_t column_count;
	uint32_t column_map[AU_TABLE_MAX_COLUMN_COUNT];
	uint32_t current_column_id;
	uint32_t current_setup_index;
	uint32_t current_column_index;
	char current_value[MAX_VALUE_SIZE];
	boolean parsed_header;
	boolean end_of_line;
};

static boolean parse_header(struct au_table * t)
{
	char * col;
	uint32_t index = 0;
	if (!read_line(t->file, t->line_buf, sizeof(t->line_buf)))
		return FALSE;
	col = strtok(t->line_buf, "\t");
	while (col) {
		uint32_t i;
		boolean mapped = FALSE;
		if (index >= AU_TABLE_MAX_COLUMN_COUNT) {
			assert(0);
			return FALSE;
		}
		for (i = 0; i < t->column_count; i++) {
			if (strcmp(col, t->column_names[i]) == 0) {
				t->column_map[index] = i;
				mapped = TRUE;
				break;
			}
		}
		if (!mapped) {
			WARN("Unrecognized column name (%s) in file (%s).", 
				col, t->path);
			t->column_map[index] = AU_TABLE_INVALID_COLUMN;
		}
		col = strtok(NULL, "\t");
		index++;
	}
	return TRUE;
}

struct au_table * au_table_open(
	const char * file_path, 
	boolean decrypt)
{
	file f = open_file(file_path, FILE_ACCESS_READ);
	struct au_table * t;
	uint32_t i;
	if (!f)
		return NULL;
	t = alloc(sizeof(*t));
	memset(t, 0, sizeof(*t));
	strlcpy(t->path, file_path, sizeof(t->path));
	t->file = f;
	t->cursor = t->line_buf;
	for (i = 0; i < AU_TABLE_MAX_COLUMN_COUNT; i++)
		t->column_map[i] = AU_TABLE_INVALID_COLUMN;
	return t;
}

struct au_table * au_table_open_fmt(
	const char * format, 
	boolean decrypt, ...)
{
	char path[1024];
	va_list ap;
	va_start(ap, decrypt);
	va_end(ap);
	if (!make_pathv(path, sizeof(path), format, ap))
		return NULL;
	return au_table_open(path, decrypt);
}

void au_table_destroy(struct au_table * table)
{
	if (table->file){
		close_file(table->file);
		table->file = NULL;
	}
	dealloc(table);
}

boolean au_table_set_column(
	struct au_table * table, 
	const char * column_name,
	uint32_t column_id)
{
	uint32_t i;
	if (table->column_count >= AU_TABLE_MAX_COLUMN_COUNT)
		return FALSE;
	i = table->column_count;
	if (strlcpy(table->column_names[i], column_name, 
			sizeof(table->column_names[i])) >= sizeof(table->column_names[i])) {
		ERROR("Column name length is too long (%s).",
			column_name);
		return FALSE;
	}
	table->column_ids[i] = column_id;
	table->column_parse_empty[i] = FALSE;
	table->column_count++;
	return TRUE;
}

boolean au_table_set_column_with_options(
	struct au_table * table, 
	const char * column_name,
	uint32_t column_id,
	boolean parse_empty_value)
{
	uint32_t i;
	if (table->column_count >= AU_TABLE_MAX_COLUMN_COUNT)
		return FALSE;
	i = table->column_count;
	if (strlcpy(table->column_names[i], column_name, 
			sizeof(table->column_names[i])) >= sizeof(table->column_names[i])) {
		ERROR("Column name length is too long (%s).",
			column_name);
		return FALSE;
	}
	table->column_ids[i] = column_id;
	table->column_parse_empty[i] = parse_empty_value;
	table->column_count++;
	return TRUE;
}

boolean au_table_read_next_line(struct au_table * table)
{
	if (!table->parsed_header) {
		if (!parse_header(table))
			return FALSE;
		table->parsed_header = TRUE;
	}
	table->cursor = table->line_buf;
	table->current_column_index = 0;
	table->end_of_line = FALSE;
	/* Read until there is a non-empty line or 
	 * end of the file is reached. */
	while (TRUE) {
		if (!read_line(table->file, table->line_buf, sizeof(table->line_buf)))
			return FALSE;
		if (table->line_buf[0])
			return TRUE;
	}
	return TRUE;
}

boolean au_table_read_next_column(struct au_table * table)
{
	while (!table->end_of_line) {
		char * s = strchr(table->cursor, '\t');
		uint32_t i;
		if (s) {
			size_t max = MIN(MAX_VALUE_SIZE, 
				(size_t)((uintptr_t)s - (uintptr_t)table->cursor) + 1);
			strlcpy(table->current_value, table->cursor, max);
			table->cursor = s + 1;
		}
		else {
			/* This is either the last column or the end 
			 * of the line. */
			strlcpy(table->current_value, table->cursor, sizeof(table->current_value));
			table->cursor = table->cursor + strlen(table->cursor);
			table->end_of_line = TRUE;
		}
		/* A column can be unmapped or empty, in which case 
		 * it will be skipped. */
		i = table->current_column_index++;
		table->current_setup_index = table->column_map[i];
		if (table->current_setup_index == AU_TABLE_INVALID_COLUMN) {
			table->current_column_id = AU_TABLE_INVALID_COLUMN;
			continue;
		}
		assert(table->current_setup_index < table->column_count);
		table->current_column_id = table->column_ids[table->current_setup_index];
		if (!table->current_value[0] && 
			!table->column_parse_empty[table->current_setup_index]) {
			continue;
		}
		return TRUE;
	}
	return FALSE;
}

uint32_t au_table_get_column(struct au_table * table)
{
	return table->current_column_id;
}

char * au_table_get_value(struct au_table * table)
{
	return table->current_value;
}

int32_t au_table_get_i32(struct au_table * table)
{
	return strtol(table->current_value, NULL, 10);
}
