#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;


  while(1){
    std::cout << "$ ";

    std::string input;
    std::cin>>input;
    if(input=="exit"){
      exit(0);
    }
    else{
      std::string command_failed=input+": command not found\n";
      std::cerr<<command_failed;
    }


  }
}
