#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  std::cout << "$ ";

  std::string input;
  std::cin>>input;

  std::string command_failed=input+": command not found\n";
  std::cerr<<command_failed;

  "test";
}
