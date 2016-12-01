#include <lua.hpp>
#define PCRE_STATIC
#include <pcrecpp.h>
#include <boost/thread/thread_only.hpp>
#include <boost/thread/xtime.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <algorithm>
#include <bitset>
#include <csignal>
#include <iostream>
#include <map>
#include <math.h>
#include <stdio.h>
#include <string>
#include <vector>
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <windows.h>
extern void sleep(int t);
#endif

#define ENGINE_NAME "The Hacker's Sandbox"
#define ENGINE_VERSION 1.19

#define debug cout
#define T_FOLDER 0
#define T_FILE 1

#define E_NO_FILE 1
#define E_DENIED 2
#define E_BAD 3
#define E_OK 0
#define E_BREAK -1

#define ERR_NO_VM 1
#define ERR_BAD_SET 2
#define ERR_BAD_MIS_PACK 3

using namespace std;
namespace pt = boost::property_tree;

struct RT
{
  string hash;
  string plain;
  RT(string h, string p):hash(h),plain(p){};
};

class File
{
  int filetype;
  string parent;
  string filename;
  string exec;
  string content;
  string owner;
  string acl;
  string on_delete;
  vector <bitset<3> > getBitset();
  bool setuid;

public:
  File ();
  File (string n, int t, string c, string e);
  string getAcl ();
  string getContent();
  string getExec ();
  int    getType ();
  string getName ();
  string getOwner();
  string getParent ();
  string getOnDelete();

  bool setAcl(string acl);
  bool setOwner(string s);
  bool setOnDelete(string s);
  bool isReadable();
  bool isWritable();
  bool isExecutable();
  void setSUID(bool);

  string drawAcl();
};

class Mail
{
  string header_to;
  string header_subject;
  string header_from;
  string msg_body;
public:
  Mail(string t, string s, string f, string b);
  string to();
  string subject();
  string from();
  string body();
};

bool registerPassword(string s);

class User
{
  string name;
  string password;
  bool broken;
  vector<Mail> inbox;
public:
  User (string username, string password);
  string getName();
  bool isPassword(string s);
  bool setPassword(string s);
  bool addMail(Mail);
  bool deleteMail(int index);
  Mail* getMail(int index);
  vector<Mail> listInbox();
  int inboxSize();
};

struct Daemon
{
public:
  int port;
  string name;
  string exec;
  string query;
  bool started;
  Daemon(int port, string name, string exec, string query);
};

class VM
{
  int broken_counter;
  string cpu;
  vector<string> gpus;
  int compute_power;
  string cwd;
  int fs_maxSize;
  bool has_root;
  string hostname;
  string ip;
  string uname;
  string hint;
  vector<User> users;
  vector<User*> user_chain;
  vector<File> filesystem;
  string on_root;
  string on_login;
  string on_logout;
  vector<Daemon> services;
  vector<int> net_domains;

public:

  VM (string hn);
  //string serialize();
  pt::ptree serialize();
  string getCpu ();
  string getCwd ();
  const vector<string> getFiles (string s);
  File *pFile (string s);
  string getHint();
  string getHostname ();
  string getIp ();
  string getUname ();
  string getUsername ();
  User *getUser(string username);
  int getUserCount();
  int  setCwd (string s);
  void setHint(string s);
  void setIp (string s);
  void setUname (string s);
  bool addFile (File f);
  bool deleteFile (string filename);
  bool addUser (User u);
  bool login(string username, string password);
  bool login(string username);
  int  logout();
  bool startBrokenCounter();
  void setBrokenCounter(int);
  bool hasRoot();
  void setRoot(bool);
  void onRoot(string s);
  void onLogin(string s);
  void onLogout(string s);
  bool startDaemon(string daemon);
  bool stopDaemon(string daemon);
  bool addDaemon(Daemon d);
  vector<Daemon*> getDaemons();
  vector<string> tabComplete(string);
  void addGPU(string, int);
  int getCPower();
  vector<string> getGPUs();
  void setDefaultNetDomain();
  void addNetDomain(int);
  void listNetDomains();
  bool inNetDomain(int);
  bool getNetRoute(string ip);
  void reset();
  int FSgetFree();
  void FSsetMax(int);
};

std::string ab64_encode(char const* bytes_to_encode, unsigned int in_len);
std::string ab64_decode(std::string const& encoded_string);
int explode(std::string line,std::string delimiter,std::vector<std::string> &store);
string vhash(string s);
string realpath(string path);

extern bool debug_on;
extern vector<VM> vPC;
extern vector<VM*> shell;
extern vector<RT> vhashes;
extern std::map<int, std::string> netd_map;

extern lua_State *lua;      // Lua state object
