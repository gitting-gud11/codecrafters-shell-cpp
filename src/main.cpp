#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include<sys/wait.h>
#include<unistd.h>

#ifdef _WIN32
constexpr char os_pathsep=';';
#else
constexpr char os_pathsep=':';
#endif

constexpr char dir_pathsep=std::filesystem::path::preferred_separator;

std::vector<std::string> parse_input(std::string & input){
  std::stringstream stream(input);

  std::vector<std::string> tokens;
  std::string token;
  while(std::getline(stream,token,' ')) {tokens.push_back(token);}

  return tokens;
}


inline std::string get_command(std::vector<std::string> & tokens){
  std::string command;

  if(tokens.size()!=0){
      command=tokens[0];
  }
  return command;
}

std::optional<std::string> find_exec_path(std::string & file,std::string & path){

  std::stringstream stream(path);
  std::string buffer;

  while(std::getline(stream,buffer,os_pathsep)){

    if(!std::filesystem::is_directory(buffer)) continue;

    std::string location=buffer+dir_pathsep+file;
    std::filesystem::directory_entry entry{location};

    if(!entry.exists()) continue;

    std::filesystem::perms entry_permissions=entry.status().permissions();

    std::filesystem::perms executeable=((entry_permissions & std::filesystem::perms::owner_exec) | (
      entry_permissions & std::filesystem::perms::group_exec) | (entry_permissions & std::filesystem::perms::others_exec));

      switch (executeable)
      {
        case (std::filesystem::perms::none):
              break;
        default:
              return location;
      }

    }

    return std::nullopt;
}

inline void  echo_output(std::vector<std::string> & tokens){
  //Need to look into how I parse quote characters might need to delimit those
  std::string buffer;

  for(size_t i=1;i<tokens.size();++i){buffer+=tokens[i];buffer.push_back(' ');}

  std::cout<<buffer<<"\n";
}

void determine_type(std::vector<std::string> & tokens,std::string & path,std::set<std::string> & builtins){
  std::string arg_type;

  if(tokens.size()>1){
    arg_type=tokens[1];
  }

  if(arg_type.empty()) return;

  if(builtins.contains(arg_type)){
    std::cout<<arg_type<<" is a shell builtin\n";
    return;
  }

  std::optional<std::string> exec_path=find_exec_path(arg_type,path);
  if(exec_path.has_value()){
    std::cout<<arg_type<<" is "<<exec_path.value()<<"\n";
  }
  else{
    std::cout<<arg_type<<": not found\n";
  }
}

void run_program(std::vector<std::string> & tokens){
  std::vector<const char*> argv(tokens.size()+1);

  for(size_t i {};i<tokens.size();++i){
    argv[i]=tokens[i].c_str();
  }
  argv[tokens.size()]=NULL;

  pid_t pid=fork();

  if(!pid){
  //Child process
    execvp(argv[0],const_cast<char* const*>(argv.data()));

  }
  else{
    waitpid(pid,NULL,0);
  }
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::string path=std::string(std::getenv("PATH"));

  std::set<std::string> builtins={"echo","type","exit"};

  while(1){
    std::cout << "$ ";

    std::string line;
    std::getline(std::cin,line);

    std::vector<std::string> tokens=parse_input(line);

    std::string command=get_command(tokens);

    if(command=="exit"){
      exit(0);
    }
    else if(command=="echo"){
      echo_output(tokens);
    }
    else if(command=="type"){
      determine_type(tokens,path,builtins);
    }
    else{
      //Want to check if I can execute the program
      //Might change to execv since I already have the path
      std::optional<std::string> exec_path=find_exec_path(command,path);
      if(exec_path.has_value()){
        run_program(tokens);
      }
      else{
        std::string command_failed=command+": command not found\n";
        std::cout<<command_failed;

      }

    }


  }
}
