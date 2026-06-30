#include <iostream>
#include <string>
#include<sstream>
#include<vector>
#include<assert.h>


std::vector<std::string> parse_input(std::string & input){
  std::stringstream stream(input);

  std::vector<std::string> tokens;
  std::string token;
  while(std::getline(stream,token,' ')) {tokens.push_back(token);}

  return tokens;
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


  while(1){
    std::cout << "$ ";

    std::string input;
    std::getline(std::cin,input);

    std::vector<std::string> tokens=parse_input(input);

    std::string command;
    if(tokens.size()!=0){
      command=tokens[0];
    }

    if(command=="exit"){
      exit(0);
    }
    else if(command=="echo"){
      std::cout<<echo_output(tokens)<<"\n";
    }
    else{
      std::string command_failed=command+": command not found\n";
      std::cerr<<command_failed;

    }


  }
}
