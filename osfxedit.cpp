#include <c64/sid.h>
#include <c64/keyboard.h>
#include <c64/memmap.h>
#include <c64/cia.h>
#include <c64/vic.h>
#include <c64/rasterirq.h>
#include <c64/iecbus.h>
#include <c64/sprites.h>
#include <audio/sidfx.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static char * const Screen = (char *)0xc000;
static char * const Hires = (char *)0xe000;
static char * const Color = (char *)0xd800;
static char * const Font = (char *)0xd000;
static char * const Sprites = (char *)0xd000;

char keyb_queue, keyb_repeat;
char csr_cnt;
char irq_cnt;

__interrupt void isr(void)
{
	irq_cnt++;
	csr_cnt++;

	vic.color_border = VCOL_YELLOW;
	sidfx_loop();
	vic.color_border = VCOL_LT_BLUE;
	keyb_poll();
	vic.color_border = VCOL_ORANGE;

	if (!keyb_queue)
	{
		if (!(keyb_key & KSCAN_QUAL_DOWN))
		{
			if (keyb_repeat)
				keyb_repeat--;
			else
			{
				if (key_pressed(KSCAN_CSR_RIGHT))
				{
					if (key_shift())
						keyb_queue = KSCAN_CSR_RIGHT | KSCAN_QUAL_DOWN | KSCAN_QUAL_SHIFT;
					else
						keyb_queue = KSCAN_CSR_RIGHT | KSCAN_QUAL_DOWN;
					keyb_repeat = 2;
				}
			}
		}
		else
		{
			keyb_queue = keyb_key;
			keyb_repeat = 20;
		}
	}
	vic.color_border = VCOL_BLACK;
}

RIRQCode	rirq_isr, rirq_mark0, rirq_mark1, rirq_bitmap;

const SIDFX basefx = {
	1000, 2048, 
	SID_CTRL_GATE | SID_CTRL_SAW,
	SID_ATK_8 | SID_DKY_24,
	0x80 | SID_DKY_204,
	0, 0,
	4, 0,
	0
};

SIDFX	effects[10];

char 	neffects = 1;

const char SidHead[] = S"  TSRNG FREQ  PWM  ADSR DFREQ DPWM T1 T0";
const char SidRow[]  = S"# TSRNG 00000 0000 0000 00000 0000 00 00";
const char MenuRow[] = S"LOAD SAVE NEW  UNDO [..................]";

const char HexDigit[] = S"0123456789ABCDEF";


void uto5digit(unsigned u, char * d)
{
	for(signed char i=4; i>=0; i--)
	{
		d[i] = u % 10 + 48;
		u /= 10;
	}
}

char cursorX, cursorY;

void showmenu(void)
{
	char * dp = Screen + 11 * 40;
	char * cp = Color + 11 * 40;

	for(char i=0; i<40; i++)
	{
		dp[i] = MenuRow[i];
		cp[i] = VCOL_LT_BLUE;
	}
}

void hires_draw_start(void);

void showfxs_row(char n)
{
	char * dp = Screen + 40 * (n + 1);
	char * cp = Color + 40 * (n + 1);

	for(char i=0; i<40; i++)
	{
		dp[i] = SidRow[i];
		cp[i] = VCOL_DARK_GREY;
	}

	if (n < neffects)
	{
		char fs[6];
		dp[0] = '0' + n;
		cp[0] = VCOL_YELLOW;

		SIDFX	&	s = effects[n];
		if (s.ctrl & SID_CTRL_TRI)   cp[2] = VCOL_YELLOW;
		if (s.ctrl & SID_CTRL_SAW)   cp[3] = VCOL_YELLOW;
		if (s.ctrl & SID_CTRL_RECT)  cp[4] = VCOL_YELLOW;
		if (s.ctrl & SID_CTRL_NOISE) cp[5] = VCOL_YELLOW;
		if (s.ctrl & SID_CTRL_GATE)  cp[6] = VCOL_YELLOW;

		uto5digit(s.freq, fs);
		for(char i=0; i<5; i++)
		{
			dp[8 + i] = fs[i];
			cp[8 + i] = VCOL_LT_GREY;
		}

		uto5digit(s.pwm, fs);
		for(char i=0; i<4; i++)
		{
			dp[14 + i] = fs[i + 1];
			cp[14 + i] = VCOL_LT_GREY;
		}

		if (s.dfreq < 0)
		{
			uto5digit(- s.dfreq, fs);
			for(char i=0; i<5; i++)
			{
				dp[24 + i] = fs[i];
				cp[24 + i] = VCOL_ORANGE;
			}				
		}
		else
		{
			uto5digit(s.dfreq, fs);
			for(char i=0; i<5; i++)
			{
				dp[24 + i] = fs[i];
				cp[24 + i] = VCOL_LT_GREY;
			}								
		}

		if (s.dpwm < 0)
		{
			uto5digit(- s.dpwm, fs);
			for(char i=0; i<4; i++)
			{
				dp[30 + i] = fs[i + 1];
				cp[30 + i] = VCOL_ORANGE;
			}				
		}
		else
		{
			uto5digit(s.dpwm, fs);
			for(char i=0; i<4; i++)
			{
				dp[30 + i] = fs[i + 1];
				cp[30 + i] = VCOL_LT_GREY;
			}								
		}

		dp[19] = HexDigit[s.attdec >> 4];   cp[19] = VCOL_YELLOW;
		dp[20] = HexDigit[s.attdec & 0x0f]; cp[20] = VCOL_YELLOW;
		dp[21] = HexDigit[s.susrel >> 4];   cp[21] = VCOL_YELLOW;
		dp[22] = HexDigit[s.susrel & 0x0f]; cp[22] = VCOL_YELLOW;

		uto5digit(s.time1, fs);
		for(char i=0; i<2; i++)
		{
			dp[35 + i] = fs[i + 3];
			cp[35 + i] = VCOL_LT_GREY;
		}
		uto5digit(s.time0, fs);
		for(char i=0; i<2; i++)
		{
			dp[38 + i] = fs[i + 3];
			cp[38 + i] = VCOL_LT_GREY;
		}


	}		
}

void showfxs(void)
{
	char * dp = Screen;
	char * cp = Color;

	for(char i=0; i<40; i++)
	{
		dp[i] = SidHead[i];
		cp[i] = VCOL_LT_BLUE;
	}

	for(char i=0; i<10; i++)
		showfxs_row(i);
}

char kscan_digits[] = {
	KSCAN_0,
	KSCAN_1,
	KSCAN_2,
	KSCAN_3,
	KSCAN_4,
	KSCAN_5,
	KSCAN_6,
	KSCAN_7,
	KSCAN_8,
	KSCAN_9,
	KSCAN_A,
	KSCAN_B,
	KSCAN_C,
	KSCAN_D,
	KSCAN_E,
	KSCAN_F
};

void check_digit(SIDFX & s, char d)
{
	switch (cursorX)
	{
		case 19: s.attdec = (s.attdec & 0x0f) | (d << 4); break;
		case 20: s.attdec = (s.attdec & 0xf0) | d; break;
		case 21: s.susrel = (s.susrel & 0x0f) | (d << 4); break;
		case 22: s.susrel = (s.susrel & 0xf0) | d; break;
	}

	if (d < 10)
	{
		switch (cursorX)
		{
		case 8:  s.freq = s.freq % 10000 + d * 10000; cursorX++; break;
		case 9:  s.freq = s.freq % 1000 + (s.freq / 10000) * 10000 + d * 1000; cursorX++; break;
		case 10: s.freq = s.freq % 100 + (s.freq / 1000) * 1000 + d * 100; cursorX++; break;
		case 11: s.freq = s.freq % 10 + (s.freq / 100) * 100 + d * 10; cursorX++; break;
		case 12: s.freq = (s.freq / 10 * 10) + d; break;

		case 14: s.pwm = s.pwm % 1000 + (s.pwm / 10000) * 10000 + d * 1000; cursorX++; break;
		case 15: s.pwm = s.pwm % 100 + (s.pwm / 1000) * 1000 + d * 100; cursorX++; break;
		case 16: s.pwm = s.pwm % 10 + (s.pwm / 100) * 100 + d * 10; cursorX++; break;
		case 17: s.pwm = (s.pwm / 10 * 10) + d; break;

		case 35: s.time1 = s.time1 % 10 + d * 10; cursorX++; break;
		case 36: s.time1 = (s.time1 / 10 * 10) + d; break;

		case 38: s.time0 = s.time0 % 10 + d * 10; cursorX++; break;
		case 39: s.time0 = (s.time0 / 10 * 10) + d; break;
		}
	}
}

void edit_filename(char * fn)
{
	char * dp = Screen + 11 * 40;
	char i = 0;
	while (dp[21 + i] != '.')
	{
		char ch = dp[21 + i];
		if (ch >= '0' && ch <= '9' || ch == '-')
			fn[i] = ch;
		else if (ch >= S'A' && ch <= S'Z')
			fn[i] = ch + p'a' - S'A';
		i++;
	}
	fn[i] = 0;
}

char save_drive = 9;

char msg_buffer[40];

void iec_read_status(void)
{
	iec_talk(save_drive, 15);
	char i = 0;
	while (iec_status == IEC_OK)
		msg_buffer[i++] = iec_read();
	msg_buffer[i] = 0;
	iec_untalk();
}

void edit_load(void)
{
	rirq_stop();

	bool ok = false;

	char fname[24];

	strcpy(fname, "@0:");
	edit_filename(fname);
	strcat(fname, ",P,R");

	iec_open(save_drive, 2, fname);
	iec_talk(save_drive, 2);

	// Magic code and save file version
	char v = iec_read();
	if (iec_status == IEC_OK && v >= 0xb3)
	{
		neffects = iec_read();
		iec_read_bytes((char *)effects, sizeof(SIDFX) * neffects);
		ok = true;
	}

	iec_untalk();
	iec_close(save_drive, 2);

	if (!ok)
		iec_read_status();

	rirq_start();
}

void edit_save(void)
{
	rirq_stop();

	bool ok = false;

	char fname[24];

	strcpy(fname, "@0:");
	edit_filename(fname + 3);
	strcat(fname, ",P,W");

	iec_open(save_drive, 2, fname);
	iec_listen(save_drive, 2);

	// Magic code and save file version
	iec_write(0xb3);
	if (iec_status < IEC_ERROR)
	{
		iec_write(neffects);
		iec_write_bytes((char *)effects, sizeof(SIDFX) * neffects);
		if (iec_status < IEC_ERROR)
			ok = true;
	}

	iec_unlisten();
	iec_close(save_drive, 2);

	if (ok)
	{
		strcpy(fname, "@0:");
		edit_filename(fname + 3);
		strcat(fname, p".c" ",P,W");

		iec_open(save_drive, 2, fname);
		iec_listen(save_drive, 2);

		edit_filename(fname);

		char buffer[200];
		sprintf(buffer, "static const SIDFX SFX_%s[] = {\n", fname);
		iec_write_bytes(buffer, strlen(buffer));

		for(char i=0; i<neffects; i++)
		{
			const SIDFX & s(effects[i]);
			sprintf(buffer, "\t{%u, %u, 0x%02x, 0x%02x, 0x%02x, %d, %d, %d, %d, 0},\n",
				s.freq, s.pwm, s.ctrl, s.attdec, s.susrel, s.dfreq, s.dpwm, s.time1, s.time0);
			iec_write_bytes(buffer, strlen(buffer));
		}
		sprintf(buffer, "};\n");
		iec_write_bytes(buffer, strlen(buffer));

		iec_unlisten();
		iec_close(save_drive, 2);
	}

	if (!ok)
		iec_read_status();

	rirq_start();
}

void edit_new(void)
{
	neffects = 1;
	effects[0] = basefx;
}

void edit_undo(void)
{

}

char * menup = Screen + 11 * 40;

void edit_menu(char k)
{

	switch(k)
	{
	case KSCAN_CSR_DOWN | KSCAN_QUAL_SHIFT:
		if (neffects < 10)
			cursorY = neffects;
		else
			cursorY = 9;
		break;
	case KSCAN_CSR_RIGHT:
		if (cursorX < 15)
			cursorX += 5;
		else if (cursorX == 15)
			cursorX = 21;
		else if (cursorX < 38 && menup[cursorX] != '.')
			cursorX++;
		break;
	case KSCAN_CSR_RIGHT | KSCAN_QUAL_SHIFT:
		if (cursorX > 21)
			cursorX --;
		else if (cursorX == 21)
			cursorX = 15;
		else if (cursorX > 0)
			cursorX -= 5;
		break;
	case KSCAN_RETURN:
		switch (cursorX)
		{
		case 0:
			edit_load();
			showfxs();
			hires_draw_start();
			break;
		case 5:
			edit_save();
			break;
		case 10:
			edit_new();
			showfxs();
			hires_draw_start();
			break;
		case 15:
			edit_undo();
			break;
		default:
			cursorX = 0;
			break;
		}
		break;
	case KSCAN_DEL:
		if (cursorX > 21)
		{
			cursorX--;
			for(char i=cursorX; i<38; i++)
				menup[i] = menup[i + 1];
			menup[38] = '.';
		}
		break;
	case KSCAN_HOME:
		cursorX = 0;
		cursorY = 0;
		break;
	default:
		if (cursorX >= 21)
		{
			char ch = keyb_codes[k];
			if (ch >= '0' && ch <= '9' || ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' || ch == '-')
			{
				if (ch >= 'a' && ch <= 'z')
					ch -= 'a' - S'A';
				else if (ch >= 'A' && ch <= 'Z')
					ch -= 'A' - S'A';

				for(char i=38; i>=cursorX; i--)
					menup[i + 1] = menup[i];
				menup[cursorX] = ch;
				if (cursorX < 38)
					cursorX++;
			}
		}
		break;
	}
}

void edit_effects(char k)
{
	bool	restart = false;
	bool	redraw = false;

	SIDFX	&	s = effects[cursorY];

	switch (k)
	{
	case KSCAN_RETURN:
		cursorX = 0;
		if (cursorY < 9)
			cursorY++;
		break;
	case KSCAN_SPACE:
		restart = true;
		break;
	case KSCAN_CSR_DOWN:
		if (cursorY < neffects)
			cursorY++;
		else
			cursorY = 10;
		break;
	case KSCAN_CSR_DOWN | KSCAN_QUAL_SHIFT:
		if (cursorY > 0)
			cursorY--;
		break;
	case KSCAN_CSR_RIGHT:
		if (cursorX < 39)
			cursorX++;
		break;
	case KSCAN_CSR_RIGHT | KSCAN_QUAL_SHIFT:
		if (cursorX > 0)
			cursorX--;
		break;
	case KSCAN_HOME:
		cursorX = 0;
		break;
	case KSCAN_PLUS:
	case KSCAN_DOT:
		switch (cursorX)
		{
		case 0:
			 if (neffects < 10)
			 {
			 	if (cursorY == neffects)
			 		effects[neffects] = basefx;
			 	else
			 	{
				 	for(char i=neffects; i>cursorY; i--)
				 		effects[i] = effects[i - 1];			 		
			 	}

			 	neffects++; 
			 } 
			 break;

		case 2: s.ctrl |= SID_CTRL_TRI; s.ctrl &= ~SID_CTRL_NOISE; break;
		case 3: s.ctrl |= SID_CTRL_SAW; s.ctrl &= ~SID_CTRL_NOISE;break;
		case 4: s.ctrl |= SID_CTRL_RECT; s.ctrl &= ~SID_CTRL_NOISE;break;
		case 5: s.ctrl |= SID_CTRL_NOISE; s.ctrl &= ~(SID_CTRL_TRI | SID_CTRL_SAW | SID_CTRL_RECT); break;
		case 6: s.ctrl |= SID_CTRL_GATE; break;

		case  8: if (s.freq < 55536) s.freq += 10000; break;
		case  9: if (s.freq < 64536) s.freq +=  1000; break;
		case 10: if (s.freq < 65436) s.freq +=   100; break;
		case 11: if (s.freq < 65526) s.freq +=    10; break;
		case 12: if (s.freq < 65535) s.freq +=     1; break;

		case 14: if (s.pwm < 3096) s.pwm +=  1000; break;
		case 15: if (s.pwm < 3996) s.pwm +=   100; break;
		case 16: if (s.pwm < 4086) s.pwm +=    10; break;
		case 17: if (s.pwm < 4085) s.pwm +=     1; break;

		case 19: if ((s.attdec & 0xf0) < 0xf0) s.attdec += 0x10; break;
		case 20: if ((s.attdec & 0x0f) < 0x0f) s.attdec += 0x01; break;
		case 21: if ((s.susrel & 0xf0) < 0xf0) s.susrel += 0x10; break;
		case 22: if ((s.susrel & 0x0f) < 0x0f) s.susrel += 0x01; break;

		case 24: if (s.dfreq < 22768) s.dfreq += 10000; break;
		case 25: if (s.dfreq < 31768) s.dfreq +=  1000; break;
		case 26: if (s.dfreq < 32668) s.dfreq +=   100; break;
		case 27: if (s.dfreq < 32758) s.dfreq +=    10; break;
		case 28: if (s.dfreq < 32767) s.dfreq +=     1; break;

		case 30: if (s.dpwm < 3096) s.dpwm +=  1000; break;
		case 31: if (s.dpwm < 3996) s.dpwm +=   100; break;
		case 32: if (s.dpwm < 4086) s.dpwm +=    10; break;
		case 33: if (s.dpwm < 4095) s.dpwm +=     1; break;

		case 35: if (s.time1 < 90) s.time1 += 10; break;
		case 36: if (s.time1 < 99) s.time1 +=  1; break;
		case 38: if (s.time0 < 90) s.time0 += 10; break;
		case 39: if (s.time0 < 99) s.time0 +=  1; break;

		}
		restart = true;
		redraw = true;
		break;
	case KSCAN_MINUS:
	case KSCAN_COMMA:
		switch (cursorX)
		{
		case 0: 
			if (neffects > 1)
			{
				neffects--;
				for(char i=cursorY; i<neffects; i++)
					effects[i] = effects[i + 1];						
			}
			break;

		case 2: s.ctrl &= ~SID_CTRL_TRI; break;
		case 3: s.ctrl &= ~SID_CTRL_SAW; break;
		case 4: s.ctrl &= ~SID_CTRL_RECT; break;
		case 5: s.ctrl &= ~SID_CTRL_NOISE; break;
		case 6: s.ctrl &= ~SID_CTRL_GATE; break;

		case  8: if (s.freq >= 10000) s.freq -= 10000; break;
		case  9: if (s.freq >=  1000) s.freq -=  1000; break;
		case 10: if (s.freq >=   100) s.freq -=   100; break;
		case 11: if (s.freq >=    10) s.freq -=    10; break;
		case 12: if (s.freq >=     1) s.freq -=     1; break;

		case 14: if (s.pwm >=  1000) s.pwm -=  1000; break;
		case 15: if (s.pwm >=   100) s.pwm -=   100; break;
		case 16: if (s.pwm >=    10) s.pwm -=    10; break;
		case 17: if (s.pwm >=     1) s.pwm -=     1; break;

		case 19: if ((s.attdec & 0xf0) > 0x00) s.attdec -= 0x10; break;
		case 20: if ((s.attdec & 0x0f) > 0x00) s.attdec -= 0x01; break;
		case 21: if ((s.susrel & 0xf0) > 0x00) s.susrel -= 0x10; break;
		case 22: if ((s.susrel & 0x0f) > 0x00) s.susrel -= 0x01; break;

		case 24: if (s.dfreq > -22768) s.dfreq -= 10000; break;
		case 25: if (s.dfreq > -31768) s.dfreq -=  1000; break;
		case 26: if (s.dfreq > -32668) s.dfreq -=   100; break;
		case 27: if (s.dfreq > -32758) s.dfreq -=    10; break;
		case 28: if (s.dfreq > -32767) s.dfreq -=     1; break;

		case 30: if (s.dpwm > -3096) s.dpwm -=  1000; break;
		case 31: if (s.dpwm > -3996) s.dpwm -=   100; break;
		case 32: if (s.dpwm > -4086) s.dpwm -=    10; break;
		case 33: if (s.dpwm > -4095) s.dpwm -=     1; break;

		case 35: if (s.time1 >= 10) s.time1 -= 10; break;
		case 36: if (s.time1 >   0) s.time1 -=  1; break;
		case 38: if (s.time0 >= 10) s.time0 -= 10; break;
		case 39: if (s.time0 >   0) s.time0 -=  1; break;
		}
		restart = true;
		redraw = true;
		break;

	default:
		char i = 0;
		while (i < 16 && k != kscan_digits[i])
			i++;
		if (i < 16)
		{
			check_digit(s, i); 
			restart = true;
			redraw = true;
		}
		break;

	}

	if (restart)
	{
		sidfx_stop(0);
		sidfx_play(0, effects, neffects);
		irq_cnt = 0;
	}

	if (redraw)
	{
		showfxs_row(cursorY);		
		hires_draw_start();
	}

}

enum Phase
{
	PHASE_RELEASE,
	PHASE_OFF,
	PHASE_ATTACK,
	PHASE_DECAY
};

enum SIDFXState
{
	SIDFX_IDLE,
	SIDFX_RESET_0,
	SIDFX_READY,
	SIDFX_PLAY,
	SIDFX_WAIT
};

struct VirtualSID
{	
	Phase			phase;
	char			ctrl;
	char			attdec, susrel;
	unsigned		adsr, freq, pwm;
	char			tick, delay, pos;
	SIDFXState		state;

}	vsid;

// Maximum value and per frame step for ADSR emulation
static const unsigned AMAX	= 31 * 256;
static const unsigned TSTEP = 20; // ms/frame
static const unsigned ASTEP = (unsigned long)AMAX * TSTEP / 4;

// ADSR constants
static const unsigned AttackStep[16] = {
	ASTEP / 2,   ASTEP / 8,  ASTEP / 16, ASTEP / 24,
	ASTEP / 38,   ASTEP / 56,  ASTEP / 68, ASTEP / 80,
	ASTEP / 100,   ASTEP / 250,  ASTEP / 500, ASTEP / 800,
	ASTEP / 1000,   ASTEP / 3000,  ASTEP / 5000, ASTEP / 8000
};

static const unsigned DecayStep[16] = {
	ASTEP / 6,   ASTEP / 24,  ASTEP / 48, ASTEP / 72,
	ASTEP / 114,   ASTEP / 168,  ASTEP / 204, ASTEP / 240,
	ASTEP / 300,   ASTEP / 750,  ASTEP / 1500, ASTEP / 2400,
	ASTEP / 3000,   ASTEP / 9000,  ASTEP / 15000, ASTEP / 24000
};


void vsid_advance(void)
{
	if (vsid.ctrl & SID_CTRL_GATE)
	{
		if (vsid.phase < PHASE_ATTACK)
		{
			vsid.phase = PHASE_ATTACK;
			vsid.adsr = 0;
		}
	}
	else
	{
		if (vsid.phase >= PHASE_ATTACK)
			vsid.phase = PHASE_RELEASE;
	}

	switch (vsid.phase)
	{
	case PHASE_ATTACK:
		// Increase channel power during attack phase
		vsid.adsr += AttackStep[vsid.attdec >> 4];
		if (vsid.adsr >= AMAX)
		{
			// Switch to decay phase next
			vsid.adsr = AMAX;
			vsid.phase = PHASE_DECAY;
		}
		break;
	case PHASE_DECAY:
		{
			// Decrease channel power during decay phase
			unsigned sus = (AMAX >> 4) * (vsid.susrel >> 4);
			unsigned dec = DecayStep[vsid.attdec & 0x0f];

			if (vsid.adsr > sus + dec)
				vsid.adsr -= dec;
			else if (vsid.adsr > sus)
				vsid.adsr = sus;
		}
		break;
	case PHASE_RELEASE:
		{
			// Decrease channel power during release phase
			unsigned dec = DecayStep[vsid.susrel & 0x0f];

			if (vsid.adsr > dec)
				vsid.adsr -= dec;
			else
			{
				// Switch to off when energy reaches flat
				vsid.adsr = 0;
				vsid.phase = PHASE_OFF;
			}
		}
		break;
	}


}

void hires_bar(char * dp, const char * ady)
{
	char k = 0;
	for(char i=0; i<32; i++)
	{
		char c = 0;
		for(char j=0; j<8; j++)
		{
			if (i >= ady[j])
				c |= (128 >> j);
		}
		if (i & 1)
			c ^= 0x22;
		dp[k] = c;
		k++;
		if (k == 8)
		{
			dp += 320;
			k = 0;
		}
	}
}

static const char binlog32[256] = {
#for(i, 256)	(int)(log(i) / log(2) * 4),
};


void hires_draw_start(void)
{
	vsid.phase = PHASE_OFF;
	vsid.ctrl = 0;
	vsid.state = SIDFX_READY;
	vsid.delay = 1;
	vsid.tick = 0;
	vsid.pos = 0;
}

void hires_draw_tick(void)
{
	char ady[8], fry[8];

	if (vsid.tick < 40)
	{
		for(char n=0; n<2; n++)
		{
			const SIDFX	*	com = effects + vsid.pos;
			vsid.delay--;
			if (vsid.delay)
			{
				if (com->dfreq)
					vsid.freq += com->dfreq;
				if (com->dpwm)
					vsid.pwm += com->dpwm;				
			}
			while (!vsid.delay)
			{
				switch (vsid.state)
				{
				case SIDFX_IDLE:
					vsid.delay = 1;
					break;
				case SIDFX_RESET_0:
					vsid.ctrl = 0;
					vsid.attdec = 0;
					vsid.susrel = 0;
					vsid.state = SIDFX_READY;
					vsid.delay = 1;
					break;
				case SIDFX_READY:
					if (vsid.pos < neffects)
					{
						vsid.freq = com->freq;
						vsid.pwm = com->pwm;
						vsid.attdec = com->attdec;
						vsid.susrel = com->susrel;
						vsid.ctrl = com->ctrl;

						if (com->ctrl & SID_CTRL_GATE)
						{
							vsid.delay = com->time1;
							vsid.state = SIDFX_PLAY;
						}
						else
						{
							vsid.delay = com->time0;
							vsid.state = SIDFX_WAIT;
						}
					}
					else
						vsid.state = SIDFX_IDLE;
					break;
				case SIDFX_PLAY:
					if (com->time0)
					{
						vsid.ctrl = com->ctrl & ~SID_CTRL_GATE;
						vsid.delay = com->time0 - 1;
						vsid.state = SIDFX_WAIT;
					}
					else
					{
						vsid.pos++;
						if (vsid.pos < neffects)
						{
							char sr = com->susrel & 0xf0;
							com++;
							if ((com->attdec & 0xef) == 0 && (com->ctrl & SID_CTRL_GATE) && (com->susrel & 0xf0) > sr)
								vsid.phase = PHASE_RELEASE;
							vsid.state = SIDFX_READY;
						}
						else
							vsid.state = SIDFX_RESET_0;
					}
					break;
				case SIDFX_WAIT:
					vsid.pos++;
					if (vsid.pos < neffects)
					{
						com++;
						if (com->ctrl & SID_CTRL_GATE)
							vsid.state = SIDFX_RESET_0;
						else
							vsid.state = SIDFX_READY;
					}
					else
						vsid.state = SIDFX_RESET_0;
					break;
				}
			}


			for(char i=0; i<4; i++)
			{
				char j = i + 4 * n;
				vsid_advance();
				ady[j] = 31 - (vsid.adsr >> 8);
				fry[j] = 31 - binlog32[vsid.freq >> 8];
			}
		}

		char * dp = Hires + 320 * 12 + 8 * vsid.tick;
		hires_bar(dp, ady);
		dp += 320 * 4;
		hires_bar(dp, fry);	
		vsid.tick++;
	}
}

int main(void)
{
	__asm {sei}

	cia_init();

	mmap_set(MMAP_CHAR_ROM);
	memcpy(Hires, Font, 0x0800);

	memset(Sprites, 0, 256);
	for(char i=0; i<9; i++)
		Sprites[0 * 64 + 3 * i] = 0xff;
	Sprites[1 * 64 + 3 * 8] = 0xff;
	for(char i=0; i<21; i++)
		Sprites[2 * 64 + 3 * i] = 0xf0;
	mmap_set(MMAP_NO_ROM);

	spr_init(Screen);

	vic_setmode(VICM_TEXT, Screen, Hires);

	sidfx_init();

	rirq_init_io();

	rirq_build(&rirq_isr, 2);
	rirq_write(&rirq_isr, 0, &vic.ctrl1, VIC_CTRL1_DEN | VIC_CTRL1_RSEL | 3);
	rirq_call(&rirq_isr, 1, isr);
	rirq_set(0, 250, &rirq_isr);

	rirq_build(&rirq_mark0, 1);
	rirq_write(&rirq_mark0, 0, &vic.color_back, VCOL_BLUE);

	rirq_build(&rirq_mark1, 1);
	rirq_write(&rirq_mark1, 0, &vic.color_back, VCOL_BLACK);

	rirq_build(&rirq_bitmap, 2);
	rirq_delay(&rirq_bitmap, 10);
	rirq_write(&rirq_bitmap, 1, &vic.ctrl1, VIC_CTRL1_DEN | VIC_CTRL1_RSEL | VIC_CTRL1_BMM | 3);
	rirq_set(3, 49 + 8 * 12, &rirq_bitmap);

	rirq_sort();
	rirq_start();

	sid.fmodevol = 15;

	vic.color_border = VCOL_BLACK;
	vic.color_back = VCOL_BLACK;

	memset(Screen, 0x10, 1000);

	effects[0] = basefx;
	neffects = 1;

	showfxs();		
	showmenu();
	hires_draw_start();

	memset(Hires + 12 * 320, 0, 13 * 320);
	memset(Screen + 12 * 40, 0x70, 160);
	memset(Screen + 16 * 40, 0xe0, 160);

	spr_set(0, true, 0, 0, 64, VCOL_WHITE, false, false, false);
	spr_set(1, true, 0, 0, 66, VCOL_BLUE, false, false, true);
	spr_set(2, true, 0, 0, 66, VCOL_BLUE, false, false, true);

	vic.spr_priority = 0x07;

	bool	markset = false;
	for(;;)
	{
		char * curp = Screen + 40 + 40 * cursorY + cursorX;
		char * curc = Color + 40 + 40 * cursorY + cursorX;

		if (cursorY < 10 || cursorX >= 20)
		{
			spr_move(0, 24 + 8 * cursorX, 49 + 8 + 8 * cursorY);
			spr_image(0, 64 + ((csr_cnt >> 4) & 1));
		}
		else
		{
			spr_move(0, 0, 0);
			for(char i=0; i<4; i++)
				curc[i] = VCOL_YELLOW;
		}

		vic.color_border = VCOL_ORANGE;
		hires_draw_tick();
		vic.color_border = VCOL_BLACK;

		vic_waitBottom();
		if (sidfx_idle(0))
		{
			spr_move(1, 0, 0);
			spr_move(2, 0, 0);

			if (markset)
			{
				rirq_clear(1); rirq_clear(2);
				rirq_sort();
				markset = false;
			}
		}
		else
		{
			spr_move(1, 24 + 4 * irq_cnt, 12 * 8 + 49);
			spr_move(2, 24 + 4 * irq_cnt, 15 * 8 + 49);

			char sx = (neffects - sidfx_cnt(0)) * 8 + 49;
			if (!markset)
			{
				rirq_set(1, sx, &rirq_mark0); 
				rirq_set(2, sx + 8, &rirq_mark1); 
				markset = true;
			}
			else
			{
				rirq_move(1, sx); 
				rirq_move(2, sx + 8); 
			}
			rirq_sort();
		}

		if (cursorY < 10 || cursorX >= 20)
			;
		else
		{
			for(char i=0; i<4; i++)
				curc[i] = VCOL_LT_BLUE;
		}

		if (keyb_queue & KSCAN_QUAL_DOWN)
		{
			csr_cnt = 16;

			char k = keyb_queue & KSCAN_QUAL_MASK;
			keyb_queue = 0;

			if (cursorY < 10)
			{
				edit_effects(k);
				if (cursorY == 10)
				{
					if (cursorX < 20)
						cursorX = cursorX / 5 * 5;
					else
					{
						while (menup[cursorX - 1] == '.')
							cursorX--;
					}
				}
			}
			else
			{
				edit_menu(k);
			}
		}
	}
}