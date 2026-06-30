#include <iostream>
#include <string>
#include<sstream>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;


  while(1){
    std::cout << "$ ";

    std::string input;
    std::getline(std::cin,input);
    std::stringstream stream(input);
    std::string token, segment;
    stream >> token >> segment;

    if(token=="exit"){
      exit(0);
    }
    else if(token=="echo"){
      std::cout<<segment<<"\n";
    }
    else{
      std::string command_failed=token+": command not found\n";
      std::cerr<<command_failed;

    }


  }
}
