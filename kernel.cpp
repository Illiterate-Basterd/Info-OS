__asm("jmp kmain");

#define MAX_STR 25
#define MAX_COL 80
#define COLOR (0x03)
#define CURSOR_PORT (0x3D4)
#define HOURS (0x528)
#define MINUTES (0x548)
#define SECONDS (0x568)
#define VIDEO_BUF_PTR (0xb8000)
#define IDT_TYPE_INTR (0x0E)
#define IDT_TYPE_TRAP (0x0F)
#define GDT_CS (0x8) // Селектор секции кода, установленный загрузчиком ОС
#define PIC1_PORT (0x20)
#define BACKSPACE (14)
#define ENTER (28)
#define SHIFT (42)


// Структура описывает данные об обработчике прерывания
struct idt_entry
{
	unsigned short base_lo; // Младшие биты адреса обработчика
	unsigned short segm_sel; // Селектор сегмента кода
	unsigned char always0; // Этот байт всегда 0
	unsigned char flags; // Флаги тип. Флаги: P, DPL, Типы - это константы - IDT_TYPE...
	unsigned short base_hi; // Старшие биты адреса обработчика
} __attribute__((packed)); // Выравнивание запрещено

// Структура, адрес которой передается как аргумент команды lidt
struct idt_ptr
{
	unsigned short limit;
	unsigned int base;
} __attribute__((packed)); // Выравнивание запрещено

struct idt_entry g_idt[256]; // Реальная таблица IDT
struct idt_ptr g_idtp; // Описатель таблицы для команды lidt

static inline unsigned char inb(unsigned short port);
static inline void outb(unsigned short port, unsigned char data);
static inline void outw(unsigned short port, unsigned int  data);
static inline void cpuid();
void out_str(int color, const char* ptr);
void out_c(int color, const char character);
void new_line();
void mov_cursor(unsigned int row, unsigned int col);
void clean(bool is_full);
void intr_enable();
void intr_disable();
void intr_init();
void info();
void loadtime();
void keyb_handler();
void on_key(unsigned char scan_code);
bool str_cmp(const char* str1, unsigned char* str2);
void curtime();
void shutdown();
void ticks();
void timer();
void uptime();


unsigned int cur_pos_row = 0;
unsigned int cur_pos_col = 0;
bool shift = false;
unsigned int ticks_counter = 0;

char symbols[] = {0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 14, 0, 'q', 'w', 'e', 'r', 't', 'y',
 'u', 'i', 'o', 'p', 0, 0, 28, 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 0, 0, 0, 0, 0, 'z', 'x', 'c', 'v', 'b', 'n', 'm',
 0, 0, '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0};

void help()
{
	out_str(COLOR, "  help - display all commands");
	new_line();
	out_str(COLOR, "  info - display the basic info of the system");
        new_line();
        out_str(COLOR, "  clear - clear the console");
        new_line();
        out_str(COLOR, "  ticks - display the amount of ticks elapsed since the system boot");
        new_line();
        out_str(COLOR, "  loadtime - display the system startup time");
        new_line();
        out_str(COLOR, "  curtime - display current time");
        new_line();
        out_str(COLOR, "  uptime - display system running time");
        new_line();
        out_str(COLOR, "  cpuid - display processor Vendor ID");
        new_line();
        out_str(COLOR, "  shutdown - power off");
        new_line();
}

// Пустой обработчик прерываний. Другие обработчики могут быть реализованы по этому шаблону
void default_intr_handler()
{
	asm("pusha");
	// ... (реализация обработки)
	asm("popa; leave; iret");
}

typedef void (*intr_handler)();

void intr_reg_handler(int num, unsigned short segm_sel, unsigned short flags, intr_handler hndlr)
{
	unsigned int hndlr_addr = (unsigned int) hndlr;
	g_idt[num].base_lo = (unsigned short) (hndlr_addr & 0xFFFF);
	g_idt[num].segm_sel = segm_sel;
	g_idt[num].always0 = 0;
	g_idt[num].flags = flags;
	g_idt[num].base_hi = (unsigned short) (hndlr_addr >> 16);
}

// Функция инициализации системы прерываний: заполнение массива с адресами обработчиков
void intr_init()
{
	int i;
	int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
	for(i = 0; i < idt_count; i++)
	intr_reg_handler(i, GDT_CS, 0x80 | IDT_TYPE_INTR, default_intr_handler); // segm_sel=0x8, P=1, DPL=0, Type=Intr
}

void intr_start()
{
	int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
	g_idtp.base = (unsigned int) (&g_idt[0]);
	g_idtp.limit = (sizeof (struct idt_entry) * idt_count) - 1;
	asm("lidt %0" : : "m" (g_idtp) );
}

void intr_enable()
{
	asm("sti");
}

void intr_disable()
{
	asm("cli");
}

bool str_cmp(const char* str1, unsigned char* str2)
{
	unsigned short i = 0, j = 0;
	
	for (i, j; str1[i] != 0; i++, j++)
	{
		if(str1[i] != str2[j])
			return false;
	}
	return true;
} 

unsigned char bcd_to_dec(unsigned char c)
{
	unsigned char dec;

	dec = (c >> 4) & 0x07;
	dec = (dec << 3) + (dec << 1) + (c & 0x0F);
	
	return dec;
}

void shutdown()
{	
	new_line();
	out_str(COLOR, "Powering off...");
	outw(0x604, 0x2000);
}

void loadtime()
{
	unsigned char hrs, min, sec;

	unsigned char* hours = (unsigned char*)HOURS;
	unsigned char* mins = (unsigned char*)MINUTES;
	unsigned char* secs = (unsigned char*)SECONDS;
	
	hrs = bcd_to_dec(hours[0]);
	min = bcd_to_dec(mins[0]);
	sec = bcd_to_dec(secs[0]);

	if (hrs >= 24)
		hrs -= 24;

	out_str(COLOR, "OS loaded at: ");
	out_c(COLOR, hrs / 10 + '0');
	out_c(COLOR, hrs % 10 + '0');
	out_c(COLOR, ':');
        out_c(COLOR, min / 10 + '0');
        out_c(COLOR, min % 10 + '0');
        out_c(COLOR, ':');
        out_c(COLOR, sec / 10 + '0');
        out_c(COLOR, sec % 10 + '0');
        new_line();

}

void curtime()
{
	unsigned char sec, min, hrs;
	char  secs[3], mins[4], hours[4];

	outb(0x70, 4);
	hrs = inb(0x71);
	outb(0x70, 2);
	min = inb(0x71);
	outb(0x70, 0);
	sec = inb(0x71);

	hrs = bcd_to_dec(hrs) + 3;
	min = bcd_to_dec(min);
	sec = bcd_to_dec(sec);

	if (hrs >= 24)
		hrs -= 24;

	hours[0] = hrs / 10 + '0';
	hours[1] = hrs % 10 + '0';
	hours[2] = ':';
	hours[3] = '\0';

	mins[0] = min / 10 + '0';
        mins[1] = min % 10 + '0';
        mins[2] = ':';
	mins[3] = '\0';

	secs[0] = sec / 10 + '0';
        secs[1] = sec % 10 + '0';
        secs[2] = '\0';

	out_str(COLOR, "Current time: ");
	out_str(COLOR, hours);
	out_str(COLOR, mins);
	out_str(COLOR, secs);
	new_line();

}

void uptime()
{
	unsigned int  seconds = ticks_counter / 18, minutes = 0, hours = 0;
	while(seconds > 60)
	{
		minutes++;
		seconds -= 60;
	}
	while(minutes > 60)
	{
		hours++;
		minutes -= 60;
	}
	unsigned int sec = seconds, min = minutes, hour = hours;
	char secs[3], mins[3], hrs[3];
	secs[1] = seconds % 10 + '0';
	seconds /= 10;
	secs[0] = seconds % 10 + '0';
	secs[2] = '\0';
        mins[1] = minutes % 10 + '0';
        minutes /= 10;
        mins[0] = minutes % 10 + '0';
        mins[2] = '\0';
        hrs[1] = hours % 10 + '0';
        hours /= 10;
        hrs[0] = hours % 10 + '0';
        hrs[2] = '\0';

	out_str(COLOR, "Uptime is ");
	out_str(COLOR, hrs);
	if(hour == 1)
		out_str(COLOR, " hour ");
	else
		out_str(COLOR, " hours ");
	out_str(COLOR, mins);
	if(min == 1)
		out_str(COLOR, " minute ");
	else
		out_str(COLOR, " minutes ");
	out_str(COLOR, secs);
	if(sec == 1)
		out_str(COLOR, " second ");
	else
		out_str(COLOR, " seconds ");
	new_line();
}

static inline void cpuid()
{
        unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
        asm volatile("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "0" (eax));
        char* cb = (char*)&ebx, *cc = (char*)&ecx, *cd = (char*)&edx;
        out_str(COLOR, "Vendor ID: ");
	out_c(COLOR, cb[0]);
        out_c(COLOR, cb[1]);
        out_c(COLOR, cb[2]);
        out_c(COLOR, cb[3]);
	out_c(COLOR, cd[0]);
        out_c(COLOR, cd[1]);
        out_c(COLOR, cd[2]);
        out_c(COLOR, cd[3]);
        out_c(COLOR, cc[0]);
        out_c(COLOR, cc[1]);
        out_c(COLOR, cc[2]);
        out_c(COLOR, cc[3]);
	new_line();

}


unsigned short action(unsigned char* action_str)
{
	unsigned short result;

	if (str_cmp("help", action_str))
		result = 1;
        else if (str_cmp("info", action_str))
                result = 2;
        else if (str_cmp("clear", action_str))
                result = 3;
        else if (str_cmp("ticks", action_str))
                result = 4;
        else if (str_cmp("loadtime", action_str))
                result = 5;
        else if (str_cmp("curtime", action_str))
                result = 6;
        else if (str_cmp("uptime", action_str))
                result = 7;
        else if (str_cmp("cpuid", action_str))
                result = 8;
        else if (str_cmp("shutdown", action_str))
                result = 9;
	else result = 0;
	
	return result;
}

void on_key(unsigned char scan_code)
{
	unsigned short i = 0, action_num = 0;
	unsigned char action_str[40];
	unsigned char* action_ptr;
	
	switch(scan_code)
	{
		case ENTER:
			action_ptr = (unsigned char*)(2 * MAX_COL * cur_pos_row + VIDEO_BUF_PTR);
			for (i; i < 40; i++)
			{
				if(*action_ptr == 0)
					break;
				action_str[i] = *action_ptr;
				action_ptr += 2;
			}
			action_str[i] = 0;
			action_num = action(action_str);
			
			switch(action_num)
			{
				case 0:
					new_line();
					out_str(COLOR, "Invalid action!");
					new_line();
					break;
				case 1:
					new_line();
					help();
					break;
				case 2:
					new_line();
					info();
					break;
				case 3:
					clean(0);
					break;
				case 4:
					new_line();
					ticks();
					break;
				case 5:
					new_line();
					loadtime();
					break;
				case 6:
					new_line();
					curtime();
					break;
				case 7:
					new_line();
					uptime();
					break;
				case 8:
					new_line();
					cpuid();
					break;
				case 9:
					shutdown();
					break;
			}
			break;
		case BACKSPACE:
			if(cur_pos_col)
			{
				action_ptr = (unsigned char*)(VIDEO_BUF_PTR + MAX_COL * cur_pos_row * 2 + cur_pos_col * 2);
				cur_pos_col--;
				action_ptr -= 2;
				*action_ptr = 0;
				mov_cursor(cur_pos_row, cur_pos_col);
			}
			break;
		case SHIFT:
			shift = true;
			break;
		default:
			if (scan_code >= 128 || cur_pos_col >= 40)
				return;
			if (!shift)
			{
				if(!symbols[scan_code])
					return;
				out_c(COLOR, symbols[scan_code]);
				clean(1);
			}
			else
			{
				if(!symbols[scan_code])
                                        return;
				if(scan_code == 9)
				{
					out_c(COLOR, '*');
					clean(1);
				}
				if(scan_code == 13)
				{
					out_c(COLOR, '+');
					clean(1);
				}
				shift = false;
			}
			break;
	}
}

void keyb_init()
{
// Регистрация обработчика прерывания
	intr_reg_handler(0x09, GDT_CS, 0x80 | IDT_TYPE_INTR, keyb_handler);
// segm_sel=0x8, P=1, DPL=0, Type=Intr

// Разрешение только прерываний клавиатуры от контроллера 8259
	outb(PIC1_PORT + 1, 0xFF ^ 0x02); // 0xFF - все прерывания, 0x02 - бит IRQ1 (клавиатура).
// Разрешены будут только прерывания, чьи биты установлены в 0
}

void keyb_process_keys()
{
	// Проверка что буфер PS/2 клавиатуры не пуст (младший бит присутствует)
	if (inb(0x64) & 0x01)
	{
		unsigned char scan_code;
		unsigned char state;
		scan_code = inb(0x60); // Считывание символа с PS/2 клавиатуры
		if (scan_code < 128) // Скан-коды выше 128 - это отпускание клавиши 
		on_key(scan_code);
	}
}

void keyb_handler()
{
	asm("pusha");
	// Обработка поступивших данных
	keyb_process_keys();
	// Отправка контроллеру 8259 нотификации о том, что прерывание обработано
	outb(PIC1_PORT, 0x20);
	asm("popa; leave; iret");
}

void timer_init()
{
        intr_reg_handler(0x08, GDT_CS, 0x80 | IDT_TYPE_INTR, timer);

        outb(PIC1_PORT + 1, 0xFF ^ 0x02 ^ 0x01);
}

void timer()
{
	asm("pusha");
        ticks_counter++;
        // Отправка контроллеру 8259 нотификации о том, что прерывание обработано
        outb(PIC1_PORT, 0x20);
        asm("popa; leave; iret");
}

void ticks()
{
	unsigned int len = 0, ticks_cnt = ticks_counter;
	char ticks_str[16];
	char tmp;

	while(ticks_cnt > 0)
	{
		ticks_str[len] = ticks_cnt % 10 + '0';
		ticks_cnt /= 10;
		len++;
	}
	ticks_str[len] = '\0';

	for(int i = 0, j = len - 1; i < j; i++, j--)
	{
		tmp = ticks_str[i];
		ticks_str[i] = ticks_str[j];
		ticks_str[j] = tmp;
	}
	out_str(COLOR, "Current ticks: ");
	out_str(COLOR, ticks_str);
	new_line();
}

void new_line()
{
	cur_pos_col = 0;
	cur_pos_row++;
	clean(true);
}

void info()
{
	out_str(COLOR, "InfoOS by Rurua Bogdan, 4851003/10002, 2023");
	new_line();
	out_str(COLOR, "Assembley: YASM, C/C++ Compiler: gcc");
	new_line();
}



void mov_cursor(unsigned int row, unsigned int col)
{
	unsigned short new_pos = (row * MAX_COL) + col;
	outb(CURSOR_PORT, 0x0F);
	outb(CURSOR_PORT + 1, (unsigned char)(new_pos & 0xFF));
	outb(CURSOR_PORT, 0x0E);
	outb(CURSOR_PORT + 1, (unsigned char)((new_pos >> 8) & 0xFF));
}

static inline unsigned char inb(unsigned short port)
{
	unsigned char data;
	asm volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
	return data; 
}

static inline void outb(unsigned short port, unsigned char data)
{
	asm volatile ("outb %b0, %w1" : : "a" (data), "Nd" (port));
}

static inline void outw(unsigned short port, unsigned int data)
{
        asm volatile ("outw %w0, %w1" : : "a" (data), "Nd" (port));
}

void clean(bool is_full)
{
	if(is_full && (cur_pos_row != MAX_STR))
	{
		mov_cursor(cur_pos_row, cur_pos_col);
		return;
	}
	unsigned char* video_ptr;
	int i, max;
	video_ptr = (unsigned char*)VIDEO_BUF_PTR;
	max = MAX_STR * MAX_COL;
	i = 0;
	for(i; i < max; i++)
	{
		*video_ptr = 0;
		video_ptr = video_ptr + 2;
	}
	cur_pos_col = cur_pos_row = 0;
	mov_cursor(cur_pos_row, cur_pos_col);
}

void out_str(int color, const char* ptr)
{
	unsigned char* video_buf = (unsigned char*)VIDEO_BUF_PTR;
	video_buf += 2 * MAX_COL * cur_pos_row + cur_pos_col * 2;
	while (*ptr)
	{
		cur_pos_col++;
		video_buf[0] = (unsigned char)*ptr;
		video_buf[1] = color; 
		video_buf += 2;
		ptr++;
	}
}

void out_c(int color, const char character)
{
	unsigned char* video_buf = (unsigned char*)VIDEO_BUF_PTR;
	video_buf += 2 * MAX_COL * cur_pos_row + cur_pos_col * 2;
	cur_pos_col++;
	video_buf[0] = (unsigned char)character;
	video_buf[1] = color;
}

extern "C" int kmain()
{
	ticks_counter = 0;
	const char* greetings = "Greetings! Welcome to InfoOS!";
	intr_disable();
	intr_init();
	keyb_init();
	intr_start();
	intr_enable();
	timer_init();
	clean(false);

	out_str(COLOR, greetings);
	new_line();

	while (1) { asm("hlt"); }

	return 0;
}
