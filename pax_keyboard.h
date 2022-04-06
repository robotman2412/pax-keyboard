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

#ifndef PAX_KEYBOARD_H
#define PAX_KEYBOARD_H

#include <pax_gfx.h>

// A number of inputs supported by the PAX keyboard.
typedef enum {
	// Represents no input being pressed.
	PKB_NO_INPUT,
	// Movement of the cursor.
	PKB_UP, PKB_DOWN, PKB_LEFT, PKB_RIGHT,
	// Delete to the left or the selection. Backspace key.
	PKB_DELETE_BEFORE,
	// Delete to the right or the selection. Delete key.
	PKB_DELETE_AFTER,
	// Switch between lower case, upper case and symbols.
	PKB_MODESELECT,
	// Enter a character.
	PKB_CHARSELECT,  
} pkb_input_t;

// The PAX keyboard context used for drawing and alike.
typedef struct {
	// Position on screen of the keyboard.
	int    x, y;
	// Maximum size of the keyboard.
	int    width, height;
	
	// Content of the keyboard.
	char  *content;
	// Size in bytes of capacity of the content buffer.
	size_t content_cap;
	
	// Starting position of the selection in the text box.
	int    selection;
	// Cursor position of the text box.
	int    cursor;
	
	// Cursor position of the keyboard.
	int    key_x, key_y;
	// The currently held input.
	pkb_input_t held;
	
	// Whether the keyboard is multi-line.
	bool   multiline;
	// Whether the keyboard is in insert mode.
	bool   insert;
	
	// The font to use.
	pax_font_t *font;
	// The font size to use.
	float       font_size;
} pkb_ctx_t;

// Redraw the complete on-screen keyboard.
void pkb_render (pax_buf_t *buf, pkb_ctx_t *ctx);
// A pressing of the input.
void pkb_press  (pkb_ctx_t *ctx, pkb_input_t input);
// A relealing of the input.
void pkb_release(pkb_ctx_t *ctx, pkb_input_t input);

#endif //PAX_KEYBOARD_H
