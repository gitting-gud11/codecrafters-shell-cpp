#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include<stack>
#include <sstream>
#include <string>
#include <vector>
#include<sys/wait.h>
#include<system_error>
#include<unistd.h>

#ifdef _WIN32
constexpr char os_pathsep=';';
#else
constexpr char os_pathsep=':';
#endif

constexpr char dir_pathsep=std::filesystem::path::preferred_separator;

enum quote_type{
  WHITESPACE=0,
  SINGLE=1,
  DOUBLE=2
};

typedef struct command_token{
  std::string data;
  size_t start_index;
  size_t end_index;
  quote_type variant;
} command_token;

std::string trim_leading_and_trailing_whitespace(const std::string & input){
  bool all_whitspace=true;

  for(auto &c:input){
    if(c!=' '){
      all_whitspace=false;
      break;
    }
  }

  if(all_whitspace) return "";

  size_t new_start, new_end=0;

  for(size_t i=0;i<input.size();++i){
    if(input[i]!=' '){
      new_start=i;
      break;
    }
  }

  //Post-increment is used because comparison is made then decrement occurs
  for(size_t i=input.size(); i-- >0 ;){
    if(input[i]!=' '){
      new_end=i;
      break;
    }
  }

  assert(new_start<=new_end);
  size_t new_length=(new_end-new_start)+1;

  return input.substr(new_start,new_length);

}

char get_token_delimiter(char c){
  assert(c!=' ');

  if(c=='\'' || c=='\"'){
    return c;
  }
  else{
    return ' ';
  }
}

inline bool delimiter_reached(char curr, char prev, char delimiter){

  //Backslash Special Case
  if(curr=='\"' && prev=='\\') return false;

  //Whitespace Regions Ends when a Quote Begins
  if((curr=='\"' || curr=='\'') && delimiter==' ') return true;

  return (curr==delimiter);
}

inline quote_type get_variant(char c){
  if(c==' '){
    return WHITESPACE;
  }
  else if(c=='\''){
    return SINGLE;
  }
  else if(c=='\"'){
    return DOUBLE;
  }
  else{
    //Input violates our assumptions
    assert(false);
  }
}


command_token build_command_token_quote_version(std::stack<std::pair<char,char>> & token_assembler, size_t end,size_t buffer_size){
  assert(!token_assembler.empty());

  command_token output;
  auto [last_char,delimiter]=token_assembler.top();
  assert(last_char==delimiter);

  std::string data;
  data.reserve(token_assembler.size());
  size_t start=(end-token_assembler.size())+1;

  while(!token_assembler.empty()){
    char letter=token_assembler.top().first;
    token_assembler.pop();
    data.push_back(letter);
  }

  output.data=std::string(data.rbegin(),data.rend());
  output.start_index=start;
  output.end_index=end;
  output.variant=get_variant(delimiter);

  return output;
}

command_token build_command_token_whitespace_version(std::stack<std::pair<char,char>> & token_assembler,size_t end,size_t buffer_size){
  assert(!token_assembler.empty());

  command_token output;
  auto[last_char,delimiter]=token_assembler.top();

  bool last_char_also_delimiter=(last_char=='\'' || last_char=='\"');

  size_t new_end=end;
  if(delimiter==' ' && (end!=buffer_size-1)){
    --new_end;
    token_assembler.pop();
  }

  assert(!token_assembler.empty());
  size_t start=(new_end-token_assembler.size())+1;

  std::string data;
  data.reserve(token_assembler.size());

  while(!token_assembler.empty()){
    char letter=token_assembler.top().first;
    token_assembler.pop();
    data.push_back(letter);
  }

  output.data=std::string(data.rbegin(),data.rend());
  output.start_index=start;
  output.end_index=new_end;
  output.variant=WHITESPACE;

  if(last_char_also_delimiter){
    assert(end!=buffer_size-1);
    token_assembler.push({last_char,last_char}); //Is this mutation kind of awkward?
  }

  return output;
}

std::vector<command_token> tokenize_quote_variants(const std::string & input){
  assert(!input.empty());
  //First product is the character, second product is the delimiter character
  std::stack <std::pair<char,char>> token_assembler;
  std::vector<command_token> tokens;
  
  for(size_t i {};i<input.size();++i){
    char curr=input[i];

    //Intermediate whitespace is ignored
    if(token_assembler.empty() && curr==' ') continue;

    //Beginning of a token
    if(token_assembler.empty()){
      char delimiter=get_token_delimiter(curr);

      token_assembler.push({curr,delimiter});

      continue;
    }

    auto [prev,delimiter]=token_assembler.top();
    token_assembler.push({curr,delimiter});

    if(delimiter_reached(curr,prev,delimiter)){
      command_token argument=((delimiter==' ') ?
       build_command_token_whitespace_version(token_assembler,i,input.size()) :build_command_token_quote_version(token_assembler,i,input.size()));
      tokens.push_back(argument);
    }

  }

  if(!token_assembler.empty()){
    auto [curr,delimiter]=token_assembler.top();
    assert(delimiter==' ');
    command_token argument=build_command_token_whitespace_version(token_assembler,input.size()-1,input.size());
    tokens.push_back(argument);
  }

  assert(token_assembler.empty());

  return tokens;
}

std::string filter_empty_quotes(const std::string & buffer){
  assert(buffer.size()!=0);
  if(buffer.size()==1){
    return buffer;
  }

  std::string output;
  output.reserve(buffer.size());

  size_t index=0;

  while(index<buffer.size()-1){
    if( (buffer[index]=='\'' || buffer[index]=='\"') && (buffer[index]==buffer[index+1])){
      index+=2;
    }
    else{
      output.push_back(buffer[index]);
      ++index;
    }
  }

  //Last character is not single or double quote
  if(index==buffer.size()-1){
    assert(buffer[index]!='\'' || buffer[index]!='\"');
    output.push_back(buffer[index]);
  }

  return output;

}

inline bool can_append_tokens(const command_token & prev_token,const command_token & curr_token){
  assert(curr_token.start_index>prev_token.end_index);

  bool adjacent=(curr_token.start_index-prev_token.end_index==1);

  bool valid_variants=((prev_token.variant==curr_token.variant) ||(prev_token.variant!=SINGLE && curr_token.variant!=SINGLE));

  return (adjacent && valid_variants);
}

std::vector<std::pair<size_t,size_t>> find_joined_command_token_intervals(const std::vector<command_token> & tokens){
  assert(tokens.size()!=0);

  if(tokens.size()==1){
    return {{0,0}};
  }

  std::vector<std::pair<size_t,size_t>> intervals;

  size_t start_idx=0;

  for(size_t i=1;i<tokens.size();++i){

    if(can_append_tokens(tokens[i-1],tokens[i])) continue; //Extend the interval

    intervals.push_back({start_idx,i-1});
    start_idx=i;
  }

  //Have the last interval to include
  intervals.push_back({start_idx,tokens.size()-1});
  return intervals;

}

void print_command_token_data(const std::vector<command_token> & tokens){

  for(auto &token:tokens){
    std::cout<<token.data<<"\n";
  }
  std::cout<<"-----------\n";
}

std::vector<std::string> append_command_tokens(const std::vector<std::pair<size_t,size_t>> & intervals,const std::vector<command_token> & tokens){
  assert(intervals.size()!=0);

  std::vector<std::string> arguments;
  arguments.reserve(std::max(arguments.capacity(),intervals.size()));

  for(auto [start,end]:intervals){
    std::string arg;
    for(size_t i=start;i<=end;++i){
      arg+=tokens[i].data;
    }
    arguments.push_back(arg);
  }

  return arguments;
}


std::vector<std::string> parse_input(const std::string & input){
  assert(!input.empty());

  std::vector<command_token> tokens=tokenize_quote_variants(input);
  assert(tokens.size()!=0);

  print_command_token_data(tokens);

  //Might want to modify this later. If I need the data to truly represent the indices, but should be fine for now
  for(auto &token:tokens){
    if(token.variant==WHITESPACE){
      token.data=filter_empty_quotes(token.data);
    }
  }
  //Might be able to logically WHITESPACE and DOUBLE as the same
  //Main difference is DOUBLE allows the whitespace characters
  //Think about it later
  for(auto &token:tokens){
    if(token.variant!=WHITESPACE){

      if(token.data.empty()) continue;
      
      assert(token.data[0]==token.data[token.data.size()-1]);
      assert(token.data[0]=='\'' || token.data[0]=='\"');
      assert(token.data.size()>=2);

      token.data=token.data.substr(1,(token.data.size()-2));
    }
  }

  std::vector<std::pair<size_t,size_t>> intervals=find_joined_command_token_intervals(tokens);

  std::vector<std::string> command_line_arguments=append_command_tokens(intervals,tokens);

  return command_line_arguments;
}


inline std::string get_command(const std::vector<std::string> & tokens){
  std::string command;

  if(tokens.size()!=0){
      command=tokens[0];
  }
  return command;
}

std::optional<std::string> find_exec_path(const std::string & file,const std::string & path){

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

inline void  echo_output(const std::vector<std::string> & tokens){
  size_t buffer_size=1; //Includes the newline character

  //Skip the "echo" command
  for(size_t i=1;i<tokens.size();++i){
    buffer_size+=tokens[i].size();

    if(i>1) ++buffer_size; //Include the delimiting whitespace
  }

  std::string buffer;
  buffer.reserve(buffer_size); //Modifies the capacity not the size

  for(size_t i=1;i<tokens.size();++i){buffer+=tokens[i];buffer.push_back(' ');} 
  buffer.push_back('\n');

  std::cout<<buffer;
}

void determine_type(const std::vector<std::string> & tokens,const std::string & path,const std::set<std::string> & builtins){
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

void run_program(const std::vector<std::string> & tokens){
  std::vector<const char*> argv(tokens.size()+1);

  for(size_t i {};i<tokens.size();++i){
    argv[i]=tokens[i].c_str();
  }
  argv[tokens.size()]=NULL;

  pid_t pid=fork();

  if(!pid){
  //Child process
    if(execvp(argv[0],const_cast<char* const*>(argv.data()))){
      std::string error_message=std::system_category().message(errno);

      std::cout<<error_message<<"\n";
    }

  }
  else{
    waitpid(pid,NULL,0);
  }
}

inline void print_path(const std::filesystem::path & input_path){

  std::cout<<(input_path.string())<<"\n";
}


void change_directory(const std::vector<std::string> & tokens){

  std::string updated_directory;

  if(tokens.size()>1){
    updated_directory=tokens[1];
  }

  //Might need to strip whitespace to make this more robust
  if(updated_directory.empty() ||(updated_directory=="~")){
    std::string home_path=std::string(getenv("HOME"));
    std::filesystem::current_path(home_path);
    return;
  }


  if(std::filesystem::is_directory(updated_directory)){
    std::filesystem::current_path(updated_directory);
  }
  else{
    std::cout<<"cd: "<<updated_directory<<": No such file or directory\n";
  }
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::string path=std::string(std::getenv("PATH"));

  std::set<std::string> builtins={"cd","echo","exit","pwd","type"};

  while(1){
    std::cout << "$ ";

    std::string line;
    std::getline(std::cin,line);

    std::string input=trim_leading_and_trailing_whitespace(line);

    if(input.empty()) continue;

    std::vector<std::string> tokens=parse_input(input);

    std::string command=get_command(tokens);

    if(command=="exit"){
      exit(0);
    }
    else if(command=="echo"){
      echo_output(tokens);
    }
    else if(command=="pwd"){
     std::filesystem::path working_path=std::filesystem::current_path();
     print_path(working_path);
    }
    else if(command=="type"){
      determine_type(tokens,path,builtins);
    }
    else if(command=="cd"){
      change_directory(tokens);
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
