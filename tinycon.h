#include <deque>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <string>
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <conio.h>
#include <Windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

// Keyboard Scan Codes
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#define KEY_CTRL1 -32
#define BACKSPACE 8
#define UP_ARROW 72
#define DOWN_ARROW 80
#define RIGHT_ARROW 77
#define LEFT_ARROW 75
#else
#define KEY_CTRL1 17
#define BACKSPACE 127
#define UP_ARROW 65
#define DOWN_ARROW 66
#define RIGHT_ARROW 67
#define LEFT_ARROW 68
#endif
#define TAB 9
#define ESC 27
#define DEL 51
#define CTRLC 3

// Some basic configs
#define TINYCON_VERSION "0.6"
#define MAX_HISTORY 500

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
const char NEWLINE = '\r';
#else
const char NEWLINE = '\n';
int getch();
#endif

// getLine modes
#define M_LINE 0
#define M_PASSWORD 1

std::string tinyGetLine();

std::string tinyGetPassword();


class tinyConsole {
protected:
	bool _quit;
	int _max_history;
	std::string _prompt;

	int pos;
	int line_pos;
	int skip_out;
	char c;
	std::string s, unused;
	std::deque<char> buffer;
	std::deque<std::string> history;
public:
	tinyConsole();
	tinyConsole(std::string);
	void run();
	void setPrompt(std::string);
	virtual int trigger(std::string);
	virtual int hotkeys(char);
	void pause();
	void quit();
	std::string getBuffer();
	std::string getLine();
	std::string getLine(int);
	std::string getLine(int, std::string);
	std::string version();
	void setBuffer(std::string);
	void setMaxHistory(int);
};
