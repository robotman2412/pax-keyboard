
#include <pax_keyboard.h>
#include <string.h>
#include <malloc.h>

char *uppercase_board[] = {
	"QWERTYUIOP",
	"ASDFGHJKL",
	" ZXCVBNM ",
	",", "."
};
char *lowercase_board[] = {
	"qwertyuiop",
	"asdfghjkl",
	" zxcvbnm ",
	",", "."
};
char *number_board[] = {
	"1234567890",
	"@#$_&-+()/",
	" *\"';:!? ",
	"<", ">"
};

static void pkb_char(pax_buf_t *buf, pkb_ctx_t *ctx, float x, float y, float dx, char *text, bool selected) {
	pax_col_t col = 0xffff0000;
	
	pax_vec1_t dims = pax_text_size(ctx->font, ctx->font_size, text);
	pax_draw_text(buf, col, ctx->font, ctx->font_size, x-dims.x*0.5, y, text);
	
	// Outline?
	if (selected) {
		pax_apply_2d(buf, matrix_2d_translate(-dx/2, 0));
		pax_draw_line(buf, col, x, y, x + dx, y);
		pax_draw_line(buf, col, x, y+ctx->font_size, x + dx, y+ctx->font_size);
		pax_draw_line(buf, col, x + dx, y, x + dx, y+ctx->font_size);
		pax_draw_line(buf, col, x, y, x, y+ctx->font_size);
		pax_apply_2d(buf, matrix_2d_translate(dx/2, 0));
	}
}

static void pkb_row(pax_buf_t *buf, pkb_ctx_t *ctx, char *row, int selected, float dx, float y) {
	// Calculate some stuff.
	size_t len    = strlen(row);
	int    x      = (buf->width - len * dx + dx) / 2;
	char   tmp[2] = {0,0};
	
	// Show all of them.
	for (int i = 0; i < len; i++) {
		// Draw a TEXT.
		*tmp = row[i];
		pkb_char(buf, ctx, x, y, dx, tmp, selected == i);
		x += dx;
	}
}

// Redraw the complete on-screen keyboard.
void pkb_render(pax_buf_t *buf, pkb_ctx_t *ctx) {
	pax_col_t text_col = 0xffff0000;
	
	// Select the board to display.
	char **board = lowercase_board;
	float  dx    = ctx->width / strlen(*board);
	float  y     = ctx->y + ctx->height - ctx->font_size * 4;
	for (int i = 0; i < 3; i ++) {
		int sel = -1;
		if (i == ctx->key_y) {
			ctx->key_x %= strlen(board[i]);
			sel = ctx->key_x;
		}
		pkb_row(buf, ctx, board[i], sel, dx, y);
		y += ctx->font_size;
	}
	
	// Spacebar row time.
	float x = (buf->width - 6 * dx) / 2;
	pkb_char(buf, ctx, x, y, dx, board[3], false);
	x += 1.0 * dx;
	pax_draw_rect(buf, text_col, x, y + ctx->font_size/3, dx*4, ctx->font_size/3);
	x += 5.0 * dx;
	pkb_char(buf, ctx, x, y, dx, board[4], false);
	
	// Crappy text.
	pax_draw_text(buf, text_col, NULL, 18, 0, 0, ctx->content);
}

static void pkb_delete(pkb_ctx_t *ctx, bool is_backspace) {
	size_t oldlen = strlen(ctx->content);
	if (!is_backspace && ctx->cursor == oldlen) {
		// No forward deleting at the end of the line.
		return;
	} else if (is_backspace && ctx->cursor == 0) {
		// No backward deleting at the start of the line.
		return;
	} else if (!is_backspace) {
		// Advanced backspace.
		ctx->cursor ++;
	}
	
	// Copy back everything including null terminator.
	ctx->cursor --;
	for (int i = ctx->cursor; i < oldlen; i++) {
		ctx->content[i] = ctx->content[i+1];
	}
	
	// DECREMENT length.
	if (oldlen * 2 < ctx->content_cap) {
		// Not decrementing here is important as oldlen excludes the null terminator.
		ctx->content_cap = oldlen;
		ctx->content = realloc(ctx->content, ctx->content_cap);
	}
}

static void pkb_append(pkb_ctx_t *ctx, char value) {
	size_t oldlen = strlen(ctx->content);
	if (oldlen + 2 >= ctx->content_cap) {
		// Expand the buffer just a bit.
		ctx->content_cap *= 2;
		ctx->content = realloc(ctx->content, ctx->content_cap);
	}
	
	// Copy over the remainder of the buffer.
	// If there's no text this still copies the null terminator.
	for (int i = oldlen; i >= ctx->cursor; i --) {
		ctx->content[i + 1] = ctx->content[i];
	}
	
	// And finally insert at the character.
	ctx->content[ctx->cursor] = value;
	ctx->cursor ++;
}

// A pressing of the input.
void pkb_press(pkb_ctx_t *ctx, pkb_input_t input) {
	char **board = lowercase_board;
	ctx->held = input;
	switch (input) {
			// Cursor movements.
			size_t rowlen;
		case PKB_UP:
			ctx->key_y --;
			if (ctx->key_y < 0) ctx->key_y = 3;
			rowlen = strlen(board[ctx->key_y]);
			if (ctx->key_x >= rowlen) ctx->key_x = rowlen - 1;
			break;
		case PKB_DOWN:
			ctx->key_y ++;
			ctx->key_y %= 4;
			rowlen = strlen(board[ctx->key_y]);
			if (ctx->key_x >= rowlen) ctx->key_x = rowlen - 1;
			break;
		case PKB_LEFT:
			ctx->key_x --;
			if (ctx->key_x < 0) ctx->key_x = strlen(board[ctx->key_y]) - 1;
			break;
		case PKB_RIGHT:
			ctx->key_x ++;
			ctx->key_x %= strlen(board[ctx->key_y]);
			break;
			// Enter a character.
		case PKB_CHARSELECT:
			if (ctx->key_y == 3) {
				switch (ctx->key_x) {
					default:
					case 0:
						// Something?
						break;
					case 1:
						// Magic.
						pkb_append(ctx, *board[3]);
						break;
					case 2:
						// Spacebar.
						pkb_append(ctx, ' ');
						break;
					case 3:
						// mAGIC.
						pkb_append(ctx, *board[4]);
						break;
					case 4:
						// Enter idk.
						break;
				}
			} else if (ctx->key_y == 2) {
				if (ctx->key_x == 0) {
					// Something?
				} else if (ctx->key_x == strlen(board[2])-1) {
					// Backspace.
					pkb_delete(ctx, true);
				} else {
					// nORMAL CHAR.
					pkb_append(ctx, board[2][ctx->key_x]);
				}
			} else {
				// Normal char.
				pkb_append(ctx, board[ctx->key_y][ctx->key_x]);
			}
			break;
		default:
			break;
	}
}

// A relealing of the input.
void pkb_release(pkb_ctx_t *ctx, pkb_input_t input) {
	if (ctx->held == input) {
		ctx->held = PKB_NO_INPUT;
	}
}
