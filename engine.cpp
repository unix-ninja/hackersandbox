/*
  description: unix hacker simulator
  written by Unix-Ninja
  original code february, 2010
*/

#include "engine.h"
#include "md5.h"
#include "tinycon.h"
#include <ctime>
#include <iomanip>
#include <fstream>
#include <typeinfo>
#if !defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)
#include <libgen.h>
#endif

bool debug_on;
vector<VM> vPC;                        // available vMachines
vector<VM*> shell;                     // current shell chain
vector<RT> vhashes;                    // list of users' password hashes
int cEC = 0;                           // command exit code
string mission_file;                   // filename for mission pack
char last_key;                         // last hotkey pressed
std::map<std::string, std::string> dns_map; // global DNS records
std::map<int, std::string> netd_map;   // map of Network Domain IDs/Names
boost::thread *cmd_thread;             // reference to running cmd thread

lua_State *lua;                        // Lua state object

int lua_vdump(vector<string> args);
int lua_restore(vector<string> args);
int lua_save(vector<string> args);

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
void sleep(int t)
{
     Sleep(t*1000);
}
#endif

// setup console
class tcon : public tinyConsole
{
public:
  tcon (std::string s): tinyConsole(s) {;}

  int trigger (std::string s);

  int hotkeys(char c)
  {
    if (c == TAB)
    {
      // save buffer length
      int length;
      length = buffer.size();

      // convert buffer to string
      string s;
      s.assign(buffer.begin(),buffer.end());

      // explode string buffer
      vector<string> cmd;
      explode(s, " ", cmd);
      if (!cmd.size())
      {
        cmd.push_back(s);
      }

      // try tab complete on last segment of commandline
      vector<string> matches;
      matches = shell.back()->tabComplete(cmd.back());

      // return if no matches
      if (!matches.size())
      {
        if (cmd.size() == 1)
        {
          matches = shell.back()->tabComplete("/bin/" + cmd.back());
          if (!matches.size()) return 1;
          for (int i=0; i<matches.size(); i++)
          {
            matches[i] = matches[i].substr(5);
          }
        } else {
          return 1;
        }
      }

      // clear commandline if matched
      for (int i=0; i<length; i++)
      {
        cout << "\b \b";
      }

      // reconstruct commandline
      if (cmd.size() > 1)
      {
        s = ""; // reset s before reconstruction
        for (int i=0; i<cmd.size()-1; i++)
        {
          if (s.length() > 0) s += " ";
          s += cmd[i];
        }
        s += " ";
      } else {
        s = "";
      }

      // process matches
      string buf;

      if (matches.size() == 1)
      {
        // do we need to replace the ./ ?
        if (pcrecpp::RE("^\\./[^\\/]+").PartialMatch(cmd.back()))
        {
          buf =  s + "./" + matches[0];
        }
        else
        {
          buf = s + matches[0];
        }
        if (buf.substr(buf.length()-1) != "/") buf += " ";

        // set new buffer
        setBuffer(buf);
        length = buffer.size();

        // copy buffer to screen
        std::cout << buf;
      } else {
        // on multiple matches, find the common chars
        // we will loop through the set and match against
        // the first index
        length = 0;
        int l = 0;
        for (int i=1; i<matches.size(); i++)
        {

          for (int pos=0; pos<matches[0].length(); pos++)
          {
            if (matches[0][pos] == matches[i][pos])
            {
              l++;
            } else {
              break;
            }
          }
          if (length)
          {
            length = l > length ? length : l;
          } else {
            length = l;
          }
        }
        buf = s + matches[0].substr(0,length);
        setBuffer(buf);
        cout << buf;
        if (last_key == TAB) {
          cout << endl;
          for (int i=0; i< matches.size(); i++)
          {
            cout << "  " << matches[i] << endl;
          }
          cout << _prompt << buf;
        }
      }
      last_key = TAB;
      return 1;
    } else if (c == CTRLC) {
      raise(SIGINT);
      return 1;
    }
    last_key = c;
    return E_OK;
  }
};

tcon tc (std::string("$ "));

// explode the string into words, each in an index of a vector
int explode(std::string line,std::string delimiter,std::vector<std::string> &store)
{
  store.clear();

  // if line is empty, return fail
  if (line.empty()) return -1;

  //unsigned int start = 0;
  signed int start = -1;
  int len = 0;
  int i = 0;
  std::vector<std::string> temp;

  len = delimiter.length();

  // if no instance of delimiter exist, return a single index
  if ( line.find(delimiter) == std::string::npos )
  {
    store.push_back(line);
    return 1;
  }

  while ((start = line.find(delimiter)) != std::string::npos) // move along finding delimiter, inc start
  {
    store.push_back( line.substr( 0, start) );
    line.erase( 0, start+len );
    i++;
  }
  if (!line.empty()) store.push_back(line);
  i++;
  return i;
}

int explode(std::string line, std::string delimiter, std::vector<std::string> &store, int limit)
{
  store.clear();

  // if line is empty, return fail
  if (line.empty()) return -1;

  //unsigned int start = 0;
  signed int start = -1;
  int len = 0;
  int i = 0;
  std::vector<std::string> temp;

  len = delimiter.length();

  // if no instance of delimiter exist, return a single index
  if ( line.find(delimiter) == std::string::npos )
  {
    store.push_back(line);
    return 1;
  }

  while ((start = line.find(delimiter)) != std::string::npos) // move along finding delimiter, inc start
  {
    store.push_back( line.substr( 0, start) );
    line.erase( 0, start+len );
    i++;
    if (i == limit) break;
  }
  if (!line.empty()) store.push_back(line);
  i++;
  return i;
}

/*
void replace_all(std::string& str, const std::string& from, const std::string& to)
{
  if (from.empty()) return;
  size_t pos = 0;
  while ((pos = str.find(from, pos)) != std::string::npos)
  {
    str.replace(pos, from.length(), to);
    pos += to.length();
  }
}
*/

string realpath(string path)
{
  if (path == "/") return path;
  if (path.substr(0,1) != "/")
  {
    // if relative path, append cwd
    path = shell.back()->getCwd() + path;
  }

  // sanitize path
  vector<string> store, realp;
  explode(path, "/", store);

  for (int i=1; i<store.size(); i++)
  {
    if (!store.empty())
    {
      if (store[i] == "..")
      {
        if (realp.size()) realp.pop_back();
      }
      else if (store[i] != ".")
      {
        realp.push_back(store[i]);
      }
    }
  }

  // reconstruct real path
  path = "";
  for (int i=0; i<realp.size(); i++)
  {
    path += "/" + realp[i];
  }
  if (path.empty()) path = "/";

  // get pointer to file
  File *fp = shell.back()->pFile(path);

  // if no file or directory found, return nothing
  if (!fp) return "";

  if (fp->getType() == T_FOLDER)
  {
    // remember to add trialing slash
    if (path.length()>1 && path.substr(path.length()-1) != "/")
      path += "/";
  }
  return path;
}

void addDNS(string name, string record)
{
  dns_map.insert(pair<string, string> (name, record));
}

string getDNS(string name)
{
  if (name.empty()) return "";
  // if we are given an IP, just return it
  if (pcrecpp::RE("[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}").FullMatch(name)) return name;
  // return the record from the DNS map
  return dns_map[name];
}

int registerNetDomain(int domain_id)
{
  std::map<int, string>::iterator it;

  if ((it=netd_map.find(domain_id)) == netd_map.end())
  {
    netd_map[domain_id] = "";
  }
  return domain_id;
}

int registerNetDomain(string domain_name)
{
  int domain_id = 0;
  std::map<int, string>::iterator it;

  // do we need to set the domain?
  domain_id = 1;
  for (it=netd_map.begin(); it!=netd_map.end(); it++)
  {
    if (it->second == domain_name) break; else domain_id++;
  }
  if (it==netd_map.end()) netd_map[domain_id] = domain_name;
  return domain_id;
}

int getNetDomainId(string domain_name)
{
  int domain_id = 1;
  std::map<int, string>::iterator it;
  for (it=netd_map.begin(); it!=netd_map.end(); it++)
  {
    if (it->second == domain_name) return domain_id; else domain_id++;
  }
  return E_OK;
}

string execFile(string f)
{
  File *fp = shell.back()->pFile(f);
  if (fp)
  {
    if (fp->isExecutable())
    {
      return fp->getExec();
    } else {
      return "!";
    }
  }
  return "";
}

bool chainVM(string remote, int port, string user = "")
{
  string svc_name;
  string password;
  string host = getDNS(remote);

  if (remote == "0.0.0.0")
  {
    cout << "no route to host!" << endl;
    return false;
  }

  for (int i=0; i < vPC.size(); i++)
  {
    if (vPC[i].getIp() == host && vPC[i].getIp() != "0.0.0.0" && shell.back()->getNetRoute(host))
    {
      vector<Daemon*> svc = vPC[i].getDaemons();
      for (int ii=0; ii<svc.size(); ii++)
      {
        svc_name = svc[ii]->name;
        if ( svc_name == "telnet" || svc_name == "telnetd" || svc_name == "ssh" || svc_name == "sshd")
        {
          if (port == svc[ii]->port)
          {
            // get usrname
            if (user.empty())
            {
              cout << "Login:";
              user = tc.getLine();
            }
            // get password
            cout << "Password:";
            password = tc.getLine(M_PASSWORD);
            // try login
            shell.push_back(&(vPC[i]));
            if (vPC[i].login(user, password))
            {
              // add VM to chain

              // make broken user
              shell.back()->startBrokenCounter();
              cout << "connected." << endl;
              return true;
            } else {
              shell.pop_back();
              cout << "invalid username or password." << endl;
              return false;
            }
          } else {
            sleep(3);
            cout << "connection timed out." << endl;
            return false;
          }
        }
      }
    }
  }
  cout << remote << ": nodename nor servname provided, or not known." << endl;
  return false;
}

VM* getVM(string hostname)
{
  for (int i=0; i < vPC.size(); i++)
  {
    if (vPC[i].getHostname() == hostname) return &vPC[i];
  }
  return NULL;
}

std::string ab64_encode(char const* bytes_to_encode, unsigned int in_len)
{
  // slight base64 varient
  static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+.";
  std::string ret;
  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];

  while (in_len--) {
    char_array_3[i++] = *(bytes_to_encode++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for (i = 0; (i <4) ; i++)
        ret += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i)
  {
    for (j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; (j < i + 1); j++)
      ret += base64_chars[char_array_4[j]];

    while ((i++ < 3))
      ret += '=';

  }
  return ret;
}

static inline bool is_ab64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '.'));
}

std::string ab64_decode(std::string const& encoded_string)
{
  static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+.";
  int in_len = encoded_string.size();
  int i = 0;
  int j = 0;
  int in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::string ret;

  while (in_len-- && ( encoded_string[in_] != '=') && is_ab64(encoded_string[in_]))
  {
    char_array_4[i++] = encoded_string[in_]; in_++;
    if (i ==4)
    {
      for (i = 0; i <4; i++)
        char_array_4[i] = base64_chars.find(char_array_4[i]);
      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
      for (i = 0; (i < 3); i++)
        ret += char_array_3[i];
      i = 0;
    }
  }

  if (i)
  {
    for (j = i; j <4; j++)
      char_array_4[j] = 0;

    for (j = 0; j <4; j++)
      char_array_4[j] = base64_chars.find(char_array_4[j]);

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
  }
  return ret;
}

string vhash(string input)
{
  string hash;
  char t[] = { 'x', 'a', 'r', 'b', 'p', 'w', 'm', 'l', 'f', 'j', 'v', 'd', 'q', 'k', 'z', 'h' };
  int len = 15;

  for (int i=0; i<input.length(); i++)
  {
    for (int n=0; n<len; n++)
    {
      if (n%4 == 0)
      {
        t[n] = t[n] | input.at(i);
      }
      else if ((n>1) && (i%2 == 0))
      {
        t[n] = t[n] & t[n-1];
      } else {
        t[n] = t[n] ^ input.at(i);
      }
    }
  }
  for (int n=0; n<len; n++) hash += t[n];
  hash = ab64_encode(hash.c_str(), len);
  return hash;
}

int can_access(string s)
{
  // are we allowed to see these files?
  vector<string> store;
  explode(s, "/", store);
  string path = "/";
  File *fp;

  for (int i=0; i<store.size(); i++)
  {
    if (store[i].empty()) continue;
    path += store[i];

    fp = shell.back()->pFile(path);
    if (!fp)
    {
      // file doesn't exist!
      return E_NO_FILE;
    }
    if (!fp->isReadable())
    {
      // no permission to access
      return E_DENIED;
    }
    if (fp->getType() == T_FOLDER)
    {
      path += "/";
    }
  }
  return E_OK;
}

// *********** built-in commands ************** //

int cmd_cat(vector<string> args)
{
  string f;
  int ret =0;
  File *fp;
  for (int i=1; i<args.size(); i++)
  {
    if (args[i].substr(0,1)=="-") continue;
    f = realpath(args[i]);
    fp = shell.back()->pFile(f);
    if (!fp) {
      cout << "cat: " << args[i] << ": No such file or directory." << endl;
      ret = 1;
    } else {
      if (can_access(f) == E_OK)
      {
        cout << fp->getContent();
      } else {
        cout << "-sh: cat: " << args[i] << ": Permission denied." << endl;
        ret = 1;
      }
    }
  }
  return ret;
}

int cmd_cd(vector<string> args)
{
  string path;

  if (args.size() < 2)
  {
    // if no args, set to home
    path = "/home/";
    return 1;
  } else {
    path = args[1];
    path = realpath(path);
  }

  // try to change dir
  int status = shell.back()->setCwd(path);
  switch (status)
  {
    case E_NO_FILE:
      cout << "-sh: cd: " << args[1] << ": No such directory." << endl;
      return 1;
      break;
    case E_DENIED:
      cout << "-sh: cd: " << args[1] << ": Permission denied." << endl;
      return 1;
      break;
  }
  // dir changed
  return E_OK;
}

int cmd_clear()
{
  // we're going to cheat here. this should probably be re-made properly at some point
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
  system("cls");
#else
  system("clear");
#endif
  return E_OK;
}

int cmd_date(vector<string> &args)
{
  // current date/time based on current system
  time_t now = time(0);
  if (args.size() > 1 && args[1] == "+%s")
  {
    cout << now << endl;
  } else {
    // convert now to string form
    char* dt = ctime(&now);
    cout << dt;
  }
  return E_OK;
}

int cmd_echo(vector<string> &args)
{
  args.erase(args.begin());
  if (args.size() == 0)
  {
    cout << endl;
    return E_OK;
  }
  for (int i=0; i<args.size(); i++)
  {
    if (args[i].at(0)=='$')
    {
      string var = args[i].substr(1,args[i].length()-1);
      if (var == "PATH")
      {
        cout << "/bin/:./";
      }
      else if (var == "SHELL")
      {
        cout << "/bin/sh";
      }
      else if (var == "?")
      {
        cout << cEC;
      }
    }
    else
    {
      cout << args[i] << " ";
    }
  }
  cout << endl;
  return E_OK;
}

int cmd_exit()
{
  // logout and check for a shell chain break
  if (shell.back()->logout() < 1 )
  {
    // leave VM
    if (shell.size() > 1)
    {
      shell.pop_back();
      cout << "connection closed." << endl;
    } else {
      // if no VMs in shell chain, ask for init login again
      for (;;)
      {
        string user;
        string password;

        cout << endl;
        if (shell.back()->getUserCount()>0) break;
        cout << "Login: ";
        user = tc.getLine();
        cout << "Password: ";
        password = tc.getLine(M_PASSWORD);
        if (shell.back()->login(user, password))
        {
          break;
        }
      }
    }
  } else {
    cout << "logout." << endl;
  }
  return E_OK;
}

int cmd_quit()
{
  cout << "Are you sure you want to quit the game? (y/N) ";
  string quit_game = tc.getLine();
  if (quit_game == "y" || quit_game == "Y")
  {
    cout << "[Thanks for playing! Come back soon!]";
    tc.quit();
  }
  return E_OK;
}

int cmd_help()
{
  cout << "Currently supported built-in commands:\n  cat\n  cd\n  clear\n  date\n  echo\n  exit\n  help\n  hint\n  hostname\n  ifconfig\n  load\n  ls\n  lspci\n  mail\n  ping\n  pwd\n  quit\n  rm\n  save\n  su\n  telnet\n  uname\n  whoami" << endl;
  return E_OK;
}

int cmd_hint()
{
  if (!shell.back()->getHint().empty())
  {
    cout << shell.back()->getHint() << endl;
  } else {
    cout << "Sorry, no hints are available for this VM." << endl;
  }
  return E_OK;
}

int cmd_hashcat(vector<string> args)
{
  // interruptable threads set error exits at the head
  cEC = 1;

  string plain;
  cout << "Initializing hashcat-micro (for virtual PCs)..." << endl;
  if (args.size() != 2 || args[1] == "-h")
  {
    cout << "usage: " << args[0] << " hash" << endl;
    cout << "    Please note: hashcat-micro does not support hashfiles at this time." << endl;
    return cEC;
  }
  cout << "Added 1 hash" << endl << "Activating quick-digest mode for single-hash" << endl;
  cout << endl << "Running." << flush;
  // find plain in rainbow table
  for (int i=0; i<vhashes.size(); i++)
  {
    if (vhashes[i].hash == args[1])
    {
      plain = vhashes[i].plain;
      break;
    }
  }
  // let's use the compute functions to simulate crack time
  int cracktime = plain.length();
  if (cracktime)
  {
    cracktime /= 4;
    if (pcrecpp::RE("[0-9]").PartialMatch(plain)) { cracktime *= 2; }
    if (pcrecpp::RE("[a-z]").PartialMatch(plain)) { cracktime *= 4; }
    if (pcrecpp::RE("[A-Z]").PartialMatch(plain)) { cracktime *= 4; }
    if (pcrecpp::RE("[^0-9a-zA-Z]").PartialMatch(plain)) { cracktime *= 2; }
    cracktime = cracktime / (int) sqrt((double) shell.back()->getCPower());
  } else {
    cracktime = 32;
  }

  for (int i=0; i<cracktime; i++)
  {
    cout << "." << flush;
    boost::this_thread::sleep(boost::posix_time::seconds(1));
  }
  cout << endl << endl;

  if (!plain.empty())
  {
    cout << args[1] << ":" << plain << endl;
    cout << "All hashes have been recovered" << endl;
    cEC = 0;
    return E_OK;
  }
  cout << "Index: 1/1 (segment)" << endl;
  cout << "Progress: 100%" << endl;
  cout << "Recovered: 0/1 hashes" << endl;
  return cEC;
}

int cmd_hostname()
{
  cout << shell.back()->getHostname() << endl;
  return E_OK;
}

int cmd_ifconfig()
{
  if (shell.back()->getIp() == "0.0.0.0") return 1;
  cout << "en0   UP BROADCAST SMART SIMPLEX MULTICAST   MTU 1500 Metric 0" << endl;
  cout << "      inet addr " << shell.back()->getIp() << endl;
  cout << "      media autoselect" << endl;
  return E_OK;
}

int cmd_ls(vector<string> args)
{
  string cmd;
  string n (shell.back()->getCwd()); // set name to cwd
  string op;
  string owners;
  bool longlist = false;
  File *fp, *fp2;

  // parse arguments. first non option is target
  for (int i=1; i<args.size(); i ++)
  {
    op = args[i].substr(0,1);
    if (op != "-")
    {
      cmd = args[i];
      n = realpath(cmd);
      break;
    }
    else if (args[i] == "-l")
    {
      longlist = true;
    }
  }

  if (n.empty())
  {
    cout << "ls: " << cmd << ": No such file or directory." << endl;
    return 1;
  }

  // are we allowed to see these files?
  vector<string> store;
  explode(n, "/", store);
  string path = "/";

  for (int i=0; i<store.size(); i++)
  {
    if (store[i].empty()) continue;
    fp = shell.back()->pFile(path + store[i]);
    if (!fp)
    {
      cout << "ls: " << cmd << ": No such file or directory." << endl;
      return 1;
    }
    if (fp->getType() == T_FOLDER)
    {
      path += store[i] + "/";
    }

    fp2 = shell.back()->pFile(path);
    if (!fp->isReadable() || !fp2->isReadable())
    {
      cout << "ls: " << cmd << ": permission denied." << endl;
      return 1;
    }
  }

  // get pointer to target
  fp = shell.back()->pFile(n);
  // get list of files
  vector<string> files = shell.back()->getFiles(n);

  // sort files
  sort(files.begin(), files.end());
  // loop through files
  for (int i=0; i < files.size(); i++)
  {
    if (longlist)
    {
      fp = shell.back()->pFile(path + files[i]);
      if (fp->getType() == T_FOLDER)
      {
        files[i] = files[i].substr(0, files[i].length()-1);
      }
      owners = "  " + fp->getOwner() + " " + fp->getOwner();
      cout << fp->drawAcl() << left << setw(20) << owners << files[i] << endl;
    } else {
      cout << files[i] << endl;
    }
  }

  return E_OK;
}

int cmd_lspci()
{
  cout << "00:00.0 Host bridge: 82443BX/ZX" << endl;
  cout << "00:0f.0 VGA compatible controller (rev 01)" << endl;
  cout << "00:10.0 Storage controller: AB Logic / Fusion-X UltraStorage (rev 01)" << endl;
  cout << "00:20.0 Ethernet controller: FastNet Gigabit Ethernet Controller (rev 01)" << endl;
  vector<string> gpus = shell.back()->getGPUs();
  for (int i=0; i<gpus.size(); i++)
  {
    cout << setw(2) << setfill('0') << i+1;;
    cout << ":00.0 " << gpus.at(i) << endl;;
  }
  return E_OK;
}

int cmd_mail()
{
  User *up = shell.back()->getUser(shell.back()->getUsername());
  if ( up->inboxSize() )
  {
    cout << "Mail version 3.2 6/6/91.  Type h for help." << endl;
    string cmd = "f";
    string buffer;
    vector<string> args;
    Mail *m;
    while (cmd != "q" && cmd != "quit")
    {
      if (cmd == "f") {
        for (int i=0; i<up->inboxSize(); i++)
        {
          m = up->getMail(i+1);
          cout << " N  " << setw(3) << i+1 << "  " << m->from() << "   " << m->subject() << endl;
        }
      }
      else if (cmd == "h" || cmd == "help")
      {
        cout << "   Mail Commands\nh                     help\nt <message list>      type messages\nf <message list>      give head lines of messages\nq                     quit\n\nA <message list> consists of integers, separated by spaces.  If omitted, Mail uses the last message typed." << endl;
      }
      else if (cmd == "t")
      {
        if (args.size()>1)
        {
          for (int i=1; i< args.size(); i++)
          {
            m = up->getMail(atoi(args[i].c_str()));
            if (m)
            {
              cout << "Message " << args[i] << ":" << endl;
              cout << "From: " << m->from() << endl;
              cout << "To: " << m->to() << endl;
              cout << "Subject: " << m->subject() << endl;
              cout << endl << m->body() << endl;
            }
          }
        }
      }
      cout << "? ";
      buffer = tc.getLine();
      explode(buffer, " ", args);
      cmd = !args.empty() ? args[0] : buffer;
      if (args.empty() && pcrecpp::RE("[0-9]*").FullMatch(cmd))
      {
        args.push_back("t");
        args.push_back(cmd);
        cmd = "t";
      }
    }
  } else {
    cout << "No mail for " << up->getName() << "." << endl;
  }
  return E_OK;
}

/*
int cmd_nmap(vector<string> args)
{
  time_t now = time(0);
  // convert now to string form
  char* dt = ctime(&now);
  // banner
  cout << "Starting Nmap-micro 1.01 ( for virtual PCs ) at " << dt << endl;
  return E_OK;
}
*/

int cmd_ping(vector<string> args)
{
  // interruptable threads set error exits at the head
  cEC = 1;

  if (args.size() < 2)
  {
    cout << "usage: " << args[0] << " hostname" << endl;
    return cEC;
  }
  string host = args[1];

  // start info line
  cout << "Sending 5, 100-byte ICMP Echos to " << host;

  // loop through vPCs
  for (int i=0; i<vPC.size(); i++)
  {
    if (vPC[i].getHostname() == host)
    {
      cout << " (" << vPC[i].getIp() << ")";
    }
    host = getDNS(host);
    if (vPC[i].getIp() == host && shell.back()->getNetRoute(host))
    {
      // close info line
      cout << ":" << endl;
      cout << "!!!!!" << endl << "Success rate is 100 percent (5/5)" << endl;
      cEC = E_OK;
      return E_OK;
    }
  }

  // close info line
  cout << ":" << endl;

  // timeout output
  for (int i=0; i<5; i++)
  {
    cout << "." << flush;
    boost::this_thread::sleep(boost::posix_time::seconds(1));
  }
  cout << endl << "Success rate is 0 percent (0/5)" << endl;
  return cEC;
}

int cmd_portscan(vector<string> args)
{
  if ((args.size() != 2 && args.size() != 3) || args[1] == "-h")
  {
    cout << "usage: " << args[0] << " [options] hostname" << endl;
    cout << "  -h    Display Help info" << endl;
    cout << "  -N    Perform scan of network to find all live hosts" << endl;
    return 1;
  }
  cout << "Stealth Port Scanner by o3u" << endl;

  if (args[1] == "-N")
  {
    cout << "Scanning network for live hosts..." << endl;
    cout << "--------------------" << endl;
    for (int i=0; i<vPC.size(); i++)
    {
      if (shell.back()->getNetRoute(vPC[i].getIp()))
      {
        sleep(2);
        cout << "HOST ALIVE: " << vPC[i].getIp() << endl;
      }
    }
    return E_OK;
  } else {
    string host = getDNS(args[1]);
    // loop through vPCs
    for (int i=0; i<vPC.size(); i++)
    {
      if (vPC[i].getIp() == host && shell.back()->getNetRoute(host))
      {
        cout << "Scanning " << host << "..." << endl;
        sleep(3);
        cout << "--------------------" << endl;
        vector<Daemon*> ports = vPC[i].getDaemons();
        for (int n=0; n<ports.size(); n++)
        {
          cout << left << setw(5) << ports[n]->port << ": CONNECT" <<endl;
          if (!ports[n]->query.empty())
          {
            cout << ports[n]->query << endl;
          }
          cout << "END PORT INFO" << endl << endl;
        }
        return E_OK;
      }
    }
    sleep(3);
    cout << "Unable to connect to remote host: Attempt to connect timed-out without" << endl <<
      "establishing a connection." << endl;
    return 1;
  }
}

int cmd_pwd(vector<string> args)
{
  cout << shell.back()->getCwd() << endl;
  return 1;
}

int cmd_rm(vector<string> args)
{
  vector<string> target;
  File *fp;
  string path;

  for (int i=1; i<args.size(); i++)
  {
    if (args[i].substr(0,1) != "-")
    {
      target.push_back(args[i]);
    }
  }

  if (target.size() == 0)
  {
    cout << "usage: rm target1 (target2) .. (targetN)" << endl;
    return 1;
  }

  for (int i=0; i<target.size(); i++)
  {
    path = realpath(target[i]);
    if (target[i].length())
    {
      fp = shell.back()->pFile(path);
      if (!fp)
      {
        cout << "rm: " << target[i] << ": No such file." << endl;
        return 1;
      }
      if (fp->getType() == T_FOLDER)
      {
        cout << "rm: " << target[i] << ": Unable to remove folders." << endl;
        return 1;
      }
      if (!fp->isWritable())
      {
        cout << "rm: " << target[i] << ": Permission denied." << endl;
        return 1;
      }
      shell.back()->deleteFile(path);
    };
  }
  return E_OK;
}

int cmd_su(vector<string> args)
{
  string user = "root";
  if (args.size()>1) {
    user = args.back();
  }
  if (shell.back()->getUsername() == "root")
  {
    if (shell.back()->login(user))
    {
      return E_OK;
    } else {
      cout << "su: Sorry." << endl;
      return 1;
    }
  }
  cout << "Password:";
  string password = tc.getLine(M_PASSWORD);
  if (shell.back()->login(user, password))
  {
    return E_OK;
  } else {
    cout << endl << "su: Sorry." << endl;
    return 1;
  }
}

int cmd_telnet(vector<string> args)
{
  if (args.size()<2 || args.size()>3)
  {
    cout << "usage: telnet host-name [port]" << endl;
    return 1;
  }
  string remote = args[1];
  int port = 23;
  if (args.size() == 3)
  {
    port = atoi(args[2].c_str());
  }

  if (chainVM (remote, port)) return E_OK;
  return 1;
}

int cmd_uname()
{
  cout << shell.back()->getUname() << endl;
  return E_OK;
}

int cmd_whoami()
{
  cout << shell.back()->getUsername() << endl;
  return E_OK;
}

// debug commands
void debug_globals()
{
  string key;
  int type;

  cout << "[DBG] Lua Globals:" << endl;
  // let's get our globals
  lua_pushglobaltable(lua);
  lua_pushnil(lua);
  while (lua_next(lua, -2) != 0)
  {
    key = lua_tostring(lua, -2);
    type = lua_type(lua, -1);

    // skip reserved keys
    if (!key.empty() && key.substr(0,1)=="_")
    {
      lua_pop(lua,1);
      continue;
    }

    if (type == LUA_TBOOLEAN)
    {
      bool b = lua_toboolean(lua, -1);
      cout << key << " = ";
      //_lua_data.append(key).append(" = ");
      if (b)
      {
        cout << "true";
        //_lua_data.append("true");
      } else {
        cout << "false";
        //_lua_data.append("false");
      }
      cout << endl;
      //_lua_data.append("\n");
    }
    else if (type == LUA_TNUMBER)
    {
      ostringstream num;
      num << lua_tonumber(lua, -1);
      //_lua_data.append(key).append(" = ").append(num.str());
      //_lua_data.append(lua_tonumber(lua, -1));
      cout << key << " = ";
      cout << lua_tonumber(lua, -1) << endl;
      //_lua_data.append("\n");
    }
    else if (type == LUA_TSTRING)
    {
      string s = lua_tostring(lua, -1);
      // let's sanitize s!
      pcrecpp::RE("\"").GlobalReplace("\\\\\"", &s);
      //_lua_data.append(key).append(" = \"").append(s).append("\"");;
      //_lua_data.append("\n");
      cout << key << "= \"" << s << "\"" << endl;
    }
    lua_pop(lua, 1);
  }
  lua_pop(lua,1);
}

// more funcs
void lua_runp()
{
          // call function
          lua_pcall(lua, 1, 0, 0);
          // TODO add threading here
}

// trigger for tinyConcole
int tcon::trigger(string cmd)
{
  bool run_debug = false;
  // initialize vector for command parsing
  vector<string> args;

  // clean multi-whitespace
  pcrecpp::RE("\\s{2,}").GlobalReplace(" ", &cmd);

  // parse args
  if (explode(cmd.c_str()," ",args) != string::npos)
  {
    // if multiple args, assign first to cmd
    cmd = args[0];
  } else {
    args.push_back(cmd);
  }

  // clean empty arg
  if (args.size() == 2 && args[1].empty())
  {
    args.pop_back();
  }

  if (cmd == "")
  {
    return E_OK;
  }

  if (debug_on)
  {
    // loop through debug commands first
    if (cmd == "globals")
    {
      debug_globals();
      run_debug = true;
    }
  }

  if (!run_debug)
  {
    // loop through commands
    if (cmd == "exit")
    {
      cEC = cmd_exit();
    }
    else if (cmd == "cat")
    {
      cEC = cmd_cat(args);
    }
    else if (cmd == "cd")
    {
      cEC = cmd_cd(args);
    }
    else if (cmd == "clear")
    {
      cEC = cmd_clear();
    }
    else if (cmd == "date")
    {
      cEC = cmd_date(args);
    }
    else if (cmd == "dump")
    {
      lua_vdump(args);
      cEC = E_OK;
    }
    else if (cmd == "echo")
    {
      cEC = cmd_echo(args);
    }
    else if (cmd == "help")
    {
      cEC = cmd_help();
    }
    else if (cmd == "hint")
    {
      cEC = cmd_hint();
    }
    else if (cmd == "hostname")
    {
      cEC = cmd_hostname();
    }
    else if (cmd == "ifconfig")
    {
      cEC = cmd_ifconfig();
    }
    else if (cmd == "load")
    {
      lua_restore(args);
      cEC = E_OK;
    }
    else if (cmd == "ls")
    {
      cEC = cmd_ls(args);
    }
    else if (cmd == "lspci")
    {
      cEC = cmd_lspci();
    }
    else if (cmd == "mail")
    {
      cEC = cmd_mail();
    }
    else if (cmd == "ping")
    {
      //cEC = cmd_ping(args);
      // let's run the ping command in a thread
      boost::thread t(cmd_ping, args);
      cmd_thread =& t;
      t.join();
    }
    else if (cmd == "pwd")
    {
      cEC = cmd_pwd(args);
    }
    else if (cmd == "quit")
    {
      cEC = cmd_quit();
    }
    else if (cmd == "rm")
    {
      cEC = cmd_rm(args);
    }
    else if (cmd == "save")
    {
      lua_save(args);
      cEC = E_OK;
    }
    else if (cmd == "su")
    {
      cEC = cmd_su(args);
    }
    else if (cmd == "telnet")
    {
      cEC = cmd_telnet(args);
    }
    else if (cmd == "uname")
    {
      cEC = cmd_uname();
    }
    else if (cmd == "whoami")
    {
      cEC = cmd_whoami();
    }
    else
    {
      string bin = execFile(realpath(cmd));
      if (bin.empty() && cmd.find("/") == -1)
      {
        bin = execFile(realpath("/bin/"+cmd));
      }
      if (!bin.empty())
      {
        if (bin == "!")
        {
          cout << "-sh: " << cmd << ": not executable." << endl;
        } else {
          if (bin == "p_hashcat")
          {
            boost::thread t(cmd_hashcat, args);
            cmd_thread =& t;
            t.join();
          }
          /*
          else if (bin == "p_nmap")
          {
            cEC = cmd_nmap(args);
          }
          */
          else if (bin == "p_portscan")
          {
            cEC = cmd_portscan(args);
          }
          else if (bin == "p_pwd")
          {
            cEC = cmd_pwd(args);
          } else {
            // push function to stack
            lua_getglobal(lua, bin.c_str());
            // create arg table
            lua_newtable(lua);
            // push cmd name
            lua_pushnumber(lua, 1);
            lua_pushstring(lua, realpath(args[0]).c_str());
            lua_settable(lua, -3);
            // push args
            for (int i=1; i<args.size(); i++)
            {
              lua_pushnumber(lua, i+1);
              lua_pushstring(lua, args[i].c_str());
              lua_settable(lua, -3);
            }
            // call function
            boost::thread t(lua_runp);
            cmd_thread =& t;
            t.join();
          }
        }
      } else {
        cout << "-sh: " << cmd << ": command not found." << endl;
      }
      cEC = 0;
    }
    cmd_thread = 0;
  }
  return E_OK;
}

VM* createNewVM(string hostname)
{
  vPC.push_back(VM(hostname));
  return &(vPC.back());
}

int addDir(string name, string acl = "", string owner = "", string host = "")
{
  File f (File(name, T_FOLDER, "", ""));

  if (!acl.empty())
  {
    f.setAcl(acl);
  }

  if (!owner.empty())
  {
    f.setOwner(owner);
  }
  VM* avm = NULL;
  if(!host.empty())
  {
    avm = getVM(host);
  } else {
    avm = &vPC.back();
  }

  if (!avm)
  {
    cout << "Unable to find host '" << host << "'" << endl;
    exit (ERR_BAD_SET);
  }

  if (!avm->addFile(f))
  {
    cout << "Unable to add '" << name << "'" << endl;
    return (ERR_BAD_SET);
  }
  return E_OK;
}

int addDir(lua_State *lua)
{
  string acl;
  string name;
  string key;
  string value;

  // number of input arguments
  int argc = lua_gettop(lua);

  // make sure argument passed is a table
  int idx = lua_gettop(lua);
  if (!lua_istable(lua, idx))
  {
    cout << "Error! Invalid call to addDir." << endl;
    exit (ERR_BAD_SET);
  }
  lua_pushnil(lua); // first key
  while (lua_next(lua, -2) != 0 )
  {
    key = lua_tostring(lua, -2);
    value = lua_tostring(lua, -1);

    // check keys
    if (key == "acl")
    {
      acl = value;
    }
    else if (key == "name")
    {
      name = value;
    }
    lua_pop(lua, 1);
  }

  // make sure there is a name set
  if (name.empty())
  {
    cout << "Error! No filename specified to addDir." << endl;
    exit (ERR_BAD_SET);
  }

  // create the file
  File f (File(name, T_FOLDER, "", ""));

  // set ACLs, if given
  if (!acl.empty())
  {
    f.setAcl(acl);
  }

  // push file to chain
  vPC.back().addFile(f);

  //cout << "[Lua] Add directory: " << name << endl;
  return E_OK;
}

int addFile(string name)
{
  // make sure there is a name set
  if (name.empty())
  {
    cout << "Error! No filename specified to addFile." << endl;
    exit (ERR_BAD_SET);
  }
  // create the file
  File f (File(name, T_FILE, "", ""));
  vPC.back().addFile(f);

  return E_OK;
}

int addFile(lua_State *lua)
{
  string acl;
  string key;
  string value;
  string name;
  string content;
  string exec;
  string on_delete;
  string owner;
  string host;
  bool suid (false);

  // number of input arguments
  int argc = lua_gettop(lua);

  // make sure argument passed is a table
  if (!lua_istable(lua, argc))
  {
    cout << "Error! Invalid call to addFile." << endl;
    exit (ERR_BAD_SET);
  }
  lua_pushnil(lua); // first key
  while (lua_next(lua, -2) != 0 )
  {
    key = lua_tostring(lua, -2);
    value = lua_tostring(lua, -1);

    // check keys
    if (key == "acl")
    {
      acl = value;
    }
    else if (key == "name")
    {
      name = value;
    }
    else if (key == "content")
    {
      content = value;
    }
    else if (key == "exec")
    {
      exec = value;
    }
    else if (key == "on_delete")
    {
      on_delete = value;
    }
    else if (key == "owner")
    {
      owner = value;
    }
    else if (key == "host")
    {
      host = value;
    }
    else if (key == "suid")
    {
      if ( value == "true" )
      {
        suid = true;
      }
    }

    lua_pop(lua, 1);
  }

  // make sure there is a name set
  if (name.empty())
  {
    cout << "Error! No filename specified to addFile." << endl;
    exit (ERR_BAD_SET);
  }

  // create the file
  File f (File(name, T_FILE, content, exec));

  // set ACLs, if given
  if (!acl.empty())
  {
    f.setAcl(acl);
  }

  // set owner if given
  if (!owner.empty())
  {
    f.setOwner(owner);
  }

  // set on_delete if given
  if (!on_delete.empty())
  {
    f.setOnDelete(on_delete);
  }

  // set suid if given
  if ( suid )
  {
    f.setSUID(true);
  }

  // find VM to add file to
  VM *vm;
  if (!host.empty())
  {
    for (int i=0; i<vPC.size(); i++)
    {
      if (vPC[i].getHostname() == host)
      {
        vm = &(vPC[i]);
      }
    }
  } else {
    if (shell.size())
    {
      vm = shell.back();
    } else {
      vm = &(vPC.back());
    }
  }
  if (!vm)
  {
    cout << "No host for file" << endl;
    return 1;
  }

  // push file to chain
  if (!vm->FSgetFree())
  {
    cout << "Error! Filesystem is full! Unable to add file: " << f.getName() << endl;
    exit (ERR_BAD_SET);
  }
  if (!vm->addFile(f))
  {
    cout << "Error! Illegal filename: " << f.getName() << endl;
    exit (ERR_BAD_SET);
  }
  return E_OK;
}

int addGPU(lua_State *lua)
{
  string desc;
  int power;

  power = lua_tonumber(lua, lua_gettop(lua));
  lua_pop(lua, 1);
  desc = lua_tostring(lua, lua_gettop(lua));
  lua_pop(lua, 1);

  if (power < 1) power = 1;
  vPC.back().addGPU(desc, power);
  return E_OK;
}

int addUser(lua_State *lua)
{
  string user;
  string password;

  // number of input arguments
  int argc = lua_gettop(lua);
  if (argc < 1 || 2 < argc)
  {
    cout << "Error! Invalid call to addUser." << endl;
    exit (ERR_BAD_SET);
  }

  VM* avm; // active VM
  avm = NULL;

  if (argc == 1)
  {
    int idx = lua_gettop(lua);
    if (!lua_istable(lua, idx))
    {
      cout << "Error! Invalid call to addUser." << endl;
      exit (ERR_BAD_SET);
    }
    
    lua_pushnil(lua); // first key
    while (lua_next(lua, -2) != 0 )
    {
      string key = lua_tostring(lua, -2);
      string value = lua_tostring(lua, -1);

      if (key == "host")
      {
	avm = getVM(value);
      }
      if (key == "user")
      {
	user = value;
      }
      if (key == "password")
      {
	password = value;
      }

      lua_pop(lua, 1);
    }
  } else {
    password = lua_tostring(lua, lua_gettop(lua));
    lua_pop(lua, 1);
    user = lua_tostring(lua, lua_gettop(lua));
    lua_pop(lua, 1);
  }

  if (user.empty())
  {
    cout << "Error! Invalid call to addUser." << endl;
    exit (ERR_BAD_SET);
  }

  if (!avm)
  {
    avm = &(vPC.back());
  }

  if (!avm->addUser(User(user, password)))
  {
    cout << "Fatal error adding user." << endl;
    exit (ERR_BAD_SET);
  }
  if (user != "root")
  {
    addDir("/home/"+user+"/", "770", user, avm->getHostname());
  }
  return E_OK;
}

int readfile (lua_State *lua)
{
  string filename;
  // number of input arguments
  int argc = lua_gettop(lua);

  if (argc > 1)
  {
    cout << "Error! Invalid call to readfile." << endl;
    exit (ERR_BAD_SET);
  }
  filename = lua_tostring (lua, lua_gettop(lua));
  lua_pop (lua, 1);
  File *fp = shell.back()->pFile(filename);
  if (!fp)
  {
    lua_pushnil(lua);
  } else {
    vector<string> content;
    explode(fp->getContent(),"\n", content);

    // create arg table
    lua_newtable(lua);
    // push cmd name
    for (int i=0; i<content.size(); i++)
    {
      lua_pushnumber(lua, i+1);
      lua_pushstring(lua, content[i].c_str());
      lua_settable(lua, -3);
    }
  }
  return 1;
}

int newVM(lua_State *lua)
{
  string hostname;
  // number of input arguments
  int argc = lua_gettop (lua);

  // check number of args

  // get hostname
  hostname = lua_tostring(lua, lua_gettop(lua));
  lua_pop(lua, 1);

  // create VM
  vPC.push_back(VM(hostname));

  // setup basic filesystem
  addDir("/");
  addDir("/bin/");
  addDir("/etc/");
  addDir("/home/");
  addDir("/lib/");
  addDir("/mnt/");
  addDir("/root/", "770", "root");
  addDir("/sbin/");
  addDir("/usr/");
  addDir("/var/");
  addDir("/var/log/");

  // add builtin commands for auto-complete?

  return E_OK;
}

int setProperty(lua_State *lua)
{
  string key;
  string value;
  // number of input arguments
  int argc = lua_gettop(lua);

  // make sure argument passed is a table
  int idx = lua_gettop(lua);
  if (!lua_istable(lua, idx))
  {
    cout << "Error! Invalid call to setProperty." << endl;
    exit (ERR_BAD_SET);
  }
  lua_pushnil(lua); // first key
  while (lua_next(lua, -2) != 0 )
  {
    key = lua_tostring(lua, -2);
    value = lua_tostring(lua, -1);

    // check keys
    if (key == "uname")
    {
      vPC.back().setUname(value);
    }
    else if (key == "ip")
    {
      // set the IP
      vPC.back().setIp(value);

      // register with DNS
      addDNS(vPC.back().getHostname(), value);
    }
    else if (key == "on_root")
    {
      vPC.back().onRoot(value);
    }
    else if (key == "on_login")
    {
      vPC.back().onLogin(value);
    }
    else if (key == "on_logout")
    {
      vPC.back().onLogout(value);
    }
    else if (key == "hint")
    {
      vPC.back().setHint(value);
    }
    else if (key == "netd")
    {
      if (lua_isnumber(lua,-1))
      {
        vPC.back().addNetDomain(registerNetDomain(lua_tonumber(lua, -1)));
      } else {
        if (pcrecpp::RE("[^0-9a-zA-Z+_-]").PartialMatch(value))
        {
          cout << "Error! Invalid network domain name: " << value << endl;
          exit (1);
        }
        vPC.back().addNetDomain(registerNetDomain(value));
      }
    }
    lua_pop(lua, 1);
  }
  vPC.back().setDefaultNetDomain();
  return E_OK;
}

bool sendMail(string t, string s, string f, string b)
{
  User *up = shell.back()->getUser(t);
  if (!up) return false;
  up->addMail(Mail(t, s, f, b));
  return true;
}

int sendMail(lua_State *lua)
{
  string key;
  string value;
  string t, f, s, b;
  string host;
  // number of input arguments
  int argc = lua_gettop(lua);

  // make sure argument passed is a table
  int idx = lua_gettop(lua);
  if (!lua_istable(lua, idx))
  {
    cout << "Error! Invalid call to sendMail." << endl;
    exit (ERR_BAD_SET);
  }
  lua_pushnil(lua); // first key
  while (lua_next(lua, -2) != 0 )
  {
    key = lua_tostring(lua, -2);
    value = lua_tostring(lua, -1);

    // check keys
    if (key == "to")
    {
      int at = value.find("@");
      if (at != string::npos)
      {
        t = value.substr(0,at);
        host = value.substr(at+1);
      } else {
        t = value;
      }
    }
    else if (key == "from")
    {
      f = value;
    }
    else if (key == "subject")
    {
      s = value;
    }
    else if (key == "body")
    {
      b = value;
    }

    lua_pop(lua, 1);
  }

  if (t.empty() || f.empty() || s.empty() || b.empty())
  {
    cout << "Error! Invalid call to sendMail." << endl;
    exit (ERR_BAD_SET);
  }

  User *up;
  if (!host.empty())
  {
    for (int i=0; i<vPC.size(); i++)
    {
      if (vPC[i].getHostname() == host)
      {
        up = vPC[i].getUser(t);
      }
    }
  } else {
    if (shell.size())
    {
      up = shell.back()->getUser(t);
    } else {
      up = vPC.back().getUser(t);
    }
  }
  if (!up)
  {
    cout << "No such user" << endl;
    return 1;
  }
  up->addMail(Mail(t, s, f, b));
  return E_OK;
}

int lua_addDNS(lua_State *lua)
{
  string name = lua_tostring(lua, lua_gettop(lua));
  lua_pop(lua, 1);
  string record = lua_tostring(lua, lua_gettop(lua));
  lua_pop(lua, 1);
  addDNS(name, record);
  return E_OK;
}

int lua_getDNS(lua_State *lua)
{
  string name = lua_tostring(lua, lua_gettop(lua));
  lua_pop(lua, 1);
  string record = getDNS(name);
  lua_pushstring(lua, record.c_str());
  return E_OK;
}

int lua_hashString(lua_State *lua)
{
  string plain = lua_tostring(lua, lua_gettop(lua));
  lua_pop(lua, 1);
  string hash = vhash(plain);
  registerPassword(plain);
  lua_pushstring(lua, hash.c_str());
  return 1;
}

int isDir(lua_State *lua)
{
  // number of input arguments
  int argc = lua_gettop(lua);

  string filename = lua_tostring(lua, -1); // get filename from Lua
  lua_pop(lua, 1);

  File *fp;

  // did we give a filename?
  if (filename.empty())
  {
    lua_pushboolean(lua, false);
    return 1;
  }

  // does the path exist?
  fp = shell.back()->pFile(filename);
  if (!fp)
  {
    lua_pushboolean(lua, false);
    return 1;
  }

  // is this a file?
  if (fp->getType() != T_FOLDER)
  {
    lua_pushboolean(lua, false);
    return 1;
  }

  // all tests passed!
  lua_pushboolean(lua, true);
  return 1;
}

int isFile(lua_State *lua)
{
  // number of input arguments
  int argc = lua_gettop(lua);

  string filename = lua_tostring(lua, -1); // get filename from Lua
  lua_pop(lua, 1);

  File *fp;

  // did we give a filename?
  if (filename.empty())
  {
    lua_pushboolean(lua, false);
    return 1;
  }

  // does the path exist?
  fp = shell.back()->pFile(filename);
  if (!fp)
  {
    lua_pushboolean(lua, false);
    return 1;
  }

  // is this a file?
  if (fp->getType() != T_FILE)
  {
    lua_pushboolean(lua, false);
    return 1;
  }

  // all tests passed!
  lua_pushboolean(lua, true);
  return 1;
}

int lua_md5(lua_State *lua)
{
  string plain = lua_tostring(lua, lua_gettop(lua));
  lua_pop(lua, 1);
  string hash = md5(plain);
  lua_pushstring(lua, hash.c_str());
  return 1;
}

int lua_addService(lua_State *lua)
{
  string key;
  string value;
  int port;
  string name;
  string exec;
  string query;
  bool start = false;

  // number of input arguments
  int argc = lua_gettop(lua);

  // make sure argument passed is a table
  int idx = lua_gettop(lua);
  if (!lua_istable(lua, idx))
  {
    cout << "Error! Invalid call to addService." << endl;
    exit (ERR_BAD_SET);
  }
  lua_pushnil(lua); // first key
  while (lua_next(lua, -2) != 0 )
  {
    key = lua_tostring(lua, -2);

    // check keys
    if (key == "port")
    {
      port = lua_tonumber(lua, -1);
    }
    else if (key == "name")
    {
      name = lua_tostring(lua, -1);
    }
    else if (key == "exec")
    {
      exec = lua_tostring(lua, -1);
    }
    else if (key == "poll")
    {
      query = lua_tostring(lua, -1);
    }
    else if (key == "start")
    {
      string s = lua_tostring(lua, -1);
      if (s == "true")
      {
        start = true;
      }
    }
    lua_pop(lua, 1);
  }
  if (!vPC.back().addDaemon(Daemon(port,name,exec,query)))
  {
    cout << "Error! Unable to add service" << endl;
  }
  if (start)
  {
    vPC.back().startDaemon(name);
  }
  return E_OK;
}

int lua_addNetDomain(lua_State *lua)
{
  int domain_id;
  string domain_name;
  std::map<int, string>::iterator it;

  // make sure domains are registered
  if (lua_isnumber(lua, -1))
  {
    domain_id = lua_tonumber(lua,-1);
    registerNetDomain(domain_id);
  } else {
    domain_name = lua_tostring(lua,-1);
    if (pcrecpp::RE("[^0-9a-zA-Z+_-]").PartialMatch(domain_name))
    {
      cout << domain_name.length() << endl;
      cout << "Error! Invalid network domain name: " << domain_name << endl;
      lua_pop(lua, 1);
      exit (1);
    }
    domain_id = registerNetDomain(domain_name);
  }
  lua_pop(lua, 1);

  // add domain membership
  vPC.back().addNetDomain(domain_id);
  return E_OK;
}

int lua_listNetDomains(lua_State *lua)
{
  // number of input arguments
  int argc = lua_gettop(lua);

  if (argc != 1)
  {
    cout << "Error! Invalid call to listNetDomains." << endl;
    exit (ERR_BAD_SET);
  }

  string host = lua_tostring(lua, 1);
  lua_pop(lua, 1);

  for (int i=0; i<vPC.size(); i++)
  {
    if (vPC[i].getIp() == getDNS(host))
    {
      vPC[i].listNetDomains();
      break;
    }
  }
  lua_pushboolean(lua, false);
  return 1;
}

int lua_inNetDomain(lua_State *lua)
{
  // number of input arguments
  int argc = lua_gettop(lua);

  if (argc != 2)
  {
    cout << "Error! Invalid call to inNetDomain." << endl;
    exit (ERR_BAD_SET);
  }

  int id = 0;
  if (lua_isnumber(lua, -1))
  {
    id = lua_tonumber(lua, -1);
  } else {
    id = getNetDomainId(lua_tostring(lua, -1));
  }
  lua_pop(lua, 1);

  string host = lua_tostring(lua, 1);
  lua_pop(lua, 1);

  bool ec = false;
  for (int i=0; i<vPC.size(); i++)
  {
    if (vPC[i].getIp() == getDNS(host) && vPC[i].inNetDomain(id))
    {
      ec = true;
      break;
    }
  }
  lua_pushboolean(lua, ec);
  return 1;
}

int lua_getCWait(lua_State *lua)
{
  int power;
  int seconds;
  int score;

  power = shell.back()->getCPower();
  score = lua_tonumber(lua,-1);
  lua_pop(lua, 1);
  seconds = score / (int) sqrt((double) power);
  lua_pushnumber(lua, seconds);
  return 1;
}

int lua_fsMax(lua_State *lua)
{
  // number of input arguments
  int argc = lua_gettop(lua);

  if (argc > 1)
  {
    cout << "Error! Invalid call to fsMax." << endl;
    exit (ERR_BAD_SET);
  }
  int fs_size = lua_tointeger(lua, -1);
  lua_pop(lua, 1);

  vPC.back().FSsetMax(fs_size);

  return E_OK;
}

int startService(lua_State *lua)
{
  // number of input arguments
  int argc = lua_gettop(lua);

  if (argc > 1)
  {
    cout << "Error! Invalid call to startService." << endl;
    exit (ERR_BAD_SET);
  }

  string name = lua_tostring(lua, -1);
  lua_pop(lua, 1);

  shell.back()->startDaemon(name);
  return E_OK;
}

int stopService(lua_State *lua)
{
  // number of input arguments
  int argc = lua_gettop(lua);

  if (argc > 1)
  {
    cout << "Error! Invalid call to stopService." << endl;
    exit (ERR_BAD_SET);
  }

  string name = lua_tostring(lua, -1);
  lua_pop(lua, 1);

  shell.back()->stopDaemon(name);
  return E_OK;
}

int serviceRunning(lua_State *lua)
{
  // number of input arguments
  int argc = lua_gettop(lua);

  if (argc != 2)
  {
    cout << "Error! Invalid call to serviceRunning." << endl;
    exit (ERR_BAD_SET);
  }

  string srvc = lua_tostring(lua, -1);
  lua_pop(lua, 1);
  string host = lua_tostring(lua, -1);
  lua_pop(lua, 1);

  for (int i=0; i<vPC.size(); i++)
  {
    if (vPC[i].getIp() == getDNS(host))
    {
      //VM *h = &(vPC[i]);
      vector<Daemon*> inetd = vPC[i].getDaemons();
      for (int n = 0; n<inetd.size(); n++)
      {
        if (inetd[n]->name == srvc)
        {
          // return true
          lua_pushboolean(lua, true);
          return 1;
        }
      }
    }
  }
  // return false
  lua_pushboolean(lua, false);
  return 1;
}

int lua_login(lua_State *lua)
{
  string key;
  string value;
  string host;
  string user;
  string password;
  bool skip_p = true;
  VM *vm;

  // number of input arguments
  int argc = lua_gettop(lua);

  // make sure argument passed is a table
  int idx = lua_gettop(lua);
  if (!lua_istable(lua, idx))
  {
    cout << "Error! Invalid call to login." << endl;
    exit (ERR_BAD_SET);
  }
  lua_pushnil(lua); // first key
  while (lua_next(lua, -2) != 0 )
  {
    key = lua_tostring(lua, -2);
    value = lua_tostring(lua, -1);

    // check keys
    if (key == "host")
    {
      host = value;
    }
    else if (key == "user")
    {
      user = value;
    }
    else if (key == "password")
    {
      password = value;
      skip_p = false;
    }
    lua_pop(lua, 1);
  }
  if (host.empty() || user.empty())
  {
    cout << "Error! Invalid call to login." << endl;
    exit (ERR_BAD_SET);
  }

  if (shell.back()->getIp() == getDNS(host))
  {
    vm = shell.back();
  } else {
    for (int i=0; i<vPC.size(); i++)
    {
      if (vPC[i].getIp() == getDNS(host))
      {
        vm = &(vPC[i]);
        //shell.push_back(vm);  // cause of seg faults
        break;
      }
    }
  }

  if ( !vm)
  {
    lua_pushboolean(lua, false);
    return 1;
  }

  if (skip_p)
  {
    if (!vm->login(user)) // seg faults here
    {
      lua_pushboolean(lua, false);
      return 1;
    }
  } else {
    vm->login(user, password);
  }
  if (shell.back() != vm) shell.push_back(vm);
  lua_pushboolean(lua, true);
  return 1;
}

int lua_logout(lua_State *lua)
{
  cmd_exit();
  return E_OK;
}

int lua_cwd(lua_State *lua)
{
  lua_pushstring(lua, shell.back()->getCwd().c_str());
  return 1;
}

int lua_hostname(lua_State *lua)
{
  lua_pushstring(lua, shell.back()->getHostname().c_str());
  return 1;
}

int lua_echo(lua_State *lua)
{
  // number of input arguments
  int argc = lua_gettop(lua);

  for (int i=0; i<argc; i++)
  {
    cout << lua_tostring(lua, -1) << flush;
    lua_pop(lua, 1);
  }
  return E_OK;
}

int lua_input(lua_State *lua)
{
  string input = tc.getLine();
  lua_pushstring(lua, input.c_str());
  return 1;
}

int lua_pause (lua_State *lua)
{
  tc.pause();
  return E_OK;
}

int lua_sleep (lua_State *lua)
{
  // number of input arguments
  int argc = lua_gettop(lua);

  if (argc != 1)
  {
    cout << "Error! Invalid call to sleep." << endl;
    exit (ERR_BAD_SET);
  }
  boost::this_thread::sleep(boost::posix_time::seconds(lua_tonumber(lua, -1)));
  lua_pop(lua, 1);
  return E_OK;
}

int lua_timestamp(lua_State *lua)
{
  // current date/time based on current system
  time_t now = time(0);
  lua_pushnumber(lua, now);
  return 1;
}

int lua_garbage(lua_State *lua)
{
  // get time
  time_t rawtime;
  time (&rawtime);
  struct tm *ltime;
  ltime = localtime(&rawtime);

  // seed random with current second
  srand(ltime->tm_sec);

  // set vars
  int limit = 600;
  char c;
  string s;

  // number of input arguments
  int argc = lua_gettop(lua);

  if (argc)
  {
    for (int i=1; i<argc+1; i++)
    {
      switch (argc)
      {
        case 1:
          limit = lua_tonumber(lua, -1);
          break;
      }
      lua_pop(lua, 1);
    }
  }

  for (int i=0; i<limit; i++)
  {
    c = rand();
    if ( c == 11 || c == 12 )
    {
      c = 32;
    }
    if ( c == 10 )
    {
      if ((char) rand() > 40)
      {
        c = 32;
      }
    }
    s += c;
  }
  lua_pushstring(lua, s.c_str());
  return 1;
}

int lua_vdump(vector<string> args)
{
  if (!debug_on)
  {
    cout << "Debug off" << endl;
    return E_OK;
  }
  if (args.size() != 2)
  {
    cout << "Invalid number of arguments to: 'dump'" << endl;
    return E_OK;
  }

  string key;
  int type;

  // let's get our globals
  lua_pushglobaltable(lua);
  lua_pushnil(lua);
  while (lua_next(lua, -2) != 0)
  {
    key = lua_tostring(lua, -2);
    type = lua_type(lua, -1);

    // skip reserved keys
    if (!key.empty() && key.substr(0,1)=="_")
    {
      lua_pop(lua,1);
      continue;
    }

    if (key == args[1] || args[1] == "@")
    {
      cout << "[DBG] var: " << key << " = ";
      if (type == LUA_TBOOLEAN)
      {
        cout << " (b) " << lua_toboolean(lua, -1);
      }
      else if (type == LUA_TNUMBER)
      {
        cout << " (i) " << lua_tonumber(lua, -1);
      }
      else if (type == LUA_TSTRING)
      {
        cout << " (s) " << lua_tostring(lua, -1);
      }
      cout << endl;
    }
    lua_pop(lua, 1);
  }
  return E_OK;
}

int old_restore(string filename)
{
  ifstream savfile(filename.c_str());
  if (!savfile.is_open())
  {
    cout << "Error! Unable to open save file." << endl;
    return E_BAD;
  }

  string data;

  while (std::getline(savfile, data))
  {
    data = ab64_decode(data);
    // let's process the saved data
    if (data.substr(0,3)=="vm:")
    {
      VM* avm; // active VM
      std::istringstream vm(data);
      while (std::getline(vm, data))
      {
        vector<string> store;
        if (data.substr(0,3)=="vm:")
        {
          string hostname = data.substr(3);
          if (debug_on) cout << "[DBG] Loading vm: " << hostname << endl;
          avm = getVM(hostname);
          if (!avm)
          {
            cout << "Error! Corrupt save file." << endl;
            exit(1);
          }
          // reset the machine so we can re-add state data
          avm->reset();
          // add root of filesystem
          File f (File("/", T_FOLDER, "", ""));
          avm->addFile(f);
        }
        else if (data.substr(0,5)=="file:")
        {
          string f_parent;
          string f_name;
          string f_acl;
          string f_owner;
          string f_exec;
          string f_content;
          string f_on_delete;
          int f_type;
          int dbg = 1;

          data = data.substr(5);
          explode(data,",",store);
          for (int i=0;i<store.size();i++)
          {
            vector<string> kv;
            if (store[i] == "f") f_type = T_FILE;
            if (store[i] == "d") f_type = T_FOLDER;
            explode(store[i],"=",kv,1);
            if (kv.size()==1) kv.push_back("");
            if (kv.size())
            {
              if (kv[0] == "parent")
              {
                f_parent = kv[1];
              }
              else if (kv[0] == "name")
              {
                f_name = kv[1];
              }
              else if (kv[0] == "acl")
              {
                f_acl = kv[1];
              }
              else if (kv[0] == "owner")
              {
                f_owner = kv[1];
              }
              else if (kv[0] == "exec")
              {
                if (kv.size()==2)
                  f_exec = kv[1];
              }
              else if (kv[0] == "content")
              {
                if (debug_on) cout << "[DBG] (" << kv.size() << ") Loading content for " << f_name << endl;
                if (kv.size()==2)
                  f_content = ab64_decode(kv[1]);
              }
              else if (kv[0] == "on_delete")
              {
                if (kv.size()==2)
                  f_on_delete = kv[1];
              }
            }
          }
          if (!f_parent.empty() && !f_name.empty())
          {
            if (debug_on) cout << "[DBG] Loading file: " << f_parent+f_name << endl;
            if (debug_on) cout << "[DBG]    exec: " << f_exec << endl;
            if (debug_on) cout << "[DBG]    on_delete: " << f_on_delete << endl;
            if (debug_on) cout << "[DBG]    content: " << f_content << endl;
            // make file here
            File f (File(f_parent+f_name, f_type, f_content, f_exec));
            f.setAcl(f_acl);
            f.setOwner(f_owner);
            if (!f_on_delete.empty())
            {
              f.setOnDelete(f_on_delete);
            }
            if (!avm->addFile(f))
            {
              cout << "Error! Unable to create file: " << f_name << endl;
              exit(ERR_BAD_SET);
            }
          }
        }
        else if (data.substr(0,2)=="p:")
        {
          data = data.substr(2);
          vector<string> kv;
          explode(data,"=",kv);
          if (kv[0] == "cwd")
          {
            avm->setCwd(kv[1]);
          }
          else if (kv[0] == "bc")
          {
            avm->setBrokenCounter(std::atoi(kv[1].c_str()));
          }
          else if (kv[0] == "has_root")
          {
            if (kv[1] == "true") avm->setRoot(true);
              else avm->setRoot(false);
          }
        }
        else if (data.substr(0,6)=="chain:")
        {
          data = data.substr(6);
          explode(data,",",store);
          // loop through and log the users in to resetup the chain
          for (int i=0;i<store.size();i++)
          {
            if (!avm->login(store[i]))
              cout << "Unable to chain " << store[i] << endl;
          }
        }
        else if (data.substr(0,5)=="netd:")
        {
          data = data.substr(5);
          explode(data,",",store);
          for (int i=0;i<store.size();i++)
          {
            avm->addNetDomain(std::atoi(store[i].c_str()));
          }
        }
      }
    }
    else if (data.substr(0,5)=="netd:")
    {
      if (debug_on) cout << "[DBG] Loading net data" << endl;
      data = data.substr(5);
      vector<string> store;
      explode(data,",",store);
      netd_map.clear();
      for (int i=0;i<store.size();i++)
      {
        vector<string> kv;
        explode(store[i],"=",kv);
        if (kv.size()==1) kv.push_back("");
        netd_map[std::atoi(kv[0].c_str())] = kv[1];
      }
    }
    else if (data.substr(0,6)=="shell:")
    {
      if (debug_on) cout << "[DBG] Building shell chain" << endl;
      vector<string> store;
      data = data.substr(6);
      explode(data,",",store);

      // clean up the shell chain
      shell.clear();

      // setup new chain
      for (int i=0;i<store.size();i++)
      {
        VM* vm = getVM(store[i]);
        if (!vm)
        {
          cout << "Error! Invalid host in shell chain!" << endl;
          exit (1);
        }
        shell.push_back(vm);
      }
    }
    else if (data.substr(0,4)=="lua:")
    {
      if (debug_on) cout << "[DBG] Loading Lua globals" << endl;
      data = data.substr(4);
      data = ab64_decode(data.c_str());
      luaL_dostring(lua, data.c_str());
    }
  }
  savfile.close();
  cout << "Restored!" << endl;

  return E_OK;
}

int new_restore(string filename)
{
  pt::ptree json;
  pt::read_json(filename, json);
  if (json.empty())
  {
    cout << "Error! Unable to open save file." << endl;
    return E_BAD;
  }

  string data;

  for (pt::ptree::iterator it = json.begin(); it != json.end(); it++)
  {
    // let's process the saved data
    if (it->first=="vm")
    {
      VM* avm; // active VM
      pt::ptree vm = it->second;
      for (pt::ptree::iterator vm_it = vm.begin(); vm_it != vm.end(); vm_it++)
      {
        vector<string> store;
        if (vm_it->first=="hostname")
        {
          string hostname = vm_it->second.get_value<string>();
          if (debug_on) cout << "[DBG] Loading vm: " << hostname << endl;
          avm = getVM(hostname);
          if (!avm)
          {
            cout << "Error! Corrupt save file." << endl;
            exit(1);
          }
          // reset the machine so we can re-add state data
          avm->reset();
          // add root of filesystem
          File f (File("/", T_FOLDER, "", ""));
          avm->addFile(f);
        }
        else if (vm_it->first=="file")
        {
          string f_parent;
          string f_name;
          string f_acl;
          string f_owner;
          string f_exec;
          string f_content;
          string f_on_delete;
          int f_type;
          int dbg = 1;

          pt::ptree file = vm_it->second;
          for (pt::ptree::iterator f_it = file.begin(); f_it != file.end(); f_it++)
          {
            vector<string> kv;
            if (f_it->second.get_value<string>() == "f") f_type = T_FILE;
            if (f_it->second.get_value<string>() == "d") f_type = T_FOLDER;
            if (f_it->first == "parent")
            {
              f_parent = f_it->second.get_value<string>();
            }
            else if (f_it->first == "name")
            {
              f_name = f_it->second.get_value<string>();
            }
            else if (f_it->first == "acl")
            {
              f_acl = f_it->second.get_value<string>();
            }
            else if (f_it->first == "owner")
            {
              f_owner = f_it->second.get_value<string>();
            }
            else if (f_it->first == "exec")
            {
              f_exec = f_it->second.get_value<string>();
            }
            else if (f_it->first == "content")
            {
              if (debug_on) cout << "[DBG] Loading content for " << f_name << endl;
              f_content = f_it->second.get_value<string>();
            }
            else if (f_it->first == "on_delete")
            {
              f_on_delete = f_it->second.get_value<string>();
            }
          }
          if (!f_parent.empty() && !f_name.empty())
          {
            if (debug_on) cout << "[DBG] Loading file: " << f_parent+f_name << endl;
            if (debug_on) cout << "[DBG]    exec: " << f_exec << endl;
            if (debug_on) cout << "[DBG]    on_delete: " << f_on_delete << endl;
            if (debug_on) cout << "[DBG]    content: " << f_content << endl;
            // create file here
            File f (File(f_parent+f_name, f_type, f_content, f_exec));
            f.setAcl(f_acl);
            f.setOwner(f_owner);
            if (!f_on_delete.empty())
            {
              f.setOnDelete(f_on_delete);
            }
            if (!avm->addFile(f))
            {
              cout << "Error! Unable to create file: " << f_name << endl;
              exit(ERR_BAD_SET);
            }
          }
        }
	else if (vm_it->first=="cwd")
        {
          avm->setCwd(vm_it->second.get_value<string>());
        }
	else if (vm_it->first=="bc")
        {
          avm->setBrokenCounter(vm_it->second.get_value<int>());
        }
	else if (vm_it->first=="has_root")
        {
          avm->setRoot(vm_it->second.get_value<bool>());
	}
        else if (vm_it->first=="chain")
        {
	  data = vm_it->second.get_value<string>();
          explode(data,",",store);
          // loop through and log the users in to resetup the chain
          for (int i=0;i<store.size();i++)
          {
            if (!avm->login(store[i]))
              cout << "Unable to chain " << store[i] << endl;
          }
        }
        else if (vm_it->first=="netd")
        {
	  data = vm_it->second.get_value<string>();
          explode(data,",",store);
          for (int i=0;i<store.size();i++)
          {
            avm->addNetDomain(std::atoi(store[i].c_str()));
          }
        }
      }
    }
    else if (it->first=="netd")
    {
      if (debug_on) cout << "[DBG] Loading net data" << endl;
      data = it->second.get_value<string>();
      vector<string> store;
      explode(data,",",store);
      netd_map.clear();
      for (int i=0;i<store.size();i++)
      {
        vector<string> kv;
        explode(store[i],"=",kv);
        if (kv.size()==1) kv.push_back("");
        netd_map[std::atoi(kv[0].c_str())] = kv[1];
      }
    }
    else if (it->first=="shells")
    {
      if (debug_on) cout << "[DBG] Building shell chain" << endl;
      vector<string> store;
      data = it->second.get_value<string>();
      explode(data,",",store);

      // clean up the shell chain
      shell.clear();

      // setup new chain
      for (int i=0;i<store.size();i++)
      {
        VM* vm = getVM(store[i]);
        if (!vm)
        {
          cout << "Error! Invalid host in shell chain!" << endl;
          exit (1);
        }
        shell.push_back(vm);
      }
    }
    else if (it->first=="globals")
    {
      if (debug_on) cout << "[DBG] Loading Lua globals" << endl;
      data = it->second.get_value<string>();
      luaL_dostring(lua, data.c_str());
    }
  }
  //savfile.close();
  cout << "Restored!" << endl;

  return E_OK;
}

int lua_restore(vector<string> args)
{
  if (args.size() != 2)
  {
    cout << "Invalid number of arguments to: 'load'" << endl;
    return E_BAD;
  }

  FILE *fp;
  string filename = args[1];
  char c;

  // if they typed in .sav, remove it for analysis
  if (filename.length() > 4 && filename.substr(strlen(filename.c_str())-4,4)==".sav")
  {
    filename = filename.substr(0,strlen(filename.c_str())-4);
  }

  if (pcrecpp::RE("[^0-9a-zA-Z+_-]").PartialMatch(filename))
  {
    cout << "Invalid save game name. Only the following characters are allowed:" << endl;
    cout << "0-9 a-z A-Z '+' '_' '-'" << endl;
    return E_BAD;
  }
  // add .sav to the filename
  filename += ".sav";

  // determine the save format
  fp = fopen(filename.c_str(), "r");
  if (!fp)
  {
    cout << "Error! Unable to open file!" << endl;
    return E_BAD;
  }
  c = fgetc(fp);
  fclose(fp);

  // restore from file
  if (c == '{')
  {
    return new_restore(filename);
  } else {
    return old_restore(filename);
  }
}

int lua_save(vector<string> args)
{
  if (args.size() != 2)
  {
    cout << "Invalid number of arguments to: 'save'" << endl;
    return E_BAD;
  }

  FILE *fp;
  string filename = args[1];

  if (pcrecpp::RE("[^0-9a-zA-Z+_-]").PartialMatch(filename))
  {
    cout << "Invalid save game name. Only the following characters are allowed:" << endl;
    cout << "0-9 a-z A-Z '+' '_' '-'" << endl;
    return E_BAD;
  }
  filename += ".sav";

  pt::ptree json;
  string key;
  int type;
  string _lua_data = "";

  // state save format version
  json.put("version", 2);

  // let's get our globals
  lua_pushglobaltable(lua);
  lua_pushnil(lua);
  while (lua_next(lua, -2) != 0)
  {
    key = lua_tostring(lua, -2);
    type = lua_type(lua, -1);

    // skip reserved keys
    if (!key.empty() && key.substr(0,1)=="_")
    {
      lua_pop(lua,1);
      continue;
    }

    if (type == LUA_TBOOLEAN)
    {
      bool b = lua_toboolean(lua, -1);
      _lua_data.append(key).append(" = ");
      if (b)
      {
        _lua_data.append("true");
      } else {
        _lua_data.append("false");
      }
      _lua_data.append("\n");
    }
    else if (type == LUA_TNUMBER)
    {
      ostringstream num;
      num << lua_tonumber(lua, -1);
      _lua_data.append(key).append(" = ").append(num.str());
      _lua_data.append("\n");
    }
    else if (type == LUA_TSTRING)
    {
      string s = lua_tostring(lua, -1);
      // let's sanitize s!
      pcrecpp::RE("\"").GlobalReplace("\\\\\"", &s);
      _lua_data.append(key).append(" = \"").append(s)
        .append("\"");;
      _lua_data.append("\n");
    }
    lua_pop(lua, 1);
  }
  lua_pop(lua,1);
  // pack globals
  json.put("globals", _lua_data);
  _lua_data = "lua:" + ab64_encode(_lua_data.c_str(), _lua_data.length());
  // pack statement
  _lua_data = ab64_encode(_lua_data.c_str(), _lua_data.length());
  // save data
  //fputs(_lua_data.c_str(), fp);
  //fputs("\n", fp);

  // save all net_domains
  {
    //string data = "netd:";
    string data;
    std::map<int, std::string>::iterator it;

    for (it=netd_map.begin(); it!=netd_map.end(); it++)
    {
      ostringstream num;
      num << it->first;
      if (it!=netd_map.begin()) data.append(",");
      data.append(num.str()).append("=").append(it->second);
    }
    json.put("netd", data);
    //data = ab64_encode(data.c_str(), data.length());
    //fputs(data.c_str(), fp);
    //fputs("\n", fp);
  }

  // save all VMs
  {
    vector<VM>::iterator it;
    for (it=vPC.begin(); it!=vPC.end(); it++)
    {
      //string vms = it->serialize();;
      pt::ptree vms = it->serialize();;
      json.push_back(std::make_pair("vm", vms));
      //fputs(vms.c_str(), fp);
      //fputs("\n", fp);
    }
  }

  // save shell chain
  {
    vector<VM*>::iterator it;
    string data;
    for (it=shell.begin(); it!=shell.end(); it++)
    {
      string hostname = (*it)->getHostname();
      if (it!=shell.begin()) data.append(",");
      data.append(hostname);
    }
    json.put("shells", data);
    //data = ab64_encode(data.c_str(), data.length());
    //fputs(data.c_str(), fp);
    //fputs("\n", fp);
  }
  //pt::write_json(std::cout, json);
  pt::write_json(filename, json);

  // close save file
  //fclose(fp);
  cout << "Saved!" << endl;

  return E_OK;
}

//int version (lua_State *lua)
//{
//  lua_pushnumber(lua, ENGINE_VERSION);
//  return 1;
//}

int lua_quit (lua_State *lua)
{
  // maybe some cleanup should go here?
  exit (0);
}

// load game environment
int loadEnv()
{
  // create Lua state
  lua = luaL_newstate();

  // load Lua libraries
  static const luaL_Reg lualibs[] =
  {
    { "base", luaopen_base },
    { "string", luaopen_string },
    { "math", luaopen_math },
    { NULL, NULL}
  };

  const luaL_Reg *lib = lualibs;
  for (; lib->func != NULL; lib++)
  {
    // use luaL_requiref to set lib globals
    luaL_requiref(lua, lib->name, lib->func, 1);
    lib->func(lua);
    lua_settop(lua, 0);
  }

  // register functions to be called in lua
  lua_register(lua, "readfile", readfile);
  lua_register(lua, "newVM", newVM);
  lua_register(lua, "setProperty", setProperty);
  lua_register(lua, "addDir", addDir);
  lua_register(lua, "addFile", addFile);
  lua_register(lua, "addGPU", addGPU);
  lua_register(lua, "addService", lua_addService);
  lua_register(lua, "addNetDomain", lua_addNetDomain);
  lua_register(lua, "addUser", addUser);
  lua_register(lua, "getCWait", lua_getCWait);
  lua_register(lua, "fsMax", lua_fsMax);
  lua_register(lua, "inNetDomain", lua_inNetDomain);
  lua_register(lua, "listNetDomains", lua_listNetDomains);
  lua_register(lua, "isDir", isDir);
  lua_register(lua, "isFile", isFile);
  lua_register(lua, "startService", startService);
  lua_register(lua, "stopService", stopService);
  lua_register(lua, "serviceRunning", serviceRunning);
  lua_register(lua, "login", lua_login);
  lua_register(lua, "logout", lua_logout);
  lua_register(lua, "mail", sendMail);
  lua_register(lua, "md5", lua_md5);
  lua_register(lua, "addDNS", lua_addDNS);
  lua_register(lua, "getDNS", lua_getDNS);
  lua_register(lua, "hash", lua_hashString);
  lua_register(lua, "echo", lua_echo);
  lua_register(lua, "input", lua_input);
  lua_register(lua, "hostname", lua_hostname);
  lua_register(lua, "cwd", lua_cwd);
  lua_register(lua, "pause", lua_pause);
  lua_register(lua, "sleep", lua_sleep);
  lua_register(lua, "timestamp", lua_timestamp);
  lua_register(lua, "garbage", lua_garbage);
  lua_register(lua, "quit", lua_quit);

  // setup the default Network Domain
  netd_map[1] = "default";

  // init pointer for current VM
  VM *pVM;

  #if !defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)
  char * mfn = (char *) malloc(mission_file.length()+1);
  strcpy(mfn, mission_file.c_str());
  cout << "Loading mission pack (" << basename(mfn) << ")..." << endl;
  free(mfn);
  #else
  char drive[255];
  char path[255];
  char fname[255];
  char fext[255];
  _splitpath_s(mission_file.c_str(), drive, path, fname, fext);
  cout << "Loading mission pack (" << fname << "." << fext << ")..." << endl;
  #endif

  // run the Lua script
  if (luaL_dofile(lua, mission_file.c_str())==1)
  {
    cout << "Error! This mission pack seems to contain errors." << endl;
    cout << lua_tostring(lua, -1) << endl;
    exit (ERR_BAD_MIS_PACK);
  }

  if (vPC.size()==0)
  {
    cout << "Error! No virtual machines have been loaded. Game can not continue." << endl;
    exit (ERR_NO_VM);
  }

  // init shell to first VM
  shell.push_back(&(vPC[0]));

  // set engine version as a global
  lua_pushnumber(lua, ENGINE_VERSION);
  lua_setglobal(lua, "VERSION");

  // launch intro func and begin game
  luaL_dostring(lua, "intro()");
  return E_OK;
}

void signalHandler(int signum)
{
  if (cmd_thread)
  {
    cmd_thread->interrupt();
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
    // Let's fake the Unix-type output you'd expect
    cout << "^C";
#endif
    cout << endl;
  } else {
    if (!tc.getBuffer().empty())
    {
      tc.setBuffer("");
      cout << endl << "$ " << flush;
    }
  }
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
  // on Win32, handle gets reset after call, so we call it again
  signal(SIGINT, signalHandler);
#endif
}

// entry point
int main(int argc, char** argv)
{
  string opt;
  string user;
  string password;
  mission_file = "destiny.mis";
  debug_on = false;

  // parse options
  for ( int i=1; i<argc; i++ )
  {
    if (argv[i][0] != '-')
    {
      mission_file = argv[i];
      break;
    }
    opt = argv[i];
    if (opt == "-v" || opt == "--version")
    {
      cout << ENGINE_NAME << " (the hacker simulator)" << endl;
      cout << "Version: " << ENGINE_VERSION << endl;
      cout << endl;
      cout << "Designer & Lead Programmer: Unix-Ninja" << endl;
      cout << "Technical Consultant: Till Varoquaux" << endl;
      cout << endl;
      exit(0);
    }
    else if (opt == "-h" || opt == "--help")
    {
      cout << "usage: " << argv[0] << " [options] [mission-pack]" << endl;
      cout << "    -d, --debug       Enable debug output" << endl;
      cout << "    -h, --help        Displays this help notice" << endl;
      cout << "    -v, --version     Displays version info and credits" << endl;
      exit(0);
    }
    else if (opt == "-d" || opt == "--debug")
    {
      debug_on = true;
      cout << "*debug on*" << endl;
    }
  }

  #if !defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)
    mission_file = (string) dirname(argv[0]) + (string) "/" + mission_file;
  #else
    char drive[255];
    char path[255];
    char fname[255];
    char fext[255];
    _splitpath_s(argv[0], drive, path, fname, fext);
    mission_file = (string) drive + (string) path + mission_file;
  #endif

  // load environment
  loadEnv();

  // register SIGINT handler
  signal(SIGINT, signalHandler);

  for (;;)
  {
    if (shell.back()->getUserCount()>0) break;
    cout << "Login: ";
    user = tc.getLine();
    cout << "Password: ";
    password = tc.getLine(M_PASSWORD);
    if (shell.back()->login(user, password))
    {
      break;
    }
    cout << endl;
  }
  // load terminal input
  tc.run();
  // quit from terminal. cleanup now.
  lua_close(lua); // clean Lua state
  cout << endl;
  return E_OK;
}
