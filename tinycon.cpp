#include "tinycon.h"

/*
 * Terminal Console
 * Usage: tinyConsole tc("prompt>");
 *        tc.run();
 * 
 * When a command is entered, it will be passed to the trigger method for
 * processing. This is a blocking function.
 *
 * tinyConsole::trigger can be over-ridden in a derivative class to
 * change its functionality.
 *
 * Additionally, a check to the 'hotkeys' method is called for each
 * keypress, which can also be over-ridden in a dirivative class.
 *
 */

// On Unix-like systems, add getch support without curses
#if !defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)
int getch ()
{
	struct termios oldt, newt;
	int ch;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	
	ch = getchar();	
	if(ch == ESC)
	{
		ch = getchar();
		if(ch == 91)
		{
			ch = KEY_CTRL1;
		}
	}
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return ch;
}
#endif

tinyConsole::tinyConsole ()
{
	_max_history = MAX_HISTORY;
	_quit = false;
	pos = -1;
	line_pos = 0;
	skip_out = 0;
}

tinyConsole::tinyConsole (std::string s)
{
	_max_history = MAX_HISTORY;
	_quit = false;
	_prompt = s;
	pos = -1;
	line_pos = 0;
	skip_out = 0;
}

std::string tinyConsole::version ()
{
	return TINYCON_VERSION;
}

void tinyConsole::pause ()
{
	getch();
}

void tinyConsole::quit ()
{
	_quit = true;
}

int tinyConsole::trigger (std::string cmd)
{
	if (cmd == "exit") {
		_quit = true;
	} else {
		std::cout << cmd << std::endl;
	}
	return 0;
}

int tinyConsole::hotkeys (char c)
{
	return 0;
}

void tinyConsole::setMaxHistory (int max)
{
	_max_history = max;
}

std::string tinyConsole::getLine ()
{
	return getLine(M_LINE, "");
}

std::string tinyConsole::getLine (int mode)
{
	return getLine(mode, "");
}

std::string tinyConsole::getLine (int mode = M_LINE, std::string delimeter = "")
{
	std::string line;
	char c;

	for (;;)
	{
		c = getch();
		if ( c == NEWLINE)
		{
			std::cout << std::endl;
			return line;
		} else if ((int) c == BACKSPACE ) {
			if (line.length())
			{
				line = line.substr(0,line.size()-1);
				if (mode != M_PASSWORD)
				{
					std::cout << "\b \b";
				}
			}
		} else {
			line += c;
			if (mode != M_PASSWORD)
			{
				std::cout << c;
			}
		}
	}
}

std::string tinyConsole::getBuffer ()
{
	return std::string(buffer.begin(),buffer.end());
}

void tinyConsole::setBuffer (std::string s)
{
	buffer.assign(s.begin(),s.end());
	line_pos = buffer.size();
}

void tinyConsole::run ()
{
	//show prompt
	std::cout << _prompt;

	// grab input
	for (;;)
	{
		c = getch();
		if(!hotkeys(c))
		switch (c)
		{
			case ESC:
				// TODO escape is only detected if double-pressed. this should be fixed
				std::cout << "(Esc)";
				break;
			case KEY_CTRL1: // look for arrow keys
				switch (c = getch())
				{
					case UP_ARROW:
						if (!history.size()) break;
						if (pos == -1)
						{
							// store current command
							unused = "";
							unused.assign(buffer.begin(), buffer.end());
						}

						// clear line
						for (int i = 0; i < line_pos; i++)
						{
							std::cout << "\b \b";
						}

						// clean buffer
						buffer.erase(buffer.begin(), buffer.end());

						pos++;
						if (pos > (history.size() - 1)) pos = history.size() - 1;

						// store in buffer
						for (int i = 0; i < history[pos].size(); i++)
						{
							buffer.push_back(history[pos][i]);
						}
						line_pos = buffer.size();
						// output to screen
						std::cout << history[pos];
						break;
					case DOWN_ARROW:
						if (!history.size()) break;

						// clear line
						for (int i = 0; i < line_pos; i++)
						{
							std::cout << "\b \b";
						}

						// clean buffer
						buffer.erase(buffer.begin(), buffer.end());

						pos--;
						if (pos<-1) pos = -1;
						if (pos >= 0) {
							std::cout << history[pos];
							// store in buffer
							for (int i = 0; i < history[pos].size(); i++)
							{
								buffer.push_back(history[pos][i]);
							}
						} else {
							if (buffer.size())
							{
								std::cout << unused;
								// store in buffer
								for (int i = 0; i < unused.size(); i++)
								{
									buffer.push_back(unused[i]);
								}
							}
						}
						line_pos = buffer.size();
						break;
					case LEFT_ARROW:
						// if there are characters to move left over, do so
						if (line_pos)
						{
							std::cout << "\b";
							line_pos--;
						}
						break;
					case RIGHT_ARROW:
						// if there are characters to move right over, do so
						if (line_pos < buffer.size())
						{
							std::cout << buffer[line_pos];
							line_pos++;
						}
						break;
					case DEL:
						if (line_pos < buffer.size())
						{
							skip_out = 1;
							buffer.erase(buffer.begin()+line_pos);
							// update screen after current position
							for (int i = line_pos; i < buffer.size(); i++ )
							{
								std::cout << buffer[i];
							}
							// erase last char
							std::cout << " ";
							for (int i = line_pos; i < buffer.size(); i++ )
							{
								std::cout << "\b";
							}
							// make-up for erase position
							std::cout << "\b";
							//std::cout << "(DEL)";
						}
						break;
					default:
						skip_out = 1;
						//std::cout << "(" << (int) c << ")" << std::endl;
					}
				break;
			case BACKSPACE:
				if (line_pos == 0) break;
				// move cursor back, blank char, and move cursor back again
				std::cout << "\b \b";
				// don't forget to clean the buffer and update line position
				if (line_pos == buffer.size()) {
					buffer.pop_back();
					line_pos--;
				} else {
					line_pos--;
					buffer.erase(buffer.begin()+line_pos);
					// update screen after current position
					for (int i = line_pos; i < buffer.size(); i++ ) {
						std::cout << buffer[i];
					}
					// erase last char
					std::cout << " ";
					for (int i = line_pos+1; i < buffer.size(); i++ ) {
						std::cout << "\b";
					}
					// make-up for erase position and go to new position
					std::cout << "\b\b";
				}
				break;
			case TAB:
				break;
				// print history
				for (int i = 0; i < history.size(); i++) {
					std::cout << history[i] << std::endl;
				}
				break;
			case NEWLINE:
				// store in string
				s.assign(buffer.begin(), buffer.end());

				
				
				// save command to history
				// trimming of command should be done in callback function
				if(s.length()) 
					history.push_front(s);

				// run command
				//(*callbackFunc)(s.c_str());
				std::cout << std::endl;
				trigger(s);
				
				// check for exit command
				if(_quit == true) {
					return;
				}
				
				if (history.size() > _max_history) history.pop_back();

				// clean buffer
				buffer.erase(buffer.begin(), buffer.end());

				// print prompt. new line should be added from callback function
				std::cout << _prompt;

				// reset position
				pos = -1;
				line_pos = 0;
				break;
			default:
        if (skip_out) {
          skip_out = 0;
          break;
        }
				std::cout << c;
				if(line_pos == buffer.size()) {
					// line position is at the end of the buffer, just append
					buffer.push_back(c);
				} else {
					// line position is not at end. Insert new char
					buffer.insert(buffer.begin()+line_pos, c);
					// update screen after current position
					for (int i = line_pos+1; i < buffer.size(); i++ ) {
						std::cout << buffer[i];
					}
					for (int i = line_pos+1; i < buffer.size(); i++ ) {	
						std::cout << "\b";
					}
				}
				line_pos++;
				break;
		} // end switch
	}

}
