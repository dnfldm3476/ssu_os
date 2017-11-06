#include <device/kbd.h>
#include <type.h>
#include <device/console.h>
#include <interrupt.h>
#include <device/io.h>
#include <ssulib.h>

static Key_Status KStat;
static char kbd_buf[BUFSIZ];
int buf_head, buf_tail;
int buf_start; // kbd_buf의 내가 쓰기 시작한 위치 저장.
static BYTE Kbd_Map[4][KBDMAPSIZE] = {
	{ /*  default */ // \b추가!!!!!!!
		0x00, 0x00, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
		'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0x00, 'a', 's',
		'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0x00, '\\', 'z', 'x', 'c', 'v',
		'b', 'n', 'm', ',', '.', '/', 0x00, 0x00, 0x00, ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, '-', 0x00, 0x00, 0x00, '+', 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	},
	{ /* capslock  */
		0x00, 0x00, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0x00, '\t',
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n', 0x00, 'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', 0x00, '\\', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0x00, 0x00, 0x00, ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, '-', 0x00, 0x00, 0x00, '+', 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	},
	{ /* Shift */
		0x00, 0x00, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0x00, 0x00,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0x00, 0x00, 'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0x00, '|', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', '<', '>', '?', 0x00, 0x00, 0x00, ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, '-', 0x00, 0x00, 0x00, '+', 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	},
	{ /* Capslock & Shift */
		0x00, 0x00, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0x00, 0x00,
		'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', 0x00, 0x00, 'a', 's',
		'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '\"', '~', 0x00, '|', 'z', 'x', 'c', 'v',
		'b', 'n', 'm', '<', '>', '?', 0x00, 0x00, 0x00, ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, '-', 0x00, 0x00, 0x00, '+', 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	}
};

void init_kbd(void)
{
	KStat.ShiftFlag = 0;
	KStat.CapslockFlag = 0;
	KStat.NumlockFlag = 0;
	KStat.ScrolllockFlag = 0;
	KStat.ExtentedFlag = 0;
	KStat.PauseFlag = 0;

	buf_head = 0;
	buf_tail = 0;
	buf_start = 0;

	reg_handler(33, kbd_handler);
}

void UpdateKeyStat(BYTE Scancode) // keyboard에 따라 scancode가 다르게 나옴
{// 시프트는 누르고 떼기 캡스락은 온오프
	if(Scancode & 0x80)  
	{
		if(Scancode == 0xB6 || Scancode == 0xAA) // 0xB6 = right shift, 0xAA = left shift를 땔때
		{
			KStat.ShiftFlag = FALSE;
		}
	}
	else // 
	{
		if(Scancode == 0x3A && KStat.CapslockFlag) // 0x3A = capslock
		{
			KStat.CapslockFlag = FALSE;
		}
		else if(Scancode == 0x3A)
			KStat.CapslockFlag = TRUE;
		else if(Scancode == 0x36 || Scancode == 0x2A) // 오른쪽 시프트, 왼쪽 시프트 누를때
		{
			KStat.ShiftFlag = TRUE;
		}
		else if(Scancode == 0x45 && KStat.NumlockFlag) // NumlockFlag 확인
		{
			KStat.NumlockFlag = FALSE;
		}
		else if(Scancode == 0x45)
			KStat.NumlockFlag = TRUE;
	}

	if(Scancode == 0xE0) // 0xE0 = extended key
	{
		KStat.ExtentedFlag = TRUE;
	}
	else if(KStat.ExtentedFlag == TRUE && Scancode != 0xE0)
	{
		;//KStat.ExtentedFlag = FALSE;
	}
}

BOOL ConvertScancodeToASCII(BYTE Scancode, BYTE *Asciicode)
{
	if (Scancode == 0x0E) { // backspace 눌렸을 때
		*Asciicode = Kbd_Map[0][Scancode];
		return TRUE;
	}
	if(KStat.PauseFlag > 0)
	{
		KStat.PauseFlag--;
		return FALSE;
	}
	if(Scancode == 0xE1)
	{
		*Asciicode = 0x00;
		KStat.PauseFlag = 2;
		return FALSE;
	}
	else if(Scancode == 0xE0)
	{
		*Asciicode = 0x00;
		return FALSE;
	}
	if(!(Scancode & 0x80))
	{
		if(KStat.ShiftFlag & KStat.CapslockFlag)
		{
			*Asciicode = Kbd_Map[3][Scancode & 0x7F];
		}
		else if(KStat.ShiftFlag)
		{
			*Asciicode = Kbd_Map[2][Scancode & 0x7F];
		}
		else if(KStat.CapslockFlag)
		{
			*Asciicode = Kbd_Map[1][Scancode & 0x7F];
		}
		else
		{
			*Asciicode = Kbd_Map[0][Scancode];
		}

		return TRUE;
	}
	return FALSE;
}

bool isFull()
{
	return (buf_head+1) % BUFSIZ == buf_tail;
}

bool isEmpty()
{
	return buf_head == buf_tail;
}

void kbd_handler(struct intr_frame *iframe)
{
	BYTE asciicode;
	BYTE data = inb(0x60);
	UpdateKeyStat(data);
	if(ConvertScancodeToASCII(data, &asciicode))
	{
		if( !isFull() && asciicode != 0) // printk삭제
		{	
			kbd_buf[buf_tail] = asciicode; 
			buf_tail = (buf_tail + 1) % BUFSIZ;
		}
	}
#ifdef SCREEN_SCROLL
	if (KStat.ExtentedFlag == TRUE || KStat.NumlockFlag == FALSE) { // 확장키 flag가 켜있을때
		switch(data)
		{
			case 0x49: // pageup
				scroll_screen(-1);
				KStat.ExtentedFlag = FALSE;
				break;
			case 0x51 : // pagedown
				scroll_screen(+1);
				KStat.ExtentedFlag = FALSE;
				break;
			case 0xE0:
				break;
			default:
				KStat.ExtentedFlag = FALSE;
				break;
		}
	}	

#endif
}

char kbd_read_char()
{
	if( isEmpty())
		return -1;

	char ret;
	ret = kbd_buf[buf_head];
	if (ret == '\b') {
		if (buf_tail - 1 == buf_start) {// backspace를 첫문자에서 입력할때
			kbd_buf[buf_head] = NULL;
			buf_tail = (buf_tail - 1) % BUFSIZ;
		}
		else {  // backspace 적용
			printk("%c", ret);
			kbd_buf[buf_head] = NULL; // 현재 가리키는 곳과 그 전값을 지움
			kbd_buf[buf_head - 1] = NULL; 
			buf_head = (buf_head - 1) % BUFSIZ;
			buf_tail = (buf_tail - 2) % BUFSIZ;
			return ret; // \b 반환
		}
		return ret;
	}
	else {
		if (ret == '\n' || ret =='\t' || ret == ' ') {
			buf_start = buf_head + 1; // buf_start 갱신
		}
		printk("%c", ret);
		buf_head = (buf_head + 1) %BUFSIZ;
	}
	return ret;
}
