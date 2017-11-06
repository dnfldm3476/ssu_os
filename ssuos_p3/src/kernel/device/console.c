#include <interrupt.h>
#include <device/console.h>
#include <type.h>
#include <device/kbd.h>
#include <device/io.h>
#include <device/pit.h>
#include <stdarg.h>

#define HSCREEN 80
#define VSCREEN 25
#define SIZE_SCREEN HSCREEN * VSCREEN
#define NSCROLL 100
#define SIZE_SCROLL NSCROLL * HSCREEN
#define VIDIO_MEMORY 0xB8000

#define IO_BASE 0x3F8               /* I/O port base address for the first serial port. */
#define FIRST_SPORT (IO_BASE)
#define LINE_STATUS (IO_BASE + 5)   /* Line Status Register(read-only). */
#define THR_EMPTY 0x20              /* Transmitter Holding Reg Empty. */

char next_line[2]; //"\r\n";

#ifdef SCREEN_SCROLL

//#define buf_e (buf_w + SIZE_NSCROLL)
#define buf_e (buf_w + SIZE_SCROLL)
#define SCROLL_END buf_s + SIZE_SCROLL

char buf_s[SIZE_SCROLL];
char *buf_w;	// = buf_s;
char *buf_p;	// = buf_s;

char* scr_end;// = buf_s + SIZE_SCROLL;

bool a_s = TRUE;
bool first_loop = TRUE; // 화면(buf_w)가 buf_e를 넘어간적이 있는지 확인
#endif

void init_console(void)
{
	Glob_x = 0;
	Glob_y = 2;

	next_line[0] = '\r';
	next_line[1] = '\r';

#ifdef SCREEN_SCROLL
	buf_w = buf_s;
	buf_p = buf_s;
	a_s = TRUE;
	scr_end = buf_s + SIZE_SCROLL;
#endif

}

void PrintCharToScreen(int x, int y, const char *pString) 
{
	Glob_x = x;
	Glob_y = y;
	int i = 0;
	while(pString[i] != 0)
	{
		if (pString[i] == '\b') {
			PrintChar(--Glob_x, Glob_y, NULL);
			i++;
			set_cursor(); // cursor 위치 수정
			continue;
		}
		PrintChar(Glob_x++, Glob_y, pString[i++]);
		set_cursor(); // cursor 위치 수정
	}
}

void PrintChar(int x, int y, const char String) 
{
#ifdef SCREEN_SCROLL
	if (String == '\n') {
		if((y+1) > VSCREEN) {
			scroll(); // 개행이면 스크롤 한줄 밑으로
			y--;
		}
		Glob_x = 0;
		Glob_y = y+1;
		return;
	}
	else {
		if ((y >= VSCREEN) && (x >= 0)) {
			scroll();
			x = 0; y--;
		}      	              	

		char* b = &buf_w[y * HSCREEN + x];
		if(b >= SCROLL_END)
			b-= SIZE_SCROLL;
		*b = String;

		if(Glob_x >= HSCREEN) // 열을 넘어가면 줄넘김
		{
			Glob_x = 0;
			Glob_y++;
		}    
	}
#else
	CHAR *pScreen = (CHAR *)VIDIO_MEMORY;

	if (String == '\n') {
		if((y+1) > 24) {
			scroll();
			y--;
		}
		pScreen += ((y+1) * 80);
		Glob_x = 0;
		Glob_y = y+1;
		return;
	}
	else {
		if ((y > 24) && (x >= 0)) {
			scroll();
			x = 0; y--;
		}                       

		pScreen += ( y * 80) + x;
		pScreen[0].bAtt = 0x07;
		pScreen[0].bCh = String;

		if(Glob_x > 79)
		{
			Glob_x = 0;
			Glob_y++;
		}    
	}
#endif
}
void clrScreen(void) 
{
	CHAR *pScreen = (CHAR *) VIDIO_MEMORY;
	int i;

	for (i = 0; i < 80*25; i++) {
		(*pScreen).bAtt = 0x07; // 속성값
		(*pScreen++).bCh = ' '; // 문자값
	}   
	Glob_x = 0;
	Glob_y = 0;
}

void scroll(void)  // 문자 입력시 매번 호출
{
#ifdef SCREEN_SCROLL
	buf_w += HSCREEN;
	while(buf_w  > SCROLL_END) {// 끝에 도달하면 시작위치를 처음으로
		buf_w -= SIZE_SCROLL;
		first_loop = FALSE;
	}

	//clear line
	for(int i = 0; i < HSCREEN ;i++) {// 한줄 생성 시 바로 밑부분 지우기
		if (buf_w + i + SIZE_SCREEN < SCROLL_END) // 지우려는 부분이 NSCROLL(100) 을 넘어갈경우
			buf_w[i + SIZE_SCREEN] = 0;//buf_w[i] = 0;
		else
			buf_w[i + SIZE_SCREEN - SIZE_SCROLL] = 0;//buf_w[i - SIZE_SCREEN] = 0;
	}
#else
	CHAR *pScreen = (CHAR *) VIDIO_MEMORY;
	CHAR *pScrBuf = (CHAR *) (VIDIO_MEMORY + 2*80);
	int i;
	for (i = 0; i < 80*24; i++) {
		(*pScreen).bAtt = (*pScrBuf).bAtt;
		(*pScreen++).bCh = (*pScrBuf++).bCh;
	}   
	for (i = 0; i < 80; i++) {
		(*pScreen).bAtt = 0x07;
		(*pScreen++).bCh = ' ';
	} 
#endif
	Glob_y--;

}

#ifdef SERIAL_STDOUT
void printCharToSerial(const char *pString)
{
	int i;
	enum intr_level old_level = intr_disable();
	for(;*pString != NULL; pString++)
	{
		if(*pString != '\n'){
			while((inb(LINE_STATUS) & THR_EMPTY) == 0)
				continue;
			outb(FIRST_SPORT, *pString);

		}
		else{
			for(i=0; i<2; i++){
				while((inb(LINE_STATUS) & THR_EMPTY) == 0)
					continue;
				outb(FIRST_SPORT, next_line[i]);
			}
		}
	}
	intr_set_level (old_level);
}
#endif


int printk(const char *fmt, ...)
{
	char buf[1024];
	va_list args;
	int len;

	set_fallow(); // 현재 출력화면으로 이동

	va_start(args, fmt);
	len = vsprintk(buf, fmt, args);
	va_end(args);

#ifdef SERIAL_STDOUT
	printCharToSerial(buf);
#endif
	PrintCharToScreen(Glob_x, Glob_y, buf);

	return len;
}

#ifdef SCREEN_SCROLL
void scroll_screen(int offset)
{
	if(a_s == TRUE && offset > 0 && buf_p == buf_w)
		return;
	a_s = FALSE;
	buf_p = (char*)((int)buf_p + (HSCREEN * offset));
	
	if (buf_p >= SCROLL_END) // 넘어가면 돌아가기
		buf_p = (char *)((int)buf_p - SIZE_SCROLL);
	else if (buf_p < buf_s)
		buf_p = (char *)((int)buf_p + SIZE_SCROLL);

	if (offset == 1) { // 화면을 밑으로 내릴때
		if (buf_w + SIZE_SCREEN < buf_e) { // 스크린의 끝보다 작을때 = 화면이 스크린내에 있을때
			if (buf_p == buf_w + HSCREEN) // 현재 화면의 바로 위에 도달할때
				buf_p = (char*)((int)buf_p - (HSCREEN * offset));
		}
		else { // 스크린의 끝보다 클때  = 화면이 걸쳐있을 때
			if (buf_p > buf_w + HSCREEN - SIZE_SCROLL) // 현재 화면의 바로 위에 도달할때 
				buf_p = (char*)((int)buf_p - (HSCREEN * offset));
		}
	}
	else if ( offset == -1) { // 화면이 올라갈때
		if (first_loop == TRUE && buf_p == buf_s) // 처음의 buf_p가 맨위의 화면으로 온 경우
			buf_p = (char*)((int)buf_p - (HSCREEN * offset));
		else if (buf_w + SIZE_SCREEN > SCROLL_END) {
			if (buf_w + SIZE_SCREEN - SIZE_SCROLL == buf_p)
				buf_p = (char*)((int)buf_p - (HSCREEN * offset));
		}
		else if (first_loop == FALSE) {
		   if (buf_w + SIZE_SCREEN < SCROLL_END) {
				if (buf_w + SIZE_SCREEN == buf_p)
					buf_p = (char*)((int)buf_p - (HSCREEN * offset));

			}
		}
	}
}

void set_fallow(void)
{
	a_s = TRUE;
}

void refreshScreen(void) // timer_handler가 호출함 매번 화면 갱신
{
	CHAR *p_s= (CHAR *) VIDIO_MEMORY;
	int i;

	if(a_s)
		buf_p = buf_w;

	char* b = buf_p;

	for(i = 0; i < SIZE_SCREEN; i++, b++, p_s++)
	{
		if(b >= SCROLL_END)
			b -= SIZE_SCROLL;
		p_s->bAtt = 0x07;
		p_s->bCh = *b;
	}
}
void set_cursor(void)
{
	unsigned short pos = (Glob_y * HSCREEN) + Glob_x;
	outb(0x3d4, 0x0f); // 하위 커서 위치 레지스터 선택
	outb(0x3d5, (unsigned char)(pos & 0xFF)); // 커서의 하위 바이트 출력
	outb(0x3d4, 0x0e); // 상위 커서 위치 레지스터 선택
	outb(0x3d5, (unsigned char)(pos >> 8)&0xFF); // CRTC 컨트롤 데이터 레지스터에 커서의 상위 바이트 출력
}
#endif

