#ifndef _AU_TABLE_H_
#define _AU_TABLE_H_

#include "core/macros.h"
#include "core/types.h"

#define AU_TABLE_INVALID_COLUMN UINT32_MAX
#define AU_TABLE_MAX_COLUMN_SIZE 64
#define AU_TABLE_MAX_COLUMN_COUNT 512

BEGIN_DECLS

struct au_table;

struct au_table * au_table_open(const char * file_path, boolean decrypt);

struct au_table * au_table_open_fmt(const char * format, boolean decrypt, ...);

void au_table_destroy(struct au_table * table);

boolean au_table_set_column(
	struct au_table * table, 
	const char * column_name,
	uint32_t column_id);

boolean au_table_set_column_with_options(
	struct au_table * table, 
	const char * column_name,
	uint32_t column_id,
	boolean parse_empty_value);

boolean au_table_read_next_line(struct au_table * table);

boolean au_table_read_next_column(struct au_table * table);

uint32_t au_table_get_column(struct au_table * table);

char * au_table_get_value(struct au_table * table);

int32_t au_table_get_i32(struct au_table * table);

END_DECLS

#endif /* _AU_TABLE_H_ */
