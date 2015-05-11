#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <iostream.h>
#include <fstream.h>

/*
Graphed / Graphics Editor, Biakov Kirill, 2012

It is my performance graphic editor for DOS graphics mode.
The main features:
1) saving and opening files
2) basic set of tools
3) ability to set the thickness of the brush
4) minimal color palette
*/

typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef struct {
	BYTE ascii;
	BYTE attr;
} SYMBOL;

// screen pointers (graphic & text regimes)
unsigned char far *ScrGraf = (unsigned char *) MK_FP(0xA000, 0);
SYMBOL far *ScrText = (SYMBOL far*) MK_FP(0xB800, 0);

// registers
union REGS regs;

// colors
int _BLACK = 16,
	_WHITE = 31,
	_GREEN_BR = 72,
	_GREEN_DK = 144,
	_GREY_1 = 122,
	_GREY_2 = 24,
	_GREY_3 = 26;

enum Tools { // tools
	Point, Line, Rect, Circle, Eraser, Fill
} Tool;	// selected tool

// toggle, his value
int Toggle, TogVal;

// selected color
char glColor;



// helpers
void SetMode(int);
void InvPixel(int, int);
void InvCursor(int, int);
int max(int, int);
int min(int, int);
void Pixel(int, int, char);
void FillRect(int, int, int, int, char);
void ClearScreen();

// main editor functions
void drPoint(int, int, int, char);
void drRect(int, int, int, int, int, char);
void drLine(int, int, int, int, char);
void drLine0(int, int, int, int, int, char);
void Ellipse(int, int, long int, long int, char);
void drCircle(int, int, int, int, int, char);
void FloodFill(int, int, char, char);

// drawing interface procedures
void DrawButtonNew(char);
void DrawButtonSave(char);
void DrawButtonOpen(char);
void DrawButtonQuit(char);
void DrawTopPanel();
void DrawPalette(char);
void DrawCurColor(char);
void DrawLowPanel();
void DrawPoint(char);
void DrawLine(char);
void DrawRect(char);
void DrawCircle(char);
void DrawEraser(char);
void DrawFill(char);
void DrawTools();
void DrawToggle(char);
void DrawSize();
void DrawToolsPanel();
void DrawCanvasPanel(char);

// procedures for save & open binary file
int SaveBMP(FILE *, char *);
int OpenBMP(FILE *, char *);

// text regime procedure
void TextRegime(char);



void main() {
	// default: tool - point, value - 1, color - _BLACK
	Tool = Point;
	Toggle = 7; TogVal = 1;
	glColor = _BLACK;

	int X, Y;
	int X0, Y0, a, b, a1, a2, b1, b2, radius;
	int changeTool = 0, changeTogVal = 0, Quit, Cancel;
	FILE *bgn, *tmp;

	// graphic regime 320x200-256 
	SetMode(0x13);

	// clear screen & draw interface
	ClearScreen();
	DrawTopPanel();
	DrawLowPanel();
	DrawCurColor(glColor);
	DrawToolsPanel();
	DrawCanvasPanel(_WHITE);
	
	// set cursor to center
	InvCursor(X = 160, Y = 100);

	Quit = 0;
	while (!Quit) {
		// if tools or size was changed then redraw this part
		if (changeTool) {
			changeTool = 0;
			InvCursor(X, Y);
			DrawTools();
			InvCursor(X, Y);
		} else if (changeTogVal) {
			changeTogVal = 0;
			InvCursor(X, Y);
			FillRect(6, 96, 31, 102, _GREY_3);
			InvCursor(X, Y);
			DrawToggle(_GREEN_DK);
		}

		// capture cursor position
		regs.x.ax = 0x000B;
		int86 (0x33, &regs, &regs);
		if (regs.x.cx || regs.x.dx) {
			InvCursor(X, Y);
			X = max(min(X + (int)regs.x.cx / 2, 312), 0);
			Y = max(min(Y + (int)regs.x.dx / 2, 192), 0);
			InvCursor(X, Y);
			
			// if the cursor in the area of button then select
			DrawButtonNew( 	X >= 65  && X <= 83  && Y >= 1 && Y <= 9 ? _GREEN_DK : _GREEN_BR);
			DrawButtonSave(	X >= 100 && X <= 118 && Y >= 1 && Y <= 9 ? _GREEN_DK : _GREEN_BR);
			DrawButtonOpen(	X >= 135 && X <= 154 && Y >= 1 && Y <= 9 ? _GREEN_DK : _GREEN_BR);
			DrawButtonQuit(	X >= 309 && X <= 317 && Y >= 1 && Y <= 9 ? _GREEN_DK : _GREEN_BR);
			
			DrawPoint(  X >= 6  && X <= 15 && Y >= 30 && Y <= 39 || Tool == Point  ? _GREEN_DK : _GREEN_BR);
			DrawLine(   X >= 6  && X <= 15 && Y >= 47 && Y <= 56 || Tool == Line   ? _GREEN_DK : _GREEN_BR);
			DrawRect(   X >= 18 && X <= 27 && Y >= 47 && Y <= 56 || Tool == Rect   ? _GREEN_DK : _GREEN_BR);
			DrawCircle( X >= 30 && X <= 39 && Y >= 47 && Y <= 56 || Tool == Circle ? _GREEN_DK : _GREEN_BR);
			DrawEraser( X >= 6  && X <= 15 && Y >= 64 && Y <= 73 || Tool == Eraser ? _GREEN_DK : _GREEN_BR);
			DrawFill(   X >= 18 && X <= 27 && Y >= 64 && Y <= 73 || Tool == Fill   ? _GREEN_DK : _GREEN_BR);
			DrawToggle( X >= 6  && X <= 31 && Y >= 96 && Y <= 102 ? _GREEN_DK : _GREEN_BR);
			
			drRect(65, 190, 183, 198, 1, X >= 65 && X <= 183 && Y >= 190 && Y <= 198 ? _GREEN_DK : _GREEN_BR);
		}

		regs.x.ax = 0x0003;
		int86(0x33, &regs, &regs);
		
		// if left mouse button clicked & cursor in the are of a button
		if (regs.x.bx == 0x0001) {
			if (X >= 65 && X <= 83 && Y >= 1 && Y <= 9) {				// New
				DrawCanvasPanel(_WHITE);
			} else if (X >= 100 && X <= 118 && Y >= 1 && Y <= 9) {			// Save
				InvCursor(X, Y);
				TextRegime('s');
				InvCursor(X, Y);
			} else if (X >= 135 && X <= 154 && Y >= 1 && Y <= 9) {			// Open
				TextRegime('o');
				DrawTopPanel();
				DrawLowPanel();
				DrawCurColor(glColor);
				DrawToolsPanel();
				InvCursor(X, Y);
			} else if (X >= 309 && X <= 317 && Y >= 1 && Y <= 9) {			// Quit
				Quit = 1;
			} else if ((X >= 6  && X <= 15 && Y >= 30 && Y <= 39) ||		// tool selection
					 (X >= 6  && X <= 15 && Y >= 47 && Y <= 56) ||
					 (X >= 18 && X <= 27 && Y >= 47 && Y <= 56) ||
					 (X >= 30 && X <= 39 && Y >= 47 && Y <= 56) ||
					 (X >= 6  && X <= 15 && Y >= 64 && Y <= 73) ||
					 (X >= 18 && X <= 27 && Y >= 64 && Y <= 73)) {
					 
				changeTool = 1;
				
				if (X >= 6 && X <= 15 && Y >= 30 && Y <= 39) {			// Point
					Tool = Point;
				} else if (X >= 6 && X <= 15 && Y >= 47 && Y <= 56)	{	// Line
					Tool = Line;
				} else if (X >= 18 && X <= 27 && Y >= 47 && Y <= 56) {		// Rectangle
					Tool = Rect;
				} else if (X >= 30 && X <= 39 && Y >= 47 && Y <= 56) {		// Circle
					Tool = Circle;
				} else if (X >= 6 && X <= 15 && Y >= 64 && Y <= 73) {		// Eraser
					Tool = Eraser;
				} else if (X >= 18 && X <= 27 && Y >= 64 && Y <= 73) {		// Fill
					Tool = Fill;
				}
			} else if (X >= 7 && X <= 30 && Y >= 96 && Y <= 102) {			// Toggle
				changeTogVal = 1;
				Toggle = X;
				TogVal = (X - 1)/3 - 1;
			} else if (X >= 66 && X <= 182 && Y >= 191 && Y <= 197) {		// Palette
				glColor = X/3-6;
				DrawCurColor(glColor);
			} else if (X >= 47 && X <= 316 && Y >= 15 && Y <= 184) {		// clicked in canvas area
				// save cursor position & save file
				InvCursor(X, Y);
				
				if (Tool == Line || Tool == Rect || Tool == Circle) {
					SaveBMP(bgn, "_bgn");
					SaveBMP(tmp, "_tmp");
				}
				
				X0 = X; Y0 = Y;

				Cancel = 0;
				while (!Cancel) {
					// if move then recalculate coordinates
					regs.x.ax = 0x000B;
					int86(0x33, &regs, &regs);
					if (regs.x.cx || regs.x.dx) {
							X0 = max(min(X0 + (int) regs.x.cx / 2, 312), 0);
							Y0 = max(min(Y0 + (int) regs.x.dx / 2, 192), 0);
						if (X0 < 47 || X0 > 316 || Y0 < 15 || Y0 > 184) {
							if (X0 >= 47 && X0 <= 316) {
								X0 = max(min(X0 + (int) regs.x.cx / 2, 312), 0);
								Y0 = (Y0 < 15 ? 15 : 184);
							} else if (Y0 >= 15 && Y0 <= 184) {
								Y0 = max(min(Y0 + (int) regs.x.dx / 2, 192), 0);
								X0 = (X0 < 47 ? 47 : 316);
							} else {
								X0 = (X0 < 47 ? 47 : 316);
								Y0 = (Y0 < 15 ? 15 : 184);
							}
						}

						// perform actions (depends from tool)
						if (Tool == Line || Tool == Rect || Tool == Circle) {
							OpenBMP(bgn, "_bgn");

							if (Tool == Line) {
								if (X0 >= 47 && X0 <= 316 && Y0 >= 15 && Y0 <= (184 - TogVal + 1)) {
									drLine0(X, Y, X0, Y0, TogVal, glColor);
								} else {
									if (X0 >= 47 && X0 <= 316) {
										Y0 = Y0 < 15 ? 15 : 184 - TogVal + 1;
									} else if (Y0 >= 15 && Y0 <= (184 - TogVal + 1)) {
										X0 = X0 < 47 ? 47 : 316;
									} else {
										X0 = X0 < 47 ? 47 : 316;
										Y0 = Y0 < 15 ? 15 : 184 - TogVal + 1;
									}
									drLine0(X, Y, X0, Y0, TogVal, glColor);
								}
							} else if (Tool == Rect || Tool == Circle) {
								a1 = min(X, X0); a2 = max(X, X0);
								b1 = min(Y, Y0); b2 = max(Y, Y0);
								
								if (X0 >= 47 && X0 <= 316 && Y0 >= 15 && Y0 <= 184) {
									if (Tool == Rect) {
										drRect(a1, b1, a2, b2, TogVal, glColor);
									} else if (Tool == Circle) {
										a = (a2 - a1) / 2;
										b = (b2 - b1) / 2;
										radius = min(a, b);
										drCircle(a1 + a, b1 + b, a, b, TogVal, glColor);
									}
								} else {
									if (X0 >= 47 && X0 <= 316) {
										Y0 = Y0 < 15 ? 15 : 184;
									} else if (Y0 >= 15 && Y0 <= 184) {
										X0 = X0 < 47 ? 47 : 316;
									} else {
										X0 = X0 < 47 ? 47 : 316;
										Y0 = Y0 < 15 ? 15 : 184;
									}
									
									if (Tool == Rect) {
										drRect(a1, b1, a2, b2, TogVal, glColor);
									} else if (Tool == Circle) {
										a = (a2 - a1) / 2;
										b = (b2 - b1) / 2;
										radius = min(a, b);
										drCircle(a1 + a, b1 + b, a, b,TogVal, glColor);
									}
								}
							}

							SaveBMP(tmp, "_tmp");
						} else if (Tool == Fill) {
							// paint over the entire area of the selected color in accordance with what color in (X, Y)
							FloodFill(X, Y, peekb(0xA000, Y0 * 320 + X0), glColor);
						}
					}
					
					if (Tool == Point || Tool == Eraser) {
						char Temp = Tool == Point ? glColor : _WHITE;
						if (X0 >= 47 && X0 <= (316 - TogVal + 1) && Y0 >= 15 && Y0 <= (184 - TogVal + 1)) {
							drPoint(X0, Y0, TogVal, Temp);
						} else {
							if (X0 >= 47 && X0 <= (316 - TogVal + 1)) {
								Y0 = Y0 < 15 ? 15 : 184 - TogVal + 1;
							} else if (Y0 >= 15 && Y0 <= (184 - TogVal + 1)) {
								X0 = X0 < 47 ? 47 : 316 - TogVal + 1;
							} else {
								X0 = X0 < 47 ? 47 : 316 - TogVal + 1;
								Y0 = Y0 < 15 ? 15 : 184 - TogVal + 1;
							}
							drPoint(X0, Y0, TogVal, Temp);
						}
					}
					
					// if release LMB or press RMB, then cancel of drawing a line, rectangle or circle
					regs.x.ax = 0x0003;
					int86(0x33, &regs, &regs);
					Cancel = (regs.x.bx == 0x0002 && (Tool == Line || Tool == Circle)) ? 2 : !regs.x.bx;
				}

				// if not cancel then open file with last with last position or begin file
				if (Tool == Line || Tool == Rect || Tool == Circle) {
					switch (Cancel) {
						case 1:
							OpenBMP(tmp, "_tmp");
							break;
						case 2:
							OpenBMP(bgn, "_bgn");
							break;
					}
					
					remove("_bgn");
					remove("_tmp");
				}
				
				X = X0; Y = Y0;
				InvCursor(X, Y);
			}

			// cleare mouse position
			while (regs.x.bx == 0x0001) {
				regs.x.ax = 0x0003;
				int86(0x33, &regs, &regs);
			}
		} // end if
	} // end while
} // end main



void SetMode(int Mode) { 
	regs.h.ah = 0; 
	regs.h.al = Mode; 
	int86(0x10, &regs, &regs); 
}

void InvPixel(int x, int y) {
	pokeb(0xA000, 320 * y + x, 0x04 ^ peekb(0xA000, 320 * y + x));
}

void InvCursor(int x, int y) {
	BYTE Pointer[] = {
		0xFE, 0xE0, 0xF0, 0xB8, 0x9C, 0x8E, 0x87, 0x03
	};
	
	BYTE b;
	for (int j = 0; j < 8; j++) {
		b = Pointer[j];
		for (int i = 0; i < 8; i++) {
			if (b & 0x80) InvPixel (x + i, y + j);
			b <<= 1;
		}
	}
}

int max(int x, int y) {
	return x > y ? x : y;
}

int min(int x, int y) {
	return x < y ? x : y;
}

void Pixel(int x, int y, char Color) { 
	ScrGraf[320 * y + x] = Color;
}

void FillRect(int x1, int y1, int x4, int y4, char Color) {
	for (int x = x1; x <= x4; x++) {
		for (int y = y1; y <= y4; y++) {
			Pixel(x, y, Color);
		}
	}
}

void ClearScreen() { 
	FillRect(0, 0, 319, 199, _GREY_2);
}

void drPoint(int x1, int y1, int Size, char Color) {
	FillRect(x1, y1, x1 + Size - 1, y1 + Size - 1, Color);
}

void drRect(int x1, int y1, int x4, int y4, int Size, char Color) {
	int x, y;
	for (int i = 0; i < Size; i++) {
		for (x = x1 + i; x <= x4 - i; x++) {
			Pixel(x, y1 + i, Color);
			Pixel(x, y4 - i, Color);
		}
		
		for (y = y1; y <= y4; y++) {
			Pixel(x1 + i, y, Color);
			Pixel(x4 - i, y, Color);
		}
	}
}

void drLine(int x1, int y1, int x2, int y2, char Color) {
	int deltaX = abs(x2 - x1);
	int deltaY = abs(y2 - y1);
	int signX = x1 < x2 ? 1 : -1;
	int signY = y1 < y2 ? 1 : -1;
	int error = deltaX - deltaY;
	
	Pixel(x2, y2, Color);
	
	while (x1 != x2 || y1 != y2) {
		Pixel(x1, y1, Color);
		int error2 = error * 2;
		
		if (error2 >- deltaY) {
			error -= deltaY;
			x1 += signX;
		}
		
		if (error2 < deltaX) {
			error += deltaX;
			y1 += signY;
		}
	}
}

void drLine0(int x1, int y1, int x2, int y2, int Size, char Color) {
	for (int i = 0; i < Size; i++)
		drLine(x1, y1 + i, x2, y2 + i, Color);
}

void Ellipse(int CX,int CY, long int XRadius,long int YRadius, char glColor) {
	int locX, locY;
	long int XChange, YChange;
	long int EllipseError;
	long int TwoASquare, TwoBSquare;
	long int StoppingX, StoppingY;
	TwoASquare = 2 * XRadius * XRadius;
	TwoBSquare = 2 * YRadius * YRadius;
	locX = XRadius;
	locY = 0;
	XChange = YRadius * YRadius * (1 - 2 * XRadius);
	YChange = XRadius * XRadius;
	EllipseError = 0;
	StoppingX = TwoBSquare * XRadius;
	StoppingY = 0;
	
	while (StoppingX > StoppingY) {
		Pixel(CX + locX, CY + locY, glColor);
		Pixel(CX - locX, CY + locY, glColor);
		Pixel(CX - locX, CY - locY, glColor);
		Pixel(CX + locX, CY - locY, glColor);
		locY++;
		StoppingY += TwoASquare;
		EllipseError += YChange;
		YChange += TwoASquare;
		
		if (2 * EllipseError+XChange > 0) {
			 locX--;
			 StoppingX -= TwoBSquare;
			 EllipseError += XChange;
			 XChange += TwoBSquare;
		}
	}
	
	locX = 0;
	locY = YRadius;
	XChange = YRadius * YRadius;
	YChange = XRadius * XRadius * (1 - 2 * YRadius);
	EllipseError = 0;
	StoppingX = 0;
	StoppingY = TwoASquare * YRadius;
	
	while (StoppingX < StoppingY) {
		Pixel(CX + locX, CY + locY, glColor);
		Pixel(CX - locX, CY + locY, glColor);
		Pixel(CX - locX, CY - locY, glColor);
		Pixel(CX + locX, CY - locY, glColor);
		locX++;
		StoppingX += TwoBSquare;
		EllipseError += XChange;
		XChange += TwoBSquare;
		
		if (2 * EllipseError + YChange > 0) {
			locY--;
			StoppingY -= TwoASquare;
			EllipseError += YChange;
			YChange += TwoASquare;
		}
	}
}

void drCircle(int x0, int y0, int radius1, int radius2, int Size, char Color) {
	for (Size = Size - 1; Size >= 0; Size--)
		Ellipse(x0, y0, radius1 - Size, radius2 - Size, Color);
}

void FloodFill(int x, int y, char Temp, char Color) {
	// X >= 47 && X <= 316 && Y >= 15 && Y <= 184
	int xL, xR, YY, i;
	int xMax = 316, yMax = 184;
	xL = xR = x;
	
	while (ScrGraf[320 * y + xL] != Temp && xL >= 47) {
		Pixel(xL, y, Color);
		xL--;
	}
	xR++;
	
	while (ScrGraf[320 * y + xR] != Temp && xR <= 316) {
		Pixel(xR, y, Color);
		xR++;
	}
	x = (xR + xL) >> 1;
	
	for (i = -1; i <= 1; i += 2) {
		YY = y;
		while (ScrGraf[320 * YY + x] != Temp && YY >= 15 && YY <= 184) YY += i;
		YY = (y + YY) >> 1;
		
		if ((ScrGraf[320 * YY + x] != Temp) && (ScrGraf[320 * YY + x] != Color)) {
			FloodFill(x, YY, Temp, Color);
		}
	}
	
	for (YY = y - 1; YY <= y + 1; YY += 2) {
		x = xL + 1;
		while (x < xR && YY >= 15 && YY <= 184) {
			if ((ScrGraf[320 * YY + x] != Temp) && (ScrGraf[YY * 320 + x] != Color)) {
				FloodFill(x, YY, Temp, Color);
			}
			x++;
		}
	}
}



void DrawButtonNew(char Color) {
	int x, y;
	drRect(65, 1, 83, 9, 1, Color);

	for (y = 3; y < 8; y++) {
		Pixel(67, y, Color);
		Pixel(70, y, Color);
	}
	Pixel(68, 4, Color);
	Pixel(69, 5, Color);

	for (y = 3; y < 8; y++) {
		Pixel(72, y, Color);
	}
	for (x = 73; x < 76; x++) {
		for (y = 3; y < 8; y += 2) {
			Pixel(x, y, Color);
		}
	}

	for (y = 3; y < 7; y++) {
		Pixel(77, y, Color);
		Pixel(81, y, Color);
	}
	Pixel(78, 7, Color);
	Pixel(79, 6, Color);
	Pixel(80, 7, Color);
}

void DrawButtonSave(char Color) {
	int x, y;
	drRect(100, 1, 118, 9, 1, Color);

	for (x = 102; x < 105; x++) {
		for (y = 3; y < 8; y += 2) {
			Pixel(x, y, Color);
		}
	}
	Pixel(102, 4, Color);
	Pixel(104, 6, Color);

	for (y = 4; y < 8; y++) {
		Pixel(106, y, Color);
		Pixel(108, y, Color);
	}
	Pixel(107, 3, Color);
	Pixel(107, 5, Color);

	for (y = 3; y < 7; y++) {
		Pixel(110, y, Color);
		Pixel(112, y, Color);
	}
	Pixel(111, 7, Color);

	for (y = 3; y < 8; y++) {
		Pixel(114, y, Color);
	}
	for (y = 3; y < 8; y += 2) {
		Pixel(115, y, Color);
		Pixel(116, y, Color);
	}
}

void DrawButtonOpen(char Color) {
	int x, y;
	drRect(135, 1, 154, 9, 1, Color);

	for (y = 3; y < 8; y++) {
		Pixel(137, y, Color);
		Pixel(139, y, Color);
	}
	Pixel(138, 3, Color);
	Pixel(138, 7, Color);

	for (y = 3; y < 8; y++) {
		Pixel(141, y, Color);
		if (y < 6) {
			Pixel(143, y, Color);
		}
	}
	Pixel(142, 3, Color);
	Pixel(142, 5, Color);

	for (y = 3; y < 8; y++) {
		Pixel(145, y, Color);
	}
	for (x = 146; x < 148; x++) {
		for (y=3; y<8; y+=2) {
			Pixel(x, y, Color);
		}
	}

	for (y = 3; y < 8; y++) {
		Pixel(149, y, Color);
		Pixel(152, y, Color);
	}
	Pixel(150, 4, Color);
	Pixel(151, 5, Color);
}

void DrawButtonQuit(char Color) {
	drRect(309, 1, 317, 9, 1, Color);

	for (int y = 3; y < 8; y++) {
		Pixel(311 + y - 3, y, Color);
		Pixel(315 + 3 - y, y, Color);
	}
}

void DrawTopPanel() {
	int x, y;

	for (x = 0; x < 320; x++) {
		Pixel(x, 11, _GREEN_BR);
	}
	FillRect(0, 0, 319, 10, _GREY_3);

	for (y = 2; y < 9; y++) {
		Pixel(2, y, _GREEN_BR); // _WHITE color?
	}
	for (x = 3; x < 6; x++) {
		Pixel(x, 2, _GREEN_BR);
		Pixel(x, 5, _GREEN_BR);
	}

	for (y = 2; y < 9; y++) {
		Pixel(7, y, _GREEN_BR);
	}
	
	for (y = 2; y < 9; y++) {
		Pixel(9, y, _GREEN_BR);
	}
	for (x = 10; x < 13; x++) {
		Pixel(x, 8, _GREEN_BR);
	}
	
	for (y = 2; y < 9; y++) {
		Pixel(14, y, _GREEN_BR);
	}
	for (x = 15; x < 18; x++) {
		Pixel(x, 2, _GREEN_BR);
		Pixel(x, 5, _GREEN_BR);
		Pixel(x, 8, _GREEN_BR);
	}

	DrawButtonNew(_GREEN_BR);
	DrawButtonSave(_GREEN_BR);
	DrawButtonOpen(_GREEN_BR);
	DrawButtonQuit(_GREEN_BR);
}

void DrawPalette(char Color) {
	int x, y, clr = 15;
	drRect(65, 190, 183, 198, 1, Color);

	// color palette: b/w + bright colors (16-55), clr - shift color
	for (x = 66; x < 183; x++) {
		if (x % 3 == 0) clr++;
		
		for (y = 191; y < 198; y++) {
			Pixel(x, y, clr);
		}
	}
}

void DrawCurColor(char Color) {
	FillRect(204, 191, 210, 197, Color);
}

void DrawLowPanel() {
	int x, y;

	for (x = 0; x < 320; x++) {
		Pixel(x, 188, _GREEN_BR);
	}
	FillRect(0, 189, 319, 199, _GREY_3);

	for (y = 191; y < 198; y++) {
		Pixel(2, y, _GREEN_BR);
	}
	for (x = 3; x < 5; x++) {
		Pixel(x, 191, _GREEN_BR);
		Pixel(x, 194, _GREEN_BR);
	}
	for (y = 191; y < 195; y++) {
		Pixel(5, y, _GREEN_BR);
	}

	for (y = 192; y < 198; y++) {
		Pixel(7, y, _GREEN_BR);
		Pixel(10, y, _GREEN_BR);
	}
	for (x = 8; x < 10; x++) {
		Pixel(x, 191, _GREEN_BR);
		Pixel(x, 194, _GREEN_BR);
	}

	for (y = 191; y < 198; y++) {
		Pixel(12, y, _GREEN_BR);
	}
	for (x=13; x<16; x++) {
		Pixel(x, 197, _GREEN_BR);
	}

	for (y = 191; y < 198; y++) {
		Pixel(17, y, _GREEN_BR);
	}
	for (x = 18; x < 21; x++) {
		Pixel(x, 191, _GREEN_BR);
		Pixel(x, 194, _GREEN_BR);
		Pixel(x, 197, _GREEN_BR);
	}

	for (x = 22; x < 25; x++) {
		Pixel(x, 191, _GREEN_BR);
	}
	for (y = 192; y < 198; y++) {
		Pixel(23, y, _GREEN_BR);
	}

	for (x = 26; x < 29; x++) {
		Pixel(x, 191, _GREEN_BR);
	}
	for (y = 192; y < 198; y++) {
		Pixel(27, y, _GREEN_BR);
	}

	for (y = 191; y < 198; y++) {
		Pixel(30, y, _GREEN_BR);
	}
	for (x = 31; x < 34; x++) {
		Pixel(x, 191, _GREEN_BR);
		Pixel(x, 194, _GREEN_BR);
		Pixel(x, 197, _GREEN_BR);
	}

	DrawPalette(_GREEN_BR);

	for (x = 188; x < 199; x++) {
		Pixel(x, 194, _GREEN_BR);
	}
	
	drRect(203, 190, 211, 198, 1, _GREEN_BR);
}

void DrawPoint(char Color) {
	drRect(6, 30, 15, 39, 1, Color);
	
	Pixel(10, 34, Color);
	Pixel(11, 34, Color);
	Pixel(10, 35, Color);
	Pixel(11, 35, Color);
}

void DrawLine(char Color) {
	drRect(6, 47, 15, 56, 1, Color);
	
	for (int x = 8; x < 14; x++) {
		Pixel(x, 54 + 8 - x, Color);
	}
}

void DrawRect(char Color) {
	drRect(18, 47, 27, 56, 1, Color);
	drRect(21, 50, 24, 53, 1, Color);
}

void DrawCircle(char Color) {
	int x;
	drRect(30, 47, 39, 56, 1, Color);
	
	for (x = 32; x < 35; x++) {
		Pixel(x, 51+ 32 - x, Color);
		Pixel(x, 52+ x - 32, Color);
	}
	
	for (x = 35; x < 38; x++) {
		Pixel(x, 49 + x - 35, Color);
		Pixel(x, 54 + 35 - x, Color);
	}
}

void DrawEraser(char Color) {
	int x, y;
	drRect(6, 64, 15, 73, 1, Color);
	
	for (x = 10; x < 12; x++) {
		Pixel(x, 66, Color);
		Pixel(x, 68, Color);
		Pixel(x, 71, Color);
	}
	
	for (y = 66; y < 72; y++) {
		Pixel(9, y, Color);
		Pixel(12, y, Color);
	}
}

void DrawFill(char Color) {
	drRect(18, 64, 27, 73, 1, Color);
	
	for (int x = 20; x < 24; x++) {
		for (int i = 0; i < 3; i++) {
			Pixel(x + i, 68 + x - 20 - i, Color);
		}
	}
	
	for (int y = 69; y < 72; y++) {
		Pixel(20, y, Color);
	}
	
	Pixel(22, 69, Color);
	Pixel(23, 68, Color);
	Pixel(23, 70, Color);
	Pixel(24, 69, Color);
}

void DrawTools() {
	DrawPoint(_GREEN_BR);
	DrawLine(_GREEN_BR);
	DrawRect(_GREEN_BR);
	DrawCircle(_GREEN_BR);
	DrawEraser(_GREEN_BR);
	DrawFill(_GREEN_BR);

	switch (Tool) {
		case Point:
			DrawPoint(_GREEN_DK);
			break;
		case Line:
			DrawLine(_GREEN_DK);
			break;
		case Rect:
			DrawRect(_GREEN_DK);
			break;
		case Circle:
			DrawCircle(_GREEN_DK);
			break;
		case Eraser:
			DrawEraser(_GREEN_DK);
			break;
		case Fill:
			DrawFill(_GREEN_DK);
			break;
	}
}

void DrawToggle(char Color) {
	int x, y;

	for (y = 96; y < 103; y++) {
		Pixel(6, y, Color);
		Pixel(31, y, Color);
	}
	for (x = 7; x < 31; x++) {
		Pixel(x, 99, Color);
	}

	for (y = 97; y < 102; y++) {
		Pixel(Toggle, y, Color);
	}
}

void DrawSize() {
	DrawToggle(_GREEN_BR);
	int x, y;

	for (y = 98; y < 103; y++) {
		Pixel(33, y, _GREEN_BR);
	}
	
	for (y = 98; y < 101; y++) {
		Pixel(35, y, _GREEN_BR);
	}
	
	Pixel(34, 98, _GREEN_BR);
	Pixel(34, 100, _GREEN_BR);

	for (x = 37; x < 40; x++) {
		Pixel(x, 100 + 37 - x, _GREEN_BR);
	}
	
	Pixel(37, 98, _GREEN_BR);
	Pixel(39, 100, _GREEN_BR);
}

void DrawToolsPanel() {
	int x, y;
	drRect(2, 14, 43, 185, 1, _GREEN_BR);
	FillRect(3, 15, 42, 184, _GREY_3);

	for (x = 13; x < 16; x++) {
		Pixel(x, 16, _GREEN_BR);
	}
	
	for (y = 17; y < 23; y++) {
		Pixel(14, y, _GREEN_BR);
	}

	for (x = 17; x < 20; x++) {
		Pixel(x, 18, _GREEN_BR);
		Pixel(x, 22, _GREEN_BR);
	}
	for (y = 19; y < 22; y++) {
		Pixel(17, y, _GREEN_BR);
		Pixel(19, y, _GREEN_BR);
	}

	for (x = 21; x < 24; x++) {
		Pixel(x, 18, _GREEN_BR);
		Pixel(x, 22, _GREEN_BR);
	}
	for (y = 19; y < 22; y++) {
		Pixel(21, y, _GREEN_BR);
		Pixel(23, y, _GREEN_BR);
	}

	for (y = 18; y < 23; y++) {
		Pixel(25, y, _GREEN_BR);
	}
	Pixel(26, 22, _GREEN_BR);
	Pixel(27, 22, _GREEN_BR);

	for (x = 29; x < 32; x++) {
		for (y = 18; y < 23; y+=2) {
			Pixel(x, y, _GREEN_BR);
		}
	}
	Pixel(29, 19, _GREEN_BR);
	Pixel(31, 21, _GREEN_BR);

	DrawTools();

	for (x = 3; x < 43; x++){
		Pixel(x, 81, _GREEN_BR);
	}

	for (x = 16; x < 20; x++) {
		for (y = 83; y < 90; y += 3) {
			Pixel(x, y, _GREEN_BR);
		}
	}
	Pixel(16, 84, _GREEN_BR);
	Pixel(16, 85, _GREEN_BR);
	Pixel(19, 87, _GREEN_BR);
	Pixel(19, 88, _GREEN_BR);

	for (y = 85; y < 90; y++) {
		Pixel(21, y, _GREEN_BR);
	}
	
	for (x = 23; x < 26; x++) {
		Pixel(x, 85, _GREEN_BR);
		Pixel(x, 89, _GREEN_BR);
		Pixel(x, 88 + 23 - x, _GREEN_BR);
	}

	for (y = 85; y < 90; y++) {
		Pixel(27, y, _GREEN_BR);
	}
	for (y = 85; y < 90; y += 2) {
		Pixel(28, y, _GREEN_BR);
		Pixel(29, y, _GREEN_BR);
	}

	DrawSize();
}

void DrawCanvasPanel(char Color) {
	drRect(46, 14, 317, 185, 1, _GREEN_BR);
	FillRect(47, 15, 316, 184, Color);
}

int SaveBMP(FILE *fp, char *fname) {
	if ((fp = fopen(fname, "wb")) == NULL) {
		return 0;
	} else {
		for (long int i = 0; i < 64000; i++) {
			fputc(peekb(0xA000, i), fp);
		}
		fclose(fp);
		return 1;
	}
}

int OpenBMP(FILE *fp, char *fname) {
	int x, y;
	if ((fp = fopen(fname, "rb")) == NULL) {
		return 0;
	} else {
		for (unsigned long int i = 0; i < 64000; i++) {
			pokeb(0xA000, i, fgetc(fp));
		}
		fclose(fp);
		return 1;
	}
}

void TextRegime(char f) {
	FILE *bmp, *tmp, *fp;
	SaveBMP(bmp, "_bmp");

	SetMode(0x3);
	_setcursortype(_NOCURSOR);
	clrscr();

	char ch, *fname = "\0";
	char Str1[]  = " Graphed / Graphics Editor, 2012 ",
		 Str2[]  = " Faculty of MM&CS SFU, Biakov Kirill ",
		 StrQ[]  = " Put the filename: ",
		 Str25[] = " Press any key to continue... ";
	char *Str3, *StrA;
	Str3 = (f=='s') ? " File saving " : " File opening ";

	int i, j;

	for (j = 0; j < 80; j++) {
		for (i = 0; i < 25; i++) {
			int pos = i * 80 + j;
			if (i < 3) {
				ScrText[pos].attr = 0x7A;
			} else if (i < 24) {
				ScrText[pos].attr = 0xA;
			} else {
				ScrText[pos].attr = 0xFA;
			}
		}
		if (j < strlen(Str1)) ScrText[j].ascii = Str1[j];
		if (j < strlen(Str2)) ScrText[80+j].ascii = Str2[j];
		if (j < strlen(Str3)) ScrText[2*80+j].ascii = Str3[j];
	}

	for (j = 0; j < 80; j++) {
		if (j < strlen(StrQ)) {
			ScrText[4 * 80 + j].ascii = StrQ[j];
		}
	}

	j = 0;
	int End = 0;
	while (!End) {
		if (kbhit()) {
			ch = getch();
			if (((ch & 0xFF) == 13) || ((ch & 0xFF) == 27))	{
				End = 1;
				
				if ((ch & 0xFF) == 13) { // Enter
					if (f == 's') {
						if (rename("_bmp", fname) == 0) {
							StrA = " Saving succesfully. ";
						} else {
							End = 2;
							StrA = " Unexpected error! ";
						}
					} else if (f == 'o') {
						StrA = " Opening succesfully. ";
					}
				} else if ((ch & 0xFF) == 27) { // Escape
					StrA = f == 's' ? " Saving aborted by the user! " : " Opening aborted by the user! ";
				}
			} else if (((ch & 0xFF) == 8) && (j!=0)) { // Backspace
				ScrText[5*80+1+(--j)].ascii = ' ';
				fname[j] = '\0';
			} else if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') && j < 8) {
				ScrText[5 * 80 + 1 + j].ascii = ch;
				fname[j++] = ch;
				fname[j] = '\0';
			}
		}
	}

	for (j = 0; j < 80; j++) {
		if (j < strlen(StrA)) {
			ScrText[6 * 80 + j].ascii = StrA[j];
		}
		if (j < strlen(Str25)) {
			ScrText[24 * 80 + j].ascii = Str25[j];
		}
	}
	getch();

	SetMode(0x13);

	if (f == 's') {
		if (End != 2) {
			OpenBMP(bmp, fname);
		} else {
			OpenBMP(bmp, "_bmp");
			remove("_bmp");
		}
	} else if (f == 'o') {
		if (OpenBMP(bmp, fname));
		else OpenBMP(bmp, "_bmp");
		remove("_bmp");
	}
}
