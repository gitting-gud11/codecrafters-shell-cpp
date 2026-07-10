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
//Tries are ideal for autocomplete tasks make a file with an implementation for this data-structure when I get to the command completion phase
#define CMD_ARG_RESERVE 10

#ifdef _WIN32
constexpr char os_pathsep=';';
#else
constexpr char os_pathsep=':';
#endif

constexpr char dir_pathsep=std::filesystem::path::preferred_separator;

enum class parse_mode{
  UNQUOTED,
  SINGLE_QUOTE,
  DOUBLE_QUOTE,
  WHITESPACE,
  ESCAPE
};

enum class token_variant{
  UNQUOTED,
  SINGLE_QUOTE,
  DOUBLE_QUOTE
};

struct command_token{
  std::vector<std::optional<char>> data;
  size_t start_index;
  size_t end_index;
  token_variant variant;
};

std::string parse_mode_to_str(parse_mode mode){
  switch (mode){
    case (parse_mode::UNQUOTED):
      return "UNQUOTED";
    case (parse_mode::SINGLE_QUOTE):
      return "SINGLE_QUOTE";
    case (parse_mode::DOUBLE_QUOTE):
      return "DOUBLE_QUOTE";
    case (parse_mode::WHITESPACE):
      return "WHITESPACE";
    case (parse_mode::ESCAPE):
      return "ESCAPE"; 
  }
}

std::string token_variant_to_str(token_variant variant){
    switch (variant){
    case (token_variant::UNQUOTED):
      return "UNQUOTED";
    case (token_variant::SINGLE_QUOTE):
      return "SINGLE_QUOTE";
    case (token_variant::DOUBLE_QUOTE):
      return "DOUBLE_QUOTE";
    }
  }


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

// void print_command_token_data(const std::vector<command_token> & tokens){

//   for(auto &token:tokens){
//     std::cout<<token.data<<"\n";
//   }
//   std::cout<<"-----------\n";
// }


void print_command_token_data(const std::vector<command_token> & tokens){
  for(auto & token:tokens){

    for(auto &opt:token.data){
      if(opt.has_value()){
        std::cout<<opt.value();
      }
    }
    std::cout<<"\n";
  }
}

bool can_append_tokens(const command_token & a,const command_token & b){
  assert(a.start_index<b.start_index);
  assert(a.end_index<b.end_index);
  assert(a.end_index<b.start_index);

  size_t a_offset=(a.variant==token_variant::UNQUOTED ? 0 : 1);
  size_t b_offset=(b.variant==token_variant::UNQUOTED ? 0 : 1);

  return (((b.start_index-b_offset) - (a.end_index+a_offset)) ==1);

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

  //Include the last interval
  intervals.push_back({start_idx,tokens.size()-1});
  // std::cout<<"Intervals.size()="<<intervals.size()<<"\n";
  return intervals;
}

parse_mode get_resulting_state(char letter,parse_mode mode){

  if(letter=='\''){
   switch(mode){
    case parse_mode::UNQUOTED:
      return parse_mode::SINGLE_QUOTE;

    case parse_mode::SINGLE_QUOTE:
      return parse_mode::UNQUOTED;

    case parse_mode::DOUBLE_QUOTE:
      return parse_mode::DOUBLE_QUOTE;
    
    case parse_mode::WHITESPACE:
      return parse_mode::SINGLE_QUOTE; //Identical to unquoted case
    
      default:
        std::cerr<<"Invalid token sequence single-quote case\n";
        assert(false);
        break;
   }
  }
  else if(letter=='\"'){
   switch(mode){
    case parse_mode::UNQUOTED:
      return parse_mode::DOUBLE_QUOTE;

    case parse_mode::SINGLE_QUOTE:
      return parse_mode::SINGLE_QUOTE;

    case parse_mode::DOUBLE_QUOTE:
      return parse_mode::UNQUOTED;
    
      case parse_mode::WHITESPACE:
        return parse_mode::DOUBLE_QUOTE; //Identical to unquoted case
    
      default:
        std::cerr<<"Invalid token sequence double-quote case\n";
        assert(false);
        break;
   } 
  }
  else if(letter=='\\'){
    //Escape logic handled separately
    assert(mode!=parse_mode::ESCAPE);
    return ((mode==parse_mode::WHITESPACE) ? (parse_mode::UNQUOTED) : mode);
  }
  //Whitespace treated literally only in single and double quoted regions
  else if(letter==' '){
    return ((mode==parse_mode::SINGLE_QUOTE || mode==parse_mode::DOUBLE_QUOTE)) ? mode : parse_mode::WHITESPACE;
  }
  else{
    //Regular character encountered either preserve current mode or identify unquoted region if last character is whitespace
    return ((mode==parse_mode::WHITESPACE) ? (parse_mode::UNQUOTED) : mode);
  }
}

bool can_escape(char letter,parse_mode mode){
  assert(mode==parse_mode::UNQUOTED || mode==parse_mode::DOUBLE_QUOTE);

  std::set<char> double_quote_escape_characters={'\"','\\','$','`','\n'};

  return ((mode==parse_mode::UNQUOTED) || (double_quote_escape_characters.contains(letter)));
}

void print_token_regions(const std::vector<parse_mode> & list){
  for(auto elem:list){
    std::cout<<parse_mode_to_str(elem)<<",";
  }
  std::cout<<"\n";
}

std::vector<parse_mode> classify_token_regions(const std::string & input){
  std::vector<parse_mode> regions;
  regions.reserve(input.size());

  bool pending_escape=false;
  parse_mode mode=parse_mode::UNQUOTED;

  for(size_t i {};i<input.size();++i){
    char letter=input[i];

    //Determine if current character can be escaped
    if(pending_escape){
      assert(mode==parse_mode::UNQUOTED || mode==parse_mode::DOUBLE_QUOTE);
      if(!can_escape(letter,mode)){
        assert(input[i-1]=='\\');
        regions[i-1]=mode; //Prior backslash does not escape current character
      }
      regions.push_back(mode);
      pending_escape=false; 
      continue;
    }

    //Read potential escape character
    if(letter=='\\' && mode!=parse_mode::SINGLE_QUOTE){
      regions.push_back(parse_mode::ESCAPE);
      pending_escape=true;

      mode=get_resulting_state(letter,mode);
      continue;
    }

    parse_mode resulting_state=get_resulting_state(letter,mode);
    parse_mode letter_mode;

    //End of quoted region
    if(letter=='\'' && mode==parse_mode::SINGLE_QUOTE){
      letter_mode=mode;
    }
    //End of quoted region
    else if(letter=='\"' && mode==parse_mode::DOUBLE_QUOTE){
      letter_mode=mode;
    }
    else{
      letter_mode=resulting_state;
    }
    mode=resulting_state;
    regions.push_back(letter_mode);

  }
  return regions;
}

std::string preprocess_character_stream(const std::string & input,const std::vector<parse_mode> & classifier){
  assert(input.size()==classifier.size());
  assert(!input.empty());
  assert(input[0]!=' ');

  std::vector<std::optional<char>> characters(input.size());
  characters[0]=input[0];

  //Compress whitespace
  for(size_t i=1;i<input.size();++i){
    if(classifier[i-1]==parse_mode::WHITESPACE && classifier[i]==parse_mode::WHITESPACE){
      characters[i]=std::nullopt;
    }
    else{
      characters[i]=input[i];
    }
  }

  size_t index=0;
  while(index<input.size()-1){
    if(classifier[index]==parse_mode::ESCAPE){
      index+=2;
      continue;
    }

    //Both characters must be the same and in the same mode
    if((input[index]!=input[index+1]) || (classifier[index]!=classifier[index+1])){
      ++index;
      continue;
    }

    //Empty single quote
    if(input[index]=='\'' && classifier[index]==parse_mode::SINGLE_QUOTE){
      characters[index]=std::nullopt;
      characters[index+1]=std::nullopt;
      index+=2;
      continue;
    }

    //Empty double quote
    if(input[index]=='\"' && classifier[index]==parse_mode::DOUBLE_QUOTE){
      characters[index]=std::nullopt;
      characters[index+1]=std::nullopt;
      index+=2;
      continue;
    }

    //Condition not met
    ++index;
  }

  std::string processed;
  processed.reserve(input.size());

  for(auto &opt:characters){
    if(opt.has_value()){
      processed.push_back(opt.value());
    }
  }
  return processed;
} 


bool end_of_token (parse_mode current_mode,parse_mode encountered_mode){
  if(encountered_mode==parse_mode::WHITESPACE){
    return true;
  }
  if(encountered_mode==parse_mode::ESCAPE){
    return false;
  }
  return (current_mode!=encountered_mode);
}

//Could make it an optional to revise later, but later cases should not be reached
token_variant parse_mode_to_token_variant(parse_mode mode){
  switch (mode){
    case (parse_mode::UNQUOTED):
      return token_variant::UNQUOTED;
    case (parse_mode::SINGLE_QUOTE):
      return token_variant::SINGLE_QUOTE;
    case (parse_mode::DOUBLE_QUOTE):
      return token_variant::DOUBLE_QUOTE;
    default:
      break;
  }
  std::cerr<<"Invalid parse mode variant\n";
  assert(false);
}

command_token assemble_token(std::vector<std::optional<char>> & argument, size_t index,token_variant variant){
  assert(!argument.empty());
  size_t start_idx=(index-argument.size())+1;
  size_t end_idx=index;
  size_t offset=0;

  if(variant!=token_variant::UNQUOTED){
    assert(argument[0].has_value() && argument[argument.size()-1].has_value());
    assert(argument[0]==argument[argument.size()-1]);
    assert(argument[0]=='\'' || argument[0]=='\"');
    offset=1;
  }

  command_token token;
  token.data=std::vector<std::optional<char>>(argument.begin()+offset,argument.end()-offset);
  token.start_index=start_idx+offset;
  token.end_index=end_idx-offset;
  assert(token.start_index<=token.end_index);
  // std::cout<<"token.start_index="<<token.start_index<<" token.end_index="<<token.end_index<<" token_variant"<<token_variant_to_str(variant)<<"\n";
  token.variant=variant;

  argument.clear();

  return token;
}

std::vector<command_token> build_command_tokens(const std::string & input,const std::vector<parse_mode> & classifier){
  assert(!input.empty());
  assert(input.size()==classifier.size());
  std::vector<command_token> argument_tokens;
  argument_tokens.reserve(CMD_ARG_RESERVE);

  std::vector<std::optional<char>> argument; //Token variant can be determined by a conversion
  argument.reserve(input.size());
  std::optional<char> first_char=(classifier[0]==parse_mode::ESCAPE ? std::nullopt : std::make_optional<char>(input[0]));
  argument.push_back(first_char);

  parse_mode current_mode=(classifier[0]==parse_mode::ESCAPE ? parse_mode::UNQUOTED : classifier[0]);

  for(size_t i=1;i<input.size();++i){
    parse_mode encountered_mode=classifier[i];

    if(end_of_token(current_mode,encountered_mode)){
      if(!argument.empty()){
        command_token token=assemble_token(argument,i-1,parse_mode_to_token_variant(current_mode));
        argument_tokens.push_back(token);
      }
    }

    if(encountered_mode==parse_mode::WHITESPACE){
      current_mode=parse_mode::UNQUOTED;
      continue;
    }

    if(encountered_mode==parse_mode::ESCAPE){
      //Escape character can only be the first character in an unquoted region
      //Otherwise it is part of the current region which must be unquoted or double quoted
      if(argument.empty()){
        current_mode=parse_mode::UNQUOTED;
      }
      argument.push_back(std::nullopt);
      continue;
    }

    current_mode=encountered_mode;
    argument.push_back(input[i]);
  }

  if(!argument.empty()){
    command_token token=assemble_token(argument,input.size()-1,parse_mode_to_token_variant(current_mode));
    argument_tokens.push_back(token);
  }

  return argument_tokens;

}

std::vector<std::string> append_command_tokens(const std::vector<std::pair<size_t,size_t>> & intervals,const std::vector<command_token> & argument_tokens){
  std::vector<std::string> command_line_arguments;
  command_line_arguments.reserve(intervals.size());

  for(auto [start,end]:intervals){
    std::vector<std::optional<char>> data_stream;

    for(size_t i=start;i<=end;++i){
      const command_token & token=argument_tokens[i];
      data_stream.insert(data_stream.end(),token.data.begin(),token.data.end());
    }

    std::string arg;
    arg.reserve(data_stream.size());

    for(auto &opt:data_stream){
      if(opt.has_value()){
        arg.push_back(opt.value());
      }
    }

    command_line_arguments.push_back(arg);
  }

  return command_line_arguments;
}

std::vector<std::string> parse_input(const std::string & input){

  assert(!input.empty());

  std::vector<parse_mode> input_classifier=classify_token_regions(input);


  std::string canonical_input=preprocess_character_stream(input,input_classifier);
  std::vector<parse_mode> canonical_classifer=classify_token_regions(canonical_input);
  if(canonical_classifer.empty()) return {""};


  std::vector<command_token> argument_tokens=build_command_tokens(canonical_input,canonical_classifer);
  std::vector<std::pair<size_t,size_t>> intervals=find_joined_command_token_intervals(argument_tokens);
  std::vector<std::string> command_line_arguments=append_command_tokens(intervals,argument_tokens);

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

      std::cerr<<error_message<<"\n";
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
