/* lib.c - Some basic library functions (printf, strlen, etc.)
 * vim:ts=4 noexpandtab
 */

#include "lib.h"
#include "keybrd.h"
#include "interrupt_handler.h"
#include "sche.h"
#include "syscall.h"
 
#define VIDEO 0xB8000
#define NUM_COLS 80
#define NUM_ROWS 25
#define ATTRIB 0x7
#define VGA_BASE1 0x3D4			// addr of VGA index register
#define VGA_BASE2 0x3D5
#define SCREEN_SIZE 4096

static const uint8_t attr[3] = {0x7, 0x10, 0x20};
static int screen_x[SCREEN_LIMIT]; 
static int screen_y[SCREEN_LIMIT];

//static uint8_t cur_char[3];
static char* video_mem = (char *)VIDEO;

//update the cursor
//static void update_cursor(int row, int col);
// helper function to print backspace
void print_backspace(void);
//let the contents in terminal console scroll
static void handle_scrolling(int type);
//check condition and call handle_scrolling
static void handle_wrap_around(int type);

/*
* void clear(void);
*   Inputs: void
*   Return Value: none
*	Function: Clears video memory
*/
void
clear(void)
{
    int32_t i;
    for(i=0; i<NUM_ROWS*NUM_COLS; i++) {
        *(uint8_t *)(video_mem + 2*SCREEN_SIZE*display_index + (i << 1)) = ' ';
        *(uint8_t *)(video_mem + 2*SCREEN_SIZE*display_index + (i << 1) + 1) = attr[display_index];
    }
    screen_x[display_index] = 0;			//reset position and update the cursor
    screen_y[display_index] = 0;
    update_cursor(0,0);
}

/*
* update_cursor(int row, int col)
*   Inputs: row -- the row number
			col -- the column number 
*   Return Value: void
*	Function: use the input information to update the cursor position
*/
void 
update_cursor(int row, int col)
 {
    unsigned short position=(row*NUM_COLS) + col;

    // cursor LOW port to vga INDEX register
    outb(0x0F, VGA_BASE1);
    outb((unsigned char)(position&0xFF), VGA_BASE2);
    // cursor HIGH port to vga INDEX register
    outb(0x0E, VGA_BASE1);
    outb((unsigned char )((position>>8)&0xFF), VGA_BASE2);
 }

 /*
*  print_backspace(void)
*   Inputs: none
*   Return Value: void
*	Function:  erase the last input and update the cursor to accomplish the backspace function.
*/
void
print_backspace(void)
{
	if (screen_x[display_index] == 0)
	{
		screen_y[display_index]--;
		screen_x[display_index] = NUM_COLS - 1;		//if at the start of a new line, go to the last of the previous line
	}
	else
	screen_x[display_index]--;							//move to the previous location
	
    *(uint8_t *)(video_mem + 2*SCREEN_SIZE*display_index + ((NUM_COLS*screen_y[display_index] + screen_x[display_index]) << 1)) = ' ';
    *(uint8_t *)(video_mem + 2*SCREEN_SIZE*display_index + ((NUM_COLS*screen_y[display_index] + screen_x[display_index]) << 1) + 1) = attr[display_index];	/*change the place in video memory to ' '*/
	update_cursor(screen_y[display_index],screen_x[display_index]);												//update the cursor to the previous position
}

/*
* handle_scrolling(int type)
*   Inputs: type for which terminal to display on
*   Return Value: void
*	Function: enable scrolling down on terminal, and when type = 1 display to the running terminal, whereas 0 print to display terminal
*/
static void 
handle_scrolling(int type)
{
	if(type == 0){
		int32_t i;
		for(i = 0; i < (NUM_ROWS - 1) * NUM_COLS; i++) {								//shift the whole video memory by one row up, except the last row
			*(uint8_t *)(video_mem + 2*SCREEN_SIZE*cur_index + (i << 1)) = *(uint8_t *)(video_mem + 2*SCREEN_SIZE*cur_index + ((i+NUM_COLS) << 1));
			*(uint8_t *)(video_mem + 2*SCREEN_SIZE*cur_index + (i << 1) + 1) = attr[cur_index];
			}
		for(i = (NUM_ROWS - 1) * NUM_COLS; i < NUM_ROWS * NUM_COLS; i++)				//for the last row, save ' ' to all of it
			{
			*(uint8_t *)(video_mem + 2*SCREEN_SIZE*cur_index + (i << 1)) = ' ';
			*(uint8_t *)(video_mem + 2*SCREEN_SIZE*cur_index + (i << 1) + 1) = attr[cur_index];
			}
		screen_y[cur_index]--;															//decrement the row count
		return;
	}
	else{
		int32_t i;
		for(i = 0; i < (NUM_ROWS - 1) * NUM_COLS; i++) {								//shift the whole video memory by one row up, except the last row
			*(uint8_t *)(video_mem + 2*SCREEN_SIZE*display_index + (i << 1)) = *(uint8_t *)(video_mem + 2*SCREEN_SIZE*display_index + ((i+NUM_COLS) << 1));
			*(uint8_t *)(video_mem + 2*SCREEN_SIZE*display_index + (i << 1) + 1) = attr[display_index];
			}
		for(i = (NUM_ROWS - 1) * NUM_COLS; i < NUM_ROWS * NUM_COLS; i++)				//for the last row, save ' ' to all of it
			{
			*(uint8_t *)(video_mem + 2*SCREEN_SIZE*display_index + (i << 1)) = ' ';
			*(uint8_t *)(video_mem + 2*SCREEN_SIZE*display_index + (i << 1) + 1) = attr[display_index];
			}
		screen_y[display_index]--;														//decrement the row count
		return;
	}
}


/*
*   void handle_wrap_around(int type)
*   Inputs: type for which terminal to display on
*   Return Value: void
*	Function: check the condition and call handle_scrolling function and when type = 1 display to the running terminal, whereas 0 print to display terminal
*/
static void 
handle_wrap_around(int type)
{
	if(type == 0){
		screen_y[cur_index]++;																//increment y
		if (screen_y[cur_index] == NUM_ROWS)												//handle scrolling if last line is reached
		handle_scrolling(type);
		return;
	}
	else{
		screen_y[display_index]++;															//increment y
		if (screen_y[display_index] == NUM_ROWS)											//handle scrolling if last line is reached
		handle_scrolling(type);
		return;
	}
}

/* Standard printf().
 * Only supports the following format strings:
 * %%  - print a literal '%' character
 * %x  - print a number in hexadecimal
 * %u  - print a number as an unsigned integer
 * %d  - print a number as a signed integer
 * %c  - print a character
 * %s  - print a string
 * %#x - print a number in 32-bit aligned hexadecimal, i.e.
 *       print 8 hexadecimal digits, zero-padded on the left.
 *       For example, the hex number "E" would be printed as
 *       "0000000E".
 *       Note: This is slightly different than the libc specification
 *       for the "#" modifier (this implementation doesn't add a "0x" at
 *       the beginning), but I think it's more flexible this way.
 *       Also note: %x is the only conversion specifier that can use
 *       the "#" modifier to alter output.
 * */
int32_t
printf(int8_t *format, ...)
{

	/* Pointer to the format string */
	int8_t* buf = format;

	/* Stack pointer for the other parameters */
	int32_t* esp = (void *)&format;
	esp++;

	while(*buf != '\0') {
		switch(*buf) {
			case '%':
				{
					int32_t alternate = 0;
					buf++;

format_char_switch:
					/* Conversion specifiers */
					switch(*buf) {
						/* Print a literal '%' character */
						case '%':
							putc('%', 0);
							break;

						/* Use alternate formatting */
						case '#':
							alternate = 1;
							buf++;
							/* Yes, I know gotos are bad.  This is the
							 * most elegant and general way to do this,
							 * IMHO. */
							goto format_char_switch;

						/* Print a number in hexadecimal form */
						case 'x':
							{
								int8_t conv_buf[64];
								if(alternate == 0) {
									itoa(*((uint32_t *)esp), conv_buf, 16);
									puts(conv_buf, 0);
								} else {
									int32_t starting_index;
									int32_t i;
									itoa(*((uint32_t *)esp), &conv_buf[8], 16);
									i = starting_index = strlen(&conv_buf[8]);
									while(i < 8) {
										conv_buf[i] = '0';
										i++;
									}
									puts(&conv_buf[starting_index], 0);
								}
								esp++;
							}
							break;

						/* Print a number in unsigned int form */
						case 'u':
							{
								int8_t conv_buf[36];
								itoa(*((uint32_t *)esp), conv_buf, 10);
								puts(conv_buf, 0);
								esp++;
							}
							break;

						/* Print a number in signed int form */
						case 'd':
							{
								int8_t conv_buf[36];
								int32_t value = *((int32_t *)esp);
								if(value < 0) {
									conv_buf[0] = '-';
									itoa(-value, &conv_buf[1], 10);
								} else {
									itoa(value, conv_buf, 10);
								}
								puts(conv_buf, 0);
								esp++;
							}
							break;

						/* Print a single character */
						case 'c':
							putc( (uint8_t) *((int32_t *)esp) , 0);
							esp++;
							break;

						/* Print a NULL-terminated string */
						case 's':
							puts( *((int8_t **)esp), 0);
							esp++;
							break;

						default:
							break;
					}

				}
				break;

			default:
				putc(*buf, 0);
				break;
		}
		buf++;
	}

	return (buf - format);
}

/*
* int32_t puts(int8_t* s, type);
*   Inputs: int_8* s = pointer to a string of characters, and type for which terminal to print on
*   Return Value: Number of bytes written
*	Function: Output a string to the console, 1 for running and 0 for displaying 
*/

int32_t
puts(int8_t* s,int type)
{
	register int32_t index = 0;
	while(s[index] != '\0') {
		if (type == 0){
			putc(s[index], 0);
			index++;
		}
		else{
			putc(s[index], 1);
			index++;
		}
	}

	return index;
}

/*
*   void putc(uint8_t c, int type)
*   Inputs: uint_8* c = character to print and type for which terminal to display on
*   Return Value: void
*	Function: Output a character to the console, and 1 for running, 0 for displaying
*/
void
putc(uint8_t c, int type)
{
	if(type == 0){
		if(c == '\n' || c == '\r') {
			screen_y[cur_index]++;
			if (screen_y[cur_index] == NUM_ROWS)
			handle_scrolling(0);																//if y reaches the limit, handle scrolling
			screen_x[cur_index] = 0;
			update_cursor (screen_y[cur_index], screen_x[cur_index]);
		} else {
			*(uint8_t *)(video_mem + 2*SCREEN_SIZE*cur_index + ((NUM_COLS*screen_y[cur_index] + screen_x[cur_index]) << 1)) = c;
			*(uint8_t *)(video_mem + 2*SCREEN_SIZE*cur_index + ((NUM_COLS*screen_y[cur_index] + screen_x[cur_index]) << 1) + 1) = attr[cur_index];
			screen_x[cur_index]++;
			if (screen_x[cur_index] == NUM_COLS)												//if x reaches the limit, handle wrap around
			handle_wrap_around(0);
			screen_x[cur_index] %= NUM_COLS;
			screen_y[cur_index] = (screen_y[cur_index] + (screen_x[cur_index] / NUM_COLS)) % NUM_ROWS;
			update_cursor (screen_y[cur_index], screen_x[cur_index]);
		}
	}
	else{
		if(c == '\n' || c == '\r') {
			screen_y[display_index]++;
			if (screen_y[display_index] == NUM_ROWS)
			handle_scrolling(1);																//if y reaches the limit, handle scrolling
			screen_x[display_index] = 0;
			update_cursor (screen_y[display_index], screen_x[display_index]);
		} else {
			*(uint8_t *)(video_mem + 2*SCREEN_SIZE*display_index + ((NUM_COLS*screen_y[display_index] + screen_x[display_index]) << 1)) = c;
			*(uint8_t *)(video_mem + 2*SCREEN_SIZE*display_index + ((NUM_COLS*screen_y[display_index] + screen_x[display_index]) << 1) + 1) = attr[display_index];
			screen_x[display_index]++;
			if (screen_x[display_index] == NUM_COLS)											//if x reaches the limit, handle wrap around
			handle_wrap_around(1);
			screen_x[display_index] %= NUM_COLS;
			screen_y[display_index] = (screen_y[display_index] + (screen_x[display_index] / NUM_COLS)) % NUM_ROWS;
			update_cursor (screen_y[display_index], screen_x[display_index]);
		}
	}
}

/*
* int8_t* itoa(uint32_t value, int8_t* buf, int32_t radix);
*   Inputs: uint32_t value = number to convert
*			int8_t* buf = allocated buffer to place string in
*			int32_t radix = base system. hex, oct, dec, etc.
*   Return Value: number of bytes written
*	Function: Convert a number to its ASCII representation, with base "radix"
*/

int8_t*
itoa(uint32_t value, int8_t* buf, int32_t radix)
{
	static int8_t lookup[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	int8_t *newbuf = buf;
	int32_t i;
	uint32_t newval = value;

	/* Special case for zero */
	if(value == 0) {
		buf[0]='0';
		buf[1]='\0';
		return buf;
	}

	/* Go through the number one place value at a time, and add the
	 * correct digit to "newbuf".  We actually add characters to the
	 * ASCII string from lowest place value to highest, which is the
	 * opposite of how the number should be printed.  We'll reverse the
	 * characters later. */
	while(newval > 0) {
		i = newval % radix;
		*newbuf = lookup[i];
		newbuf++;
		newval /= radix;
	}

	/* Add a terminating NULL */
	*newbuf = '\0';

	/* Reverse the string and return */
	return strrev(buf);
}

/*
* int8_t* strrev(int8_t* s);
*   Inputs: int8_t* s = string to reverse
*   Return Value: reversed string
*	Function: reverses a string s
*/

int8_t*
strrev(int8_t* s)
{
	register int8_t tmp;
	register int32_t beg=0;
	register int32_t end=strlen(s) - 1;

	while(beg < end) {
		tmp = s[end];
		s[end] = s[beg];
		s[beg] = tmp;
		beg++;
		end--;
	}

	return s;
}

/*
* uint32_t strlen(const int8_t* s);
*   Inputs: const int8_t* s = string to take length of
*   Return Value: length of string s
*	Function: return length of string s
*/

uint32_t
strlen(const int8_t* s)
{
	register uint32_t len = 0;
	while(s[len] != '\0')
		len++;

	return len;
}

/*
* void* memset(void* s, int32_t c, uint32_t n);
*   Inputs: void* s = pointer to memory
*			int32_t c = value to set memory to
*			uint32_t n = number of bytes to set
*   Return Value: new string
*	Function: set n consecutive bytes of pointer s to value c
*/

void*
memset(void* s, int32_t c, uint32_t n)
{
	c &= 0xFF;
	asm volatile("                  \n\
			.memset_top:            \n\
			testl   %%ecx, %%ecx    \n\
			jz      .memset_done    \n\
			testl   $0x3, %%edi     \n\
			jz      .memset_aligned \n\
			movb    %%al, (%%edi)   \n\
			addl    $1, %%edi       \n\
			subl    $1, %%ecx       \n\
			jmp     .memset_top     \n\
			.memset_aligned:        \n\
			movw    %%ds, %%dx      \n\
			movw    %%dx, %%es      \n\
			movl    %%ecx, %%edx    \n\
			shrl    $2, %%ecx       \n\
			andl    $0x3, %%edx     \n\
			cld                     \n\
			rep     stosl           \n\
			.memset_bottom:         \n\
			testl   %%edx, %%edx    \n\
			jz      .memset_done    \n\
			movb    %%al, (%%edi)   \n\
			addl    $1, %%edi       \n\
			subl    $1, %%edx       \n\
			jmp     .memset_bottom  \n\
			.memset_done:           \n\
			"
			:
			: "a"(c << 24 | c << 16 | c << 8 | c), "D"(s), "c"(n)
			: "edx", "memory", "cc"
			);

	return s;
}

/*
* void* memset_word(void* s, int32_t c, uint32_t n);
*   Inputs: void* s = pointer to memory
*			int32_t c = value to set memory to
*			uint32_t n = number of bytes to set
*   Return Value: new string
*	Function: set lower 16 bits of n consecutive memory locations of pointer s to value c
*/

/* Optimized memset_word */
void*
memset_word(void* s, int32_t c, uint32_t n)
{
	asm volatile("                  \n\
			movw    %%ds, %%dx      \n\
			movw    %%dx, %%es      \n\
			cld                     \n\
			rep     stosw           \n\
			"
			:
			: "a"(c), "D"(s), "c"(n)
			: "edx", "memory", "cc"
			);

	return s;
}

/*
* void* memset_dword(void* s, int32_t c, uint32_t n);
*   Inputs: void* s = pointer to memory
*			int32_t c = value to set memory to
*			uint32_t n = number of bytes to set
*   Return Value: new string
*	Function: set n consecutive memory locations of pointer s to value c
*/

void*
memset_dword(void* s, int32_t c, uint32_t n)
{
	asm volatile("                  \n\
			movw    %%ds, %%dx      \n\
			movw    %%dx, %%es      \n\
			cld                     \n\
			rep     stosl           \n\
			"
			:
			: "a"(c), "D"(s), "c"(n)
			: "edx", "memory", "cc"
			);

	return s;
}

/*
* void* memcpy(void* dest, const void* src, uint32_t n);
*   Inputs: void* dest = destination of copy
*			const void* src = source of copy
*			uint32_t n = number of byets to copy
*   Return Value: pointer to dest
*	Function: copy n bytes of src to dest
*/

void*
memcpy(void* dest, const void* src, uint32_t n)
{
	asm volatile("                  \n\
			.memcpy_top:            \n\
			testl   %%ecx, %%ecx    \n\
			jz      .memcpy_done    \n\
			testl   $0x3, %%edi     \n\
			jz      .memcpy_aligned \n\
			movb    (%%esi), %%al   \n\
			movb    %%al, (%%edi)   \n\
			addl    $1, %%edi       \n\
			addl    $1, %%esi       \n\
			subl    $1, %%ecx       \n\
			jmp     .memcpy_top     \n\
			.memcpy_aligned:        \n\
			movw    %%ds, %%dx      \n\
			movw    %%dx, %%es      \n\
			movl    %%ecx, %%edx    \n\
			shrl    $2, %%ecx       \n\
			andl    $0x3, %%edx     \n\
			cld                     \n\
			rep     movsl           \n\
			.memcpy_bottom:         \n\
			testl   %%edx, %%edx    \n\
			jz      .memcpy_done    \n\
			movb    (%%esi), %%al   \n\
			movb    %%al, (%%edi)   \n\
			addl    $1, %%edi       \n\
			addl    $1, %%esi       \n\
			subl    $1, %%edx       \n\
			jmp     .memcpy_bottom  \n\
			.memcpy_done:           \n\
			"
			:
			: "S"(src), "D"(dest), "c"(n)
			: "eax", "edx", "memory", "cc"
			);

	return dest;
}

/*
* void* memmove(void* dest, const void* src, uint32_t n);
*   Inputs: void* dest = destination of move
*			const void* src = source of move
*			uint32_t n = number of byets to move
*   Return Value: pointer to dest
*	Function: move n bytes of src to dest
*/

/* Optimized memmove (used for overlapping memory areas) */
void*
memmove(void* dest, const void* src, uint32_t n)
{
	asm volatile("                  \n\
			movw    %%ds, %%dx      \n\
			movw    %%dx, %%es      \n\
			cld                     \n\
			cmp     %%edi, %%esi    \n\
			jae     .memmove_go     \n\
			leal    -1(%%esi, %%ecx), %%esi    \n\
			leal    -1(%%edi, %%ecx), %%edi    \n\
			std                     \n\
			.memmove_go:            \n\
			rep     movsb           \n\
			"
			:
			: "D"(dest), "S"(src), "c"(n)
			: "edx", "memory", "cc"
			);

	return dest;
}

/*
* int32_t strncmp(const int8_t* s1, const int8_t* s2, uint32_t n)
*   Inputs: const int8_t* s1 = first string to compare
*			const int8_t* s2 = second string to compare
*			uint32_t n = number of bytes to compare
*	Return Value: A zero value indicates that the characters compared 
*					in both strings form the same string.
*				A value greater than zero indicates that the first 
*					character that does not match has a greater value 
*					in str1 than in str2; And a value less than zero 
*					indicates the opposite.
*	Function: compares string 1 and string 2 for equality
*/

int32_t
strncmp(const int8_t* s1, const int8_t* s2, uint32_t n)
{
	int32_t i;
	for(i=0; i<n; i++) {
		if( (s1[i] != s2[i]) ||
				(s1[i] == '\0') /* || s2[i] == '\0' */ ) {

			/* The s2[i] == '\0' is unnecessary because of the short-circuit
			 * semantics of 'if' expressions in C.  If the first expression
			 * (s1[i] != s2[i]) evaluates to false, that is, if s1[i] ==
			 * s2[i], then we only need to test either s1[i] or s2[i] for
			 * '\0', since we know they are equal. */

			return s1[i] - s2[i];
		}
	}
	return 0;
}

/*
* int8_t* strcpy(int8_t* dest, const int8_t* src)
*   Inputs: int8_t* dest = destination string of copy
*			const int8_t* src = source string of copy
*   Return Value: pointer to dest
*	Function: copy the source string into the destination string
*/

int8_t*
strcpy(int8_t* dest, const int8_t* src)
{
	int32_t i=0;
	while(src[i] != '\0') {
		dest[i] = src[i];
		i++;
	}

	dest[i] = '\0';
	return dest;
}

/*
* int8_t* strcpy(int8_t* dest, const int8_t* src, uint32_t n)
*   Inputs: int8_t* dest = destination string of copy
*			const int8_t* src = source string of copy
*			uint32_t n = number of bytes to copy
*   Return Value: pointer to dest
*	Function: copy n bytes of the source string into the destination string
*/

int8_t*
strncpy(int8_t* dest, const int8_t* src, uint32_t n)
{
	int32_t i=0;
	while(src[i] != '\0' && i < n) {
		dest[i] = src[i];
		i++;
	}

	while(i < n) {
		dest[i] = '\0';
		i++;
	}

	return dest;
}

/*
* void test_interrupts(void)
*   Inputs: void
*   Return Value: void
*	Function: increments video memory. To be used to test rtc
*/

void
test_interrupts(void)
{
	int32_t i;
	for (i=0; i < NUM_ROWS*NUM_COLS; i++) {
		video_mem[i<<1]++;
	}
}
