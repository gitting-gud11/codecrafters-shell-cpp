#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

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

std::optional<std::string> find_path(std::string & file,std::string & path){

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

std:: string echo_output(std::vector<std::string> & tokens){
  std::string buffer;

  for(size_t i=1;i<tokens.size();++i){buffer+=tokens[i];buffer.push_back(' ');}

  return buffer;

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
      std::cout<<echo_output(tokens)<<"\n";
    }
    else if(command=="type"){
      std::string arg_type;
      if(tokens.size()>1){
        arg_type=tokens[1];
      }

      if(builtins.contains(arg_type)){
        std::cout<<arg_type<<" is a shell builtin\n";
        continue;
      }

      std::optional<std::string> returned_path=find_path(arg_type,path);
      if(returned_path.has_value()){
        std::cout<<arg_type<<" is "<<returned_path.value()<<"\n";
      }
      else{
        std::cout<<arg_type<<": command not found\n";
      }
      
    }
    else{
      std::string command_failed=command+": command not found\n";
      std::cout<<command_failed;

    }


  }
}
