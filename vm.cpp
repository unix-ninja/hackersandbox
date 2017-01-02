#include "engine.h"

VM::VM (string hn)
{
  hostname = hn;
  uname = "VNX Kernel 0.1: Tue Dec 21 14:11 EST 2010; x86";
  users.push_back(User("root",""));
  ip = "0.0.0.0";
  cpu = "Octium II";
  cwd = "/";
  broken_counter = 0;
  has_root = false;
  fs_maxSize = 64;
  compute_power = 1;
}

/*
string VM::serialize()
{
  ostringstream num;
  string data;
  data += "vm:" + getHostname() + "\n";
  // save filesystem
  vector<File>::iterator f;
  for(f=filesystem.begin(); f!=filesystem.end(); f++)
  {
    string type;
    if(f->getType() == T_FOLDER) type = "d";
      else type = "f";
    data += "file:" + type + ",parent=" + f->getParent() + ",name=" + f->getName() + ",acl=" + f->getAcl() + ",owner=" + f->getOwner() + ",exec=" + f->getExec();
    if (!f->getOnDelete().empty()) data += ",on_delete=" + f->getOnDelete();
    data += ",content=" + ab64_encode(f->getContent().c_str(), f->getContent().length()) + "\n";
  }
  // save user chain
  {
    data.append("chain:");
    vector<User*>::iterator it;
    for(it=user_chain.begin(); it!=user_chain.end(); it++)
    {
      if(it!=user_chain.begin()) data.append(",");
      data.append((*it)->getName());
    }
    data.append("\n");
  }
  // save server properties
  data += "p:cwd=" + getCwd() + "\n";
  num << broken_counter;
  data += "p:bc=" + num.str() + "\n";
  if(has_root) data += "p:has_root=true\n";
    else data += "p:has_root=false\n";
  // save net domains
  if(net_domains.size())
  {
    data += "netd:";
    vector<int>::iterator ii;
    for(ii=net_domains.begin(); ii!=net_domains.end(); ii++)
    {
      if(ii!=net_domains.begin()) data += ",";
      num.str(string());
      num << *ii;
      data += num.str();
    }
    data += "\n";
  }
  return ab64_encode(data.c_str(), data.length());
}
*/

//string VM::serialize()
pt::ptree VM::serialize()
{
  ostringstream num;
  ostringstream output;
  pt::ptree json;
  string data;
  json.put("hostname", getHostname());
  // save filesystem
  vector<File>::iterator f;
  for(f=filesystem.begin(); f!=filesystem.end(); f++)
  {
    pt::ptree file;
    string type;
    if(f->getType() == T_FOLDER) type = "d";
      else type = "f";
    data += "type:" + type + ",parent=" + f->getParent() + ",name=" + f->getName() + ",acl=" + f->getAcl() + ",owner=" + f->getOwner() + ",exec=" + f->getExec();
    file.put("type", type);
    file.put("parent", f->getParent());
    file.put("name", f->getName());
    file.put("acl", f->getAcl());
    file.put("owner", f->getOwner());
    file.put("exec", f->getExec());
    if (!f->getOnDelete().empty())
    {
      file.put("on_delete", f->getOnDelete());
    }
    file.put("content", f->getContent());
    json.push_back(std::make_pair("file", file));
  }
  // save user chain
  json.put("chain", printUserChain());
  // save server properties
  json.put("cwd", getCwd());
  num << broken_counter;
  json.put("bc", num.str());
  if(has_root)
  {
    json.put("has_root", true);
  } else {
    json.put("has_root", false);
  }
  // save net domains
  if(net_domains.size())
  {
    data = "";
    vector<int>::iterator ii;
    for(ii=net_domains.begin(); ii!=net_domains.end(); ii++)
    {
      if(ii!=net_domains.begin()) data += ",";
      num.str(string());
      num << *ii;
      data += num.str();
    }
    json.put("netd", data);
  }
  //return ab64_encode(data.c_str(), data.length());
  pt::write_json(output, json, false);
  //return output.str();
  return json;
}

string VM::getCpu()
{
  return cpu;
}

string VM::getCwd()
{
  return cwd;
}

const vector<string> VM::getFiles(string parent)
{
  vector<string> files;
  for(int i=0; i < filesystem.size(); i++)
  {
    if(!files.size() && (filesystem[i].getType() == T_FILE) && (filesystem[i].getParent() + filesystem[i].getName() == parent))
    {
      files.push_back(filesystem[i].getName());
      return files;
    }

    if(filesystem[i].getParent() == parent)
    {
      files.push_back(filesystem[i].getName());
    }
  }
  return files;
}

File* VM::pFile (string s)
{
  if ( s.empty() ) return NULL;
  for(int i=0; i < filesystem.size(); i++)
  {
    if(filesystem[i].getParent()+filesystem[i].getName() == s ||
       filesystem[i].getParent()+filesystem[i].getName() == s+"/")
    {
      return &(filesystem[i]);
    }
  }
  return NULL;
}

string VM::getHostname ()
{
  return hostname;
}

string VM::getHint()
{
  return hint;
}

string VM::getIp ()
{
  return ip;
}

string VM::getUname ()
{
  return uname;
}

string VM::getUsername ()
{
  return user_chain.back()->getName();
}

User* VM::getUser (string u)
{
  for (int i=0; i<users.size(); i++ )
  {
    if (users[i].getName() == u) return &(users[i]);
  }
  return NULL;
}

int VM::getUserCount()
{
  return user_chain.size();
}

int VM::setCwd (string s)
{
  File *fp;
  // are we allowed to see these files?
  vector<string> store;
  explode(s, "/", store);
  string path = "/";
  for (int i=0; i<store.size(); i++)
  {
    if (store[i].empty()) continue;
    path += store[i] + "/";
    fp = pFile(path);
    if (!fp) {
      return E_NO_FILE;
    }
    if (!fp->isReadable())
    {
      return E_DENIED;
    }
  }
  for (int i=0; i<filesystem.size(); i++)
  {
    if ((filesystem[i].getType() == T_FOLDER) && (filesystem[i].getParent() + filesystem[i].getName() == s))
    {
      cwd = s;
      return E_OK;
    }
  }
  return E_NO_FILE;
}

void VM::setHint(string s)
{
  hint = s;
}

void VM::setIp (string s)
{
  ip = s;
}

void VM::setUname (string s)
{
  uname = s;
}

bool VM::addFile (File f)
{
  if (pcrecpp::RE("[^a-zA-Z0-9/.+_-]").PartialMatch(f.getName()))
  {
    return false;
  }
  if(filesystem.size() == fs_maxSize)
  {
    return false;
  }
  for (int i=0; i<filesystem.size(); i++)
  {
    if (filesystem[i].getParent() + filesystem[i].getName() == f.getParent() + f.getName())
    {
      filesystem[i] = f;
      return true;
    }
  }
  filesystem.push_back(f);
  return true;
}

bool VM::deleteFile (string name)
{
  string run_on_delete = "";
  for (int i=0; i<filesystem.size(); i++)
  {
    if ((filesystem[i].getParent() + filesystem[i].getName() == name) && filesystem[i].getType() == T_FILE)
    {
      run_on_delete = filesystem[i].getOnDelete()+"()";
      filesystem.erase(filesystem.begin()+i);
      if (!run_on_delete.empty())
      {
        luaL_dostring(lua, run_on_delete.c_str());
      }
      return true;
    }
  }
  return false;
}

bool VM::addUser (User u)
{
  if (pcrecpp::RE("[^a-zA-Z0-9.]").PartialMatch(u.getName()))
  {
    return false;
  }
  if(users.size() == 64)
  {
    return false;
  }
  for (int i=0; i<users.size(); i++)
  {
    if (users[i].getName() == u.getName())
    {
      users[i] = u;
      break;
    } else {
      users.push_back(u);
      break;
    }
  }
  return true;
}

bool VM::login ( string u, string p )
{
  if (u.empty()) return false;

  // loop through users
  for (int i=0; i<users.size(); i++)
  {
    // check for username match
    if (users[i].getName() == u)
    {
      // check for password match
      if (users[i].isPassword(p))
      {
        // add user to chain
        user_chain.push_back(&(users[i]));
        
        // update broken counter
        if (broken_counter) broken_counter++;

        // did we hit root?
        if (u == "root")
        {
          if (!has_root)
          {
            has_root = true;
            if (!on_root.empty())
            {
              luaL_dostring(lua, (on_root+"()").c_str());
            }
          }
          setCwd("/root/");
        } else {
          setCwd("/home/" + u + "/");
        }

        // let's run post login code
        if (!on_login.empty())
        {
          // escape the password
          size_t n = 0;
          while ( (n=p.find("\"",n)) != std::string::npos )
          {
              p.insert(n,"\\");
                n+=2;
          }
          // launch post login code and pass creds
          luaL_dostring(lua, (on_login+"(\""+u+"\",\""+p+"\")").c_str());
        }

        return true;
      } else {
        return false;
      }
    }
  }
  return false;
}

bool VM::login ( string u )
{
  if (u.empty()) return false;

  // loop through users
  for (int i=0; i<users.size(); i++)
  {
    // check for username match
    if (users[i].getName() == u)
    {
      // add user to chain
      user_chain.push_back(&(users[i]));
      if (u == "root")
      {
        if (!has_root)
        {
          has_root = true;
          if (!on_root.empty())
          {
            luaL_dostring(lua, (on_root+"()").c_str());
          }
        }
        setCwd("/root/");
      } else {
        setCwd("/home/" + u + "/");
      }
      // update broken counter
      if (broken_counter)
      {
        broken_counter++;
      }
      return true;
    }
  }
  return false;
}

int VM::logout ()
{
  int ret = user_chain.size();
  // if users in chain, log out of the last user
  if (user_chain.size())
  {
    string u = shell.back()->getUsername();
    user_chain.pop_back();
    if (broken_counter)
    {
      broken_counter--;
      if (broken_counter == 0 ) return -1;
    }
    // let's run post logout code
    if (!on_logout.empty())
    {
      luaL_dostring(lua, (on_logout+"(\""+u+"\")").c_str());
    }
  }
  // return user chain size
  return user_chain.size();
}

void VM::setBrokenCounter (int count)
{
  broken_counter = count;
}

bool VM::startBrokenCounter ()
{
  broken_counter = 1;
  return true;
}

bool VM::startDaemon (string daemon)
{
  for (int i=0; i<services.size(); i++)
  {
    // check is daemon exists
    if (services[i].name == daemon)
    {
      // start daemon
      services[i].started = true;
      return true;
    }
  }
  return false;
}

bool VM::stopDaemon (string daemon)
{
  for (int i=0; i<services.size(); i++)
  {
    // check is daemon exists
    if (services[i].name == daemon)
    {
      // stop daemon
      services[i].started = false;
      return true;
    }
  }
  return false;
}

bool VM::addDaemon (Daemon d)
{
  services.push_back(d);
  return true;
}

vector<Daemon*> VM::getDaemons ()
{
  vector<Daemon*> inetd;
  for (int i=0; i<services.size(); i++)
  {
    if (services[i].started) inetd.push_back(&(services[i]));
  }
  return inetd;
}

void VM::setRoot (bool hr)
{
  has_root = hr;
}

bool VM::hasRoot ()
{
  return has_root;
}

void VM::onRoot (string s)
{
  on_root = s;
}

void VM::onLogin (string s)
{
  on_login = s;
}

void VM::onLogout (string s)
{
  on_logout = s;
}

void VM::addGPU(string desc, int power)
{
  gpus.push_back(desc);
  compute_power += power;
}

vector<string> VM::getGPUs()
{
  return gpus;
}

int VM::getCPower()
{
  return compute_power;
}

vector<string> VM::tabComplete (string s)
{
  vector<string> matches;
  string filename;
  string path;
  string cwd = shell.back()->getCwd();
  
  // check to see if a path was given
  if (pcrecpp::RE("(.*/)").PartialMatch(s, &path))
  {
    s = s.substr(path.length());
    path = realpath(path);
    if(path.empty()) return matches;
  } else {
    // if no path, assign cwd
    path = cwd;
  }

  // sanitize tab input
  string in = s;
  s = "";
  for (int i=0; i<in.length(); i++)
  {
    s += "[";
    s += in[i];
    s += "]";
  }
  
  // loop through files
  for(int i=0; i < filesystem.size(); i++)
  {
    filename = filesystem[i].getName();
    // make sure file is in the working path
    if( filesystem[i].getParent() == path )
    {
      // does it match our search text?
      if (pcrecpp::RE(s + ".*").FullMatch(filename))
      {
        // make sure we have permission to see this file
        if (pFile(filesystem[i].getParent())->isReadable())
        {
          if (cwd == path)
          {
            matches.push_back(filesystem[i].getName());
          } else {
            matches.push_back(path + filesystem[i].getName());
          }
        }
      }
    }
  }
  return matches;
}

void VM::setDefaultNetDomain()
{
  if(ip != "0.0.0.0" && net_domains.size() == 0)
  {
    addNetDomain(1);
  }
}

void VM::addNetDomain(int domain)
{
  // only positive int IDs are allowed
  if(domain>0) net_domains.push_back(domain);
}

void VM::listNetDomains()
{
  for(int j=0; j<net_domains.size(); j++)
  {
    cout << ":netd+ " << net_domains[j] << endl;
  }
}

bool VM::inNetDomain(int domain)
{
  if(std::find(net_domains.begin(), net_domains.end(), domain) != net_domains.end())
    return true;
  return false;
}

bool VM::getNetRoute(string ip)
{
  for(int i=0; i<vPC.size(); i++)
  {
    if(vPC[i].getIp() == ip)
    {
      for(int j=0; j<net_domains.size(); j++)
      {
        if(vPC[i].inNetDomain(net_domains[j]))
        {
          return true;
        }
      }
    }
  }
  return false;
}

string VM::printUserChain()
{
  string data = "";
  vector<User*>::iterator it;
  for(it=user_chain.begin(); it!=user_chain.end(); it++)
  {
    if(it!=user_chain.begin()) data.append(",");
    data.append((*it)->getName());
  }
  return data;
}

void VM::reset()
{
  filesystem.clear();
  net_domains.clear();
  user_chain.clear();
}

int VM::FSgetFree()
{
  return fs_maxSize - filesystem.size();
}

void VM::FSsetMax(int fs_sz)
{
  fs_maxSize = fs_sz;
}

Daemon::Daemon (int p, string n, string e, string q)
{
  port = p;
  name = n;
  exec = e;
  query = q;
  started = false;
}

File::File () {}

File::File (string n, int t, string c, string e)
{
  if ( n != "/" )
  {
    if ( n.substr(n.length()-1) != "/" )
    {
      pcrecpp::RE("(.*/)").PartialMatch(n, &parent);
    } else {
      pcrecpp::RE("(.*/).*/").PartialMatch(n, &parent);
    }
    filename = n.substr(parent.length());
  }
  else
  {
    parent = "";
    filename = "/";
  }
  filetype = t;
  
  // set content
  if(!c.empty()) content = c;
  
  // set exec function
  if(!e.empty()) exec = e;
  
  //sanitize name
  if (t == T_FOLDER && filename.substr(filename.length()-1) != "/")
    filename += "/";
  
  // set permissions
  if (t == T_FILE && e.empty())
  {
    acl = "644";
  }
  else
  {
    acl = "755";
  }
  owner = "root";
  setuid = false;
}

string File::getAcl()
{
  return acl;
}

vector <bitset<3> > File::getBitset ()
{
  vector <bitset<3> > bset;
  int acl_u;
  int acl_g;
  int acl_o;
  
  acl_u = acl.at(0) - '0';
  acl_g = acl.at(1) - '0';
  acl_o = acl.at(2) - '0';

  bset.push_back(acl_u);
  bset.push_back(acl_g);
  bset.push_back(acl_o);
  return bset;
}

string File::getContent ()
{
  if(isReadable()) return content;
  return "";
}

string File::getExec ()
{
  return exec;
}

string File::getName ()
{
  return filename;
}

string File::getOwner ()
{
  return owner;
}

string File::getParent ()
{
  return parent;
}

bool File::setAcl (string acl)
{
  this->acl = acl;
  return true;
}

bool File::setOwner (string s)
{
  owner = s;
  return true;
}

string  File::getOnDelete ()
{
  return on_delete;
}

bool File::setOnDelete (string s)
{
  on_delete = s;
  return true;
}

bool File::isReadable ()
{
  string user = setuid ? owner : shell.back()->getUsername();
  vector <bitset<3> > bset = getBitset ();
  
  // root should ALWAYS be allowed access
  if (user == "root") { return true; }

  // world
  if ( bset[2][2] == 1 )
  {
    return true;
  } else 
  // owner
  if ( user == owner && bset[0][2] == 1 )
  {
    return true;
  }
  return false;
}

bool File::isWritable ()
{
  string user = setuid ? owner : shell.back()->getUsername();
  vector <bitset<3> > bset = getBitset ();
  // world
  if ( bset[2][1] == 1 )
  {
    return true;
  }
  // owner
  if ( user == owner && bset[0][1] == 1 )
  {
    return true;
  }
  return false;
}

bool File::isExecutable ()
{
  string user = setuid ? owner : shell.back()->getUsername();
  vector <bitset<3> > bset = getBitset ();
  // world
  if ( bset[2][0] == 1 )
  {
    return true;
  }
  // owner
  if ( user == owner && bset[0][0] == 1 )
  {
    return true;
  }
  return false;
}

int File::getType ()
{
  return filetype;
}

string File::drawAcl ()
{
  string acl("");
  vector <bitset<3> > bset = getBitset ();
  acl += getType() == T_FOLDER ? "d" : "-";
  acl += ( bset[0][2] == 1 ) ? "r" : "-";
  acl += ( bset[0][1] == 1 ) ? "w" : "-";
  if ( setuid == true )
  {
    acl += "s";
  } else {
    acl += ( bset[0][0] == 1 ) ? "x" : "-";
  }
  acl += bset[1][2] ? "r" : "-";
  acl += bset[1][1] ? "w" : "-";
  acl += bset[1][0] ? "x" : "-";
  acl += bset[2][2] ? "r" : "-";
  acl += bset[2][1] ? "w" : "-";
  acl += bset[2][0] ? "x" : "-";
  return acl;
}

void File::setSUID(bool b)
{
  setuid = b;
}

User::User (string u, string p)
{
  name = u;
  password = p;
  registerPassword(p);
}

string User::getName()
{
  return name;
}

bool User::isPassword (string s)
{
  if (password.empty()) return false;
  if (password == "*") return true;
  if (s == password) return true;
  return false;
}

bool registerPassword (string s)
{
  string h = vhash(s);
  for(int i=0; i<vhashes.size(); i++)
  {
    if (vhashes[i].hash == h) return true;
  }
  vhashes.push_back(RT(h,s));
  return true;
}

bool User::setPassword (string s)
{
  password = s;
  registerPassword(s);
  return true;
}

bool User::addMail (Mail m)
{
  inbox.push_back(m);
  return true;
}

bool User::deleteMail (int index)
{
  int sz = inbox.size();
  inbox.erase(inbox.begin()+index);
  if (inbox.size()<sz) return true;
  return false;
}

Mail* User::getMail (int index)
{
  if (index > inbox.size() || --index < 0 ) return NULL;
  return &(inbox[index]);
}

vector<Mail> User::listInbox ()
{
  return inbox;
}

int User::inboxSize()
{
  return inbox.size();
}

Mail::Mail(string t, string s, string f, string b)
{
  header_to = t;
  header_subject = s;
  header_from = f;
  msg_body = b;
}

string Mail::to()
{
  return header_to;
}

string Mail::subject()
{
  return header_subject;
}

string Mail::from()
{
  return header_from;
}

string Mail::body()
{
  return msg_body;
}
