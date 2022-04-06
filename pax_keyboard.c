/*
	MIT License

	Copyright (c) 2022 Julian Scheffers

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include <pax_keyboard.h>
#include <esp_timer.h>
#include <string.h>
#include <malloc.h>

// Initialise the context with default settings.
void pkb_init(pax_buf_t *buf, pkb_ctx_t *ctx) {
	// Allocate a bufffer.
	char *buffer = malloc(4);
	memset(buffer, 0, 4);
	
	// Some defaults.
	*ctx = (pkb_ctx_t) {
		// Position on screen of the keyboard.
		.x              = 0,
		.y              = 0,
		// Maximum size of the keyboard.
		.width          = buf->width,
		.height         = buf->height,
		
		// Content of the keyboard.
		.content        = buffer,
		// Size in bytes of capacity of the content buffer.
		.content_cap    = 4,
		
		// Starting position of the selection in the text box.
		.selection      = 0,
		// Cursor position of the text box.
		.cursor         = 0,
		
		// Cursor position of the keyboard.
		.key_x          = 3,
		.key_y          = 1,
		// The currently held input.
		.held           = PKB_NO_INPUT,
		// The time that holding the input started.
		.hold_start     = 0,
		// The last time pkb_press was called.
		.last_press     = 0,
		
		// Whether the keyboard is multi-line.
		.multiline      = false,
		// Whether the keyboard is in insert mode.
		.insert         = false,
		// The board that is currently selected.
		.board_sel      = PKB_LOWERCASE,
		
		// The font to use for the keyboard.
		.kb_font        = PAX_FONT_DEFAULT,
		// The font size to use for the keyboard.
		.kb_font_size   = 27,
		// The font to use for the text.
		.text_font      = PAX_FONT_DEFAULT,
		// The font size to use for the text.
		.text_font_size = 18,
		// The text color to use.
		.text_col       = 0xffffffff,
		// The text color to use when a character is being held down.
		.sel_text_col   = 0xff000000,
		// The selection color to use.
		.sel_col        = 0xff007fff,
		// The background color to use.
		.bg_col         = 0xff000000,
		
		// Whether something has changed since last draw.
		.dirty          = true,
		// Whether the text has changed since last draw.
		.text_dirty     = true,
		// Whether the keyboard has changed since last draw.
		.kb_dirty       = true,
		// Whether just the selected character has changed since last draw.
		.sel_dirty      = true,
		// Previous cursor position of the keyboard.
		// Used for sel_dirty.
		.last_key_x     = 3,
		.last_key_y     = 1,
	};
	
	// TODO: Pick fancier text sizes.
}

// Free any memory associated with the context.
void pkb_destroy(pkb_ctx_t *ctx) {
	free(ctx->content);
	ctx->content = NULL;
	ctx->content_cap = 0;
}

const char *uppercase_board[] = {
	"QWERTYUIOP",
	"ASDFGHJKL",
	" ZXCVBNM ",
	" <     > ",
	"<", ">",
};
const char *lowercase_board[] = {
	"qwertyuiop",
	"asdfghjkl",
	" zxcvbnm ",
	" ,     . ",
	",", ".",
};
const char *number_board[] = {
	"1234567890",
	"@#$_&-+()/",
	" *\"';:!? ",
	" =      \\ ",
	"=", "\\",
};
const char *symbols_board[] = {
	"()[]{}`~",
	"/|\\+-_=",
	" ^%<>'\" ",
	" ,     . ",
	",", ".",
};
const char **boards[] = {
	lowercase_board, uppercase_board,
	number_board,    symbols_board,
};

static void pkb_char(pax_buf_t *buf, pkb_ctx_t *ctx, float x, float y, float dx, char *text, bool selected) {
	pax_vec1_t dims = pax_text_size(ctx->kb_font, ctx->kb_font_size, text);
	
	if (selected && ctx->held == PKB_CHARSELECT) {
		// Infilll!
		pax_draw_rect(buf, ctx->sel_col, x-dx/2, y, dx, ctx->kb_font_size);
		pax_draw_text(buf, ctx->sel_text_col, ctx->kb_font, ctx->kb_font_size, x-dims.x*0.5, y, text);
	} else {
		pax_draw_text(buf, ctx->text_col, ctx->kb_font, ctx->kb_font_size, x-dims.x*0.5, y, text);
		
		// Outline?
		if (selected) {
			pax_push_2d(buf);
			pax_apply_2d(buf, matrix_2d_translate(-dx/2, 0));
			pax_draw_line(buf, ctx->sel_col, x, y, x + dx - 1, y);
			pax_draw_line(buf, ctx->sel_col, x, y+ctx->kb_font_size-1, x + dx - 1, y+ctx->kb_font_size-1);
			pax_draw_line(buf, ctx->sel_col, x + dx - 1, y, x + dx - 1, y+ctx->kb_font_size-1);
			pax_draw_line(buf, ctx->sel_col, x, y, x, y+ctx->kb_font_size-1);
			pax_pop_2d(buf);
		}
	}
}

static void pkb_row(pax_buf_t *buf, pkb_ctx_t *ctx, char *row, int selected, float dx, float y) {
	// Calculate some stuff.
	size_t len    = strlen(row);
	int    x      = (buf->width - len * dx + dx) / 2;
	char   tmp[2] = {0,0};
	
	// Show all of them.
	for (int i = 0; i < len; i++) {
		// Draw a KEY.
		*tmp = row[i];
		pkb_char(buf, ctx, x, y, dx, tmp, selected == i);
		x += dx;
	}
}

static void pkb_row_key(pax_buf_t *buf, pkb_ctx_t *ctx, char *row, bool selected, float dx, float y, int keyno) {
	// Calculate some stuff.
	size_t len    = strlen(row);
	int    x      = (buf->width - len * dx + dx) / 2 + dx * keyno;
	char   tmp[2] = {0,0};
	
	// Show one of them.
	*tmp = row[keyno];
	pax_draw_rect(buf, ctx->bg_col, x-dx/2, y, dx, ctx->kb_font_size);
	pkb_char(buf, ctx, x, y, dx, tmp, selected);
}

// Draw just the text part.
static void pkb_render_keyb(pax_buf_t *buf, pkb_ctx_t *ctx, bool do_bg) {
	// Draw background.
	if (do_bg) {
		pax_draw_rect(buf, ctx->bg_col, 0, ctx->height-ctx->kb_font_size*4, ctx->width, ctx->kb_font_size*4);
	}
	
	// Select the board to display.
	char **board = boards[ctx->board_sel];
	float  dx    = ctx->width / 10;
	float  y     = ctx->y + ctx->height - ctx->kb_font_size * 4;
	
	// Draw the first three rows.
	for (int i = 0; i < 3; i ++) {
		int sel = -1;
		if (i == ctx->key_y) {
			sel = ctx->key_x;
		}
		pkb_row(buf, ctx, board[i], sel, dx, y);
		y += ctx->kb_font_size;
	}
	
	// Spacebar row time.
	bool space_sel = ctx->key_y == 3 && ctx->key_x > 1 && ctx->key_x < 7;
	float x = (buf->width - 6 * dx) / 2;
	
	// Left char.
	pkb_char(buf, ctx, x, y, dx, board[4], ctx->key_y == 3 && ctx->key_x == 1);
	x += 1.0 * dx;
	
	// SPACE.
	if (space_sel && ctx->held == PKB_CHARSELECT) {
		pax_draw_rect(buf, ctx->sel_col, x-dx/2, y, dx*5, ctx->kb_font_size);
		pax_draw_rect(buf, ctx->sel_text_col, x, y + ctx->kb_font_size/3, dx*4, ctx->kb_font_size/3);
	} else {
		pax_draw_rect(buf, ctx->text_col, x, y + ctx->kb_font_size/3, dx*4, ctx->kb_font_size/3);
		if (space_sel) {
			// Outline rect?
			pax_push_2d(buf);
			pax_apply_2d(buf, matrix_2d_translate(-dx/2, 0));
			pax_draw_line(buf, ctx->sel_col, x, y, x + dx*5 - 1, y);
			pax_draw_line(buf, ctx->sel_col, x, y+ctx->kb_font_size-1, x + dx*5 - 1, y+ctx->kb_font_size-1);
			pax_draw_line(buf, ctx->sel_col, x + dx*5 - 1, y, x + dx*5 - 1, y+ctx->kb_font_size-1);
			pax_draw_line(buf, ctx->sel_col, x, y, x, y+ctx->kb_font_size-1);
			pax_pop_2d(buf);
		}
	}
	
	// Right char.
	x += 5.0 * dx;
	pkb_char(buf, ctx, x, y, dx, board[5], ctx->key_y == 3 && ctx->key_x == 7);
	
}

// Draw just the board part.
static void pkb_render_text(pax_buf_t *buf, pkb_ctx_t *ctx, bool do_bg) {
	// Draw background.
	if (do_bg) {
		pax_draw_rect(buf, ctx->bg_col, 0, 0, ctx->width, ctx->height - ctx->kb_font_size*4);
	}
	if (ctx->key_y == -1) {
		// Outline us.
		pax_outline_rect(buf, ctx->sel_col, 0, 0, ctx->width, ctx->height - ctx->kb_font_size*4);
	}
	
	// Some setup.
	float x = 2;
	float y = 2;
	char tmp[2] = {0, 0};
	
	// Draw everything.
	for (int i = 0; i < strlen(ctx->content); i++) {
		if (ctx->cursor == i) {
			// The cursor in between the input.
			pax_draw_line(buf, ctx->sel_col, x, y, x, y + ctx->text_font_size - 1);
		}
		
		// The character of the input.
		tmp[0] = ctx->content[i];
		pax_vec1_t dims = pax_text_size(ctx->text_font, ctx->text_font_size, tmp);
		
		if (x + dims.x > ctx->width - 2) {
			// Word wrap.
			x  = 2;
			y += ctx->text_font_size;
		}
		pax_draw_text(buf, ctx->text_col, ctx->text_font, ctx->text_font_size, x, y, tmp);
		x += dims.x;
	}
	if (ctx->cursor == strlen(ctx->content)) {
		// The cursor after the input.
		pax_draw_line(buf, ctx->sel_col, x, y, x, y + ctx->text_font_size - 1);
	}
}

// Draw one specific key.
static void pkb_render_key(pax_buf_t *buf, pkb_ctx_t *ctx, int key_x, int key_y) {
	if (key_y == -1) {
		pkb_render_text(buf, ctx, true);
		return;
	}
	
	// Select the board to display.
	char **board = boards[ctx->board_sel];
	float  dx    = ctx->width / 10;
	float  y     = ctx->y + ctx->height - ctx->kb_font_size * 4;
	
	if (key_y < 3) {
		// Draw one of the first three rows.
		y += ctx->kb_font_size * key_y;
		pkb_row_key(buf, ctx, board[key_y], key_y == ctx->key_y && key_x == ctx->key_x, dx, y, key_x);
		y += ctx->kb_font_size;
	} else {
		// Spacebar row time.
		y += ctx->kb_font_size * 3;
		bool space_sel = ctx->key_y == 3 && ctx->key_x > 1 && ctx->key_x < 7;
		float x = (buf->width - 6 * dx) / 2;
		
		// Left char.
		if (key_x == 1) {
			pax_draw_rect(buf, ctx->bg_col, x-dx/2, y, dx, ctx->kb_font_size);
			pkb_char(buf, ctx, x, y, dx, board[4], ctx->key_y == 3 && ctx->key_x == 1);
		}
		x += 1.0 * dx;
		
		// SPACE.
		if (space_sel && ctx->held == PKB_CHARSELECT) {
			pax_draw_rect(buf, ctx->sel_col, x-dx/2, y, dx*5, ctx->kb_font_size);
			pax_draw_rect(buf, ctx->sel_text_col, x, y + ctx->kb_font_size/3, dx*4, ctx->kb_font_size/3);
		} else {
			pax_draw_rect(buf, ctx->bg_col, x-dx/2, y, dx*5, ctx->kb_font_size);
			pax_draw_rect(buf, ctx->text_col, x, y + ctx->kb_font_size/3, dx*4, ctx->kb_font_size/3);
			if (space_sel) {
				// Outline rect?
				pax_push_2d(buf);
				pax_apply_2d(buf, matrix_2d_translate(-dx/2, 0));
				pax_draw_line(buf, ctx->sel_col, x, y, x + dx*5 - 1, y);
				pax_draw_line(buf, ctx->sel_col, x, y+ctx->kb_font_size-1, x + dx*5 - 1, y+ctx->kb_font_size-1);
				pax_draw_line(buf, ctx->sel_col, x + dx*5 - 1, y, x + dx*5 - 1, y+ctx->kb_font_size-1);
				pax_draw_line(buf, ctx->sel_col, x, y, x, y+ctx->kb_font_size-1);
				pax_pop_2d(buf);
			}
		}
		
		// Right char.
		x += 5.0 * dx;
		if (key_x == 7) {
			pax_draw_rect(buf, ctx->bg_col, x-dx/2, y, dx, ctx->kb_font_size);
			pkb_char(buf, ctx, x, y, dx, board[5], ctx->key_y == 3 && ctx->key_x == 7);
		}
	}
}

// Redraw the complete on-screen keyboard.
void pkb_render(pax_buf_t *buf, pkb_ctx_t *ctx) {
	pax_background(buf, ctx->bg_col);
	
	// Draw the board.
	pkb_render_keyb(buf, ctx, false);
	// Time to draw some text.
	pkb_render_text(buf, ctx, false);
	
	// Mark as not dirty.
	ctx->dirty      = false;
	ctx->kb_dirty   = false;
	ctx->sel_dirty  = false;
	ctx->text_dirty = false;
	ctx->last_key_x = ctx->key_x;
	ctx->last_key_y = ctx->key_y;
}

// Redraw only the changed parts of the on-screen keyboard.
void pkb_redraw(pax_buf_t *buf, pkb_ctx_t *ctx) {
	if (ctx->text_dirty) {
		pkb_render_text(buf, ctx, true);
	}
	if (ctx->kb_dirty) {
		pkb_render_keyb(buf, ctx, true);
	} else if (ctx->sel_dirty) {
		pkb_render_key(buf, ctx, ctx->last_key_x, ctx->last_key_y);
		pkb_render_key(buf, ctx, ctx->key_x,      ctx->key_y);
	}
	
	// Mark as not dirty.
	ctx->dirty      = false;
	ctx->kb_dirty   = false;
	ctx->sel_dirty  = false;
	ctx->text_dirty = false;
	ctx->last_key_x = ctx->key_x;
	ctx->last_key_y = ctx->key_y;
}

// The loop that allows input repeating.
void pkb_loop(pkb_ctx_t *ctx) {
	int64_t now = esp_timer_get_time();
	if (!ctx->held) return;
	bool is_dir = (ctx->held >= PKB_UP) && (ctx->held <= PKB_RIGHT);
	
	if ((ctx->hold_start + 1000000 < now) || (is_dir && ctx->hold_start + 250000 < now)) {
		// 8 repeats per second.
		if (ctx->last_press + 125000 < now) {
			pkb_press(ctx, ctx->held);
		}
	}
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
	ctx->text_dirty = true;
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
	ctx->text_dirty = true;
}

// A pressing of the input.
void pkb_press(pkb_ctx_t *ctx, pkb_input_t input) {
	char **board = boards[ctx->board_sel];
	ctx->last_press = esp_timer_get_time();
	switch (input) {
			// Cursor movements.
			size_t rowlen;
		case PKB_UP:
			ctx->key_y --;
			if (ctx->key_y < -1) ctx->key_y = 3;
			else if (ctx->key_y != -1) {
				rowlen = strlen(board[ctx->key_y]);
				if (ctx->key_x >= rowlen) ctx->key_x = rowlen - 1;
			}
			ctx->sel_dirty = true;
			break;
			
		case PKB_DOWN:
			ctx->key_y ++;
			ctx->key_y %= 4;
			rowlen = strlen(board[ctx->key_y]);
			if (ctx->key_x >= rowlen) ctx->key_x = rowlen - 1;
			ctx->sel_dirty = true;
			break;
			
		case PKB_LEFT:
			if (ctx->key_y == -1) {
				if (ctx->cursor > 0) ctx->cursor --;
			} else if (ctx->key_y == 3 && ctx->key_x > 1 && ctx->key_x < 7) {
				ctx->key_x = 1;
			} else {
				ctx->key_x --;
				if (ctx->key_x < 0) ctx->key_x = strlen(board[ctx->key_y]) - 1;
			}
			ctx->sel_dirty = true;
			break;
			
		case PKB_RIGHT:
			if (ctx->key_y == -1) {
				if (ctx->cursor < strlen(ctx->content)) ctx->cursor ++;
			} else if (ctx->key_y == 3 && ctx->key_x > 1 && ctx->key_x < 7) {
				ctx->key_x = 7;
			} else {
				ctx->key_x ++;
				ctx->key_x %= strlen(board[ctx->key_y]);
			}
			ctx->sel_dirty = true;
			break;
			
			// Enter a character.
		case PKB_CHARSELECT:
			ctx->sel_dirty |= ctx->held != PKB_CHARSELECT;
			if (ctx->key_y == 3) {
				switch (ctx->key_x) {
					case 0:
						// Something?
						break;
					case 1:
						// Magic.
						pkb_append(ctx, *board[4]);
						break;
					default:
						// Spacebar.
						pkb_append(ctx, ' ');
						break;
					case 7:
						// mAGIC.
						pkb_append(ctx, *board[5]);
						break;
					case 8:
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
			
			// Shift key, the pressening.
		case PKB_SHIFT:
			ctx->board_sel |= 1;
			ctx->kb_dirty = true;
			break;
			
			// Next keyboard.
		case PKB_MODESELECT:
			ctx->board_sel ++;
			ctx->board_sel %= 4;
			rowlen = strlen(board[ctx->key_y]);
			if (ctx->key_x >= rowlen) ctx->key_x = rowlen - 1;
			ctx->kb_dirty = true;
			break;
			
			// Backspace.
		case PKB_DELETE_BEFORE:
			pkb_delete(ctx, true);
			break;
			
			// Delete.
		case PKB_DELETE_AFTER:
			pkb_delete(ctx, false);
			break;
		default:
			break;
	}
	if (input != PKB_SHIFT && input != ctx->held) {
		ctx->held = input;
		ctx->hold_start = esp_timer_get_time();
	}
	ctx->dirty = true;
}

// A relealing of the input.
void pkb_release(pkb_ctx_t *ctx, pkb_input_t input) {
	switch (input) {
			// Shift key, the releasening.
		case PKB_SHIFT:
			ctx->dirty = true;
			ctx->board_sel &= ~1;
			ctx->kb_dirty = true;
			break;
			
			// Unpress them char.
		case PKB_CHARSELECT:
			ctx->sel_dirty = true;
			break;
			
		default:
			break;
	}
	if (ctx->held == input) {
		ctx->held = PKB_NO_INPUT;
		ctx->dirty = true;
	}
}
