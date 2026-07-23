#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <print>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <readline/readline.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
//Tries are ideal for autocomplete tasks make a file with an implementation for this data-structure when I get to the command completion phase
//Bash documentation https://www.gnu.org/software/bash/manual/bash.html#Introduction
#define CMD_ARG_RESERVE 10
#define PATH_CNT_RESERVE 2048 //Estimate on a bound of executables in the path
#define BUFFER_MAX 1024
#define DIGIT_MAX 64
#define OVERWRITE 1
#define BACKGROUND_OUTPUT_PADDING 24

constexpr char os_pathsep=':'; //Unix-Based Operating Systems

constexpr char dir_pathsep=std::filesystem::path::preferred_separator;

void print_errno_message(void){
      std::string error_message=std::system_category().message(errno);
      std::println(stderr,"{}",error_message);
      return;
}

inline std::optional<std::string> get_nth_token(const std::vector<std::string> & tokens,size_t n){
  return (n<tokens.size()) ? (std::make_optional<std::string>(tokens[n])) : std::nullopt;
}

namespace JobsManager{

  enum class process_state{
    RUNNING,
    STOPPED,
    TERMINATED
  };

  struct job_info{
    std::string running_prompt;
    std::string prompt;
    pid_t process_index;
    size_t job_index;
    process_state mode;
    std::optional<int> exit_code;
    std::optional<int> received_signal;
    bool updated_state;

    job_info(){
      running_prompt="";
      prompt="";
      process_index=0;
      job_index=0;
      mode=process_state::RUNNING;
      exit_code=std::nullopt;
      received_signal=std::nullopt;
      updated_state=false;
    }

    job_info(const std::string & running_line,const std::string & line,pid_t pid,size_t index){
      running_prompt=running_line;
      prompt=line;
      process_index=pid;
      job_index=index;
      mode=process_state::RUNNING;
      exit_code=std::nullopt;
      received_signal=std::nullopt;
      updated_state=false;

    }
    //Consider moving reseting the updated_state to method or free function
    //Consider moving printing to a method
  };

  std::map<size_t,job_info> job_table;
  std::priority_queue<size_t,std::vector<size_t>,std::greater<size_t>> job_assigner;
  std::list<size_t> job_history;

  size_t find_job_number(void){
    if(job_assigner.empty()){
      return job_table.size()+1;
    }
    else{
      size_t smallest_index=job_assigner.top();
      job_assigner.pop();
      return smallest_index;
    }
  }

  void register_process(const std::string & running_line,const std::string & line,pid_t pid){
    size_t job_number=find_job_number();
    job_info process_context(running_line,line,pid,job_number);
    job_table[job_number]=process_context;
    job_history.push_front(job_number);
    std::println("[{}] {}",job_number,pid);
    
  }

  //Think about print formatting a bit later
  void manage_and_update_job(job_info & child_process){
    if(child_process.mode==process_state::TERMINATED) return; //Do not attempt to reap a process multiple times

    int status;
    pid_t return_pid=waitpid(child_process.process_index,&status,WNOHANG);
    if(return_pid==-1){
      print_errno_message();
    }
    else if(return_pid==0){
      //Child process state unchanged
    }
    else if(return_pid==child_process.process_index){
      //Child Process state updated
      child_process.updated_state=true;

      if(WIFEXITED(status)){
        child_process.mode=process_state::TERMINATED;
        child_process.exit_code=WEXITSTATUS(status);
      }

      if(WIFSTOPPED(status)){
        child_process.mode=process_state::STOPPED;
      }

      if(WIFCONTINUED(status)){
        child_process.mode=process_state::RUNNING;
      }

      if(WIFSIGNALED(status)){
        child_process.mode=process_state::TERMINATED;
        int received_signal=WTERMSIG(received_signal);
        child_process.received_signal=received_signal;
      }
      
    }
    else{
      std::println(stderr,"Unmatched return_pid case");
    }

  }

  char get_job_marker (size_t job_index){
    assert(!job_history.empty());

    if(job_index==job_history.front()){
      return '+';
    }
    else if((job_history.size()>1) && (job_index==(*std::next(job_history.begin())))){
      return '-';
    }
    else{
      return ' ';
    }
  }

  void print_terminated_job(job_info & queried_job){
    assert(queried_job.mode==process_state::TERMINATED);

    if(queried_job.exit_code.has_value()){
      int exit_code=queried_job.exit_code.value();

      if(exit_code){
        std::string exit_string="Exit"+(std::to_string(exit_code));
        int padding=BACKGROUND_OUTPUT_PADDING-exit_string.size();
        std::println("[{}]{}  {}{}",queried_job.job_index,get_job_marker(queried_job.job_index),(exit_string+std::string(padding,' ')),
        queried_job.prompt);
      }
      else{
        std::println("[{}]{}  {}{}",queried_job.job_index,get_job_marker(queried_job.job_index),("Done"+std::string(20,' ')),
        queried_job.prompt);
      }

      return;
    }
    //Process terminated from a signal
    assert(queried_job.received_signal.has_value());
    int term_signal=queried_job.received_signal.value();
    //strsignal is not thread-safe though shell manages concurrency with processes
    std::string signal_str=(std::string(strsignal(term_signal)))+": "+std::to_string(term_signal);
    int padding=BACKGROUND_OUTPUT_PADDING-signal_str.size();
    if(padding <0) padding=0;
    std::println("{}{}",signal_str,queried_job.prompt);
  }

  void print_job(job_info & queried_job){
    const process_state current_mode=queried_job.mode;

    //Pad to 24 characters
    switch (current_mode){
      case (process_state::RUNNING):{
        std::println("[{}]{}  {}{}",queried_job.job_index,get_job_marker(queried_job.job_index),("Running"+std::string(17,' ')),
        queried_job.running_prompt);
        break;
      }
      case (process_state::STOPPED):{
        std::println("[{}]{}  {}{}",queried_job.job_index,get_job_marker(queried_job.job_index),("Stopped"+std::string(17,' ')),
        queried_job.prompt);
        break;
      }
      default:{
        //Terminated Case
        print_terminated_job(queried_job);
        break;
      }
    }
  }

  void refresh_jobs(void){
    assert(job_table.size()==job_history.size());
    
    if(job_table.empty()) return;

    std::set<size_t> terminated_jobs;

    for(auto iter=job_table.begin();iter!=job_table.end();iter++){
      auto & job_entry=iter->second;
      assert(iter->first==job_entry.job_index);
      manage_and_update_job(job_entry);
      if(job_entry.updated_state){
        print_job(job_entry);
        job_entry.updated_state=false;
      }

      if(job_entry.mode==process_state::TERMINATED){
        terminated_jobs.insert(job_entry.job_index);
      }
    }

    auto job_table_predicate=[&](const std::pair<size_t,job_info> & table_entry){return terminated_jobs.contains(table_entry.first);};
    auto job_history_predicate=[&](size_t job_index){return terminated_jobs.contains(job_index);};
    std::erase_if(job_table,job_table_predicate);
    std::erase_if(job_history,job_history_predicate);

    for(auto iter=terminated_jobs.begin();iter!=terminated_jobs.end();iter++){
      size_t terminated_job_index=(*iter);
      job_assigner.push(terminated_job_index);
    }
    return;

  }

  std::optional<size_t> find_job_by_specifier(std::string & token){
    size_t offset=1;
    if(token[0]!='%'){
      --offset;
      std::println(stderr,"-bash: jobs: warning: {}: job specification requires leading `%'",token);
    }

    bool well_formed=true;
    for(size_t i=offset;i<token.size();++i){
      if(!isdigit(token[i])){
        well_formed=false;
        break;
      }
    }

    if(!well_formed || job_history.empty()) return std::nullopt; //Query cannot resolve if no background jobs

    if(token=="%") return (job_history.front());

    size_t job_index=std::stoull(std::string(token.begin()+offset,token.end()));

    if(job_table.contains(job_index)){
      return job_index;
    }
    else{
      return std::nullopt;
    }
  }

  void print_all_running_jobs(void){
    //Map is sorted ensures processes are printed in order with respect to their job number
    for(auto iter=job_table.begin();iter!=job_table.end();iter++){
      //job number information encoded in the job_type struct
      job_info & queried_job=iter->second;
      manage_and_update_job(queried_job);
      print_job(iter->second);
      if(queried_job.updated_state) queried_job.updated_state=false; //Might want to wrap this into a method
    }
  }

  void list_jobs(std::vector<std::string> & tokens){
    if(tokens.size()==1){
      //Prompt was jobs
      JobsManager::print_all_running_jobs();
      return;
    }
    //Loop across each argument token
    for(size_t i=1;i<tokens.size();++i){
      std::optional<size_t> found_job_index=find_job_by_specifier(tokens[i]);
      if(found_job_index.has_value()){
        job_info & queried_job=job_table[found_job_index.value()];
        manage_and_update_job(queried_job);
        print_job(queried_job);
        if(queried_job.updated_state) queried_job.updated_state=false;
      }
      else{
          std::println(stderr,"-bash: jobs: {}: no such job",tokens[i]);
      }
    }
  }
};

namespace AutoComplete{
  //Maybe pull out the struct stuff and make it a class? Add a constructor which contains the data that I want
  //Trie for the path seems quite similar
  //Look into path being dynamic/nonconstant and modifying node struct to facilitate it
  struct node{
    std::optional<std::string> data;
    std::map<char,std::unique_ptr<node>> children;
  };

  std::string path=getenv("PATH"); //need to make dynamic since could be modified
  const std::vector<std::string> builtins={"cd","complete","declare","echo","exit","jobs","history","pwd","type"};

  std::map<std::string,std::string> custom_completer;
  int custom_start_invoke, custom_end_invoke=0; //Support the custom generator interaction with Readline API
  std::unique_ptr<node> Trie=std::make_unique<node>();

  void insert(const std::string & text){
    node* iterator=Trie.get();

    for(auto &letter:text){
      auto & children=iterator->children;

      if(!children.contains(letter)){
        children[letter]=std::make_unique<node>();
      }
      iterator=children[letter].get();
    }

    iterator->data=text;
  }

  void dfs_extract_matches(node* curr,std::vector<std::string> & matches){
    assert(curr!=nullptr);
    if(curr->data.has_value()){
      matches.push_back(curr->data.value());
    }

    auto & children=curr->children;

    for(auto iter=children.begin();iter!=children.end();++iter){
      node* child=iter->second.get();
      dfs_extract_matches(child,matches);
    }
  }

  std::vector<std::string> find_matches(const std::string & text){
    node* iterator=Trie.get();

    for(auto & letter:text){
      auto & children=iterator->children;
      
      if(!children.contains(letter)){
        return {}; //No match
      }
      iterator=children[letter].get();
    }
    std::vector<std::string> matches;
    matches.reserve(PATH_CNT_RESERVE);
    dfs_extract_matches(iterator,matches);
    return matches;
  }

  char* command_generator(const char* text,int state){
    static int index;
    static std::vector<std::string> matches;
    static std::string text_str;

    if(!state){
      index=0;
      matches.clear();
      text_str=text;
      matches=find_matches(text_str);
    }

    char * match=((index<matches.size()) ? (strdup(matches[index++].data())) : NULL); //Post-fix increment to fetch the corect index for the next call

    return match;
  }

  bool command_has_custom_completer(const char * line_buffer){
    assert(line_buffer!=NULL);
    size_t num_word_break_characters=strlen(rl_completer_word_break_characters);
    size_t buffer_length=strlen(line_buffer);
    std::string command;

    for(size_t i {};i<buffer_length;++i){
      bool split_char=false;

      for(size_t j {};j<num_word_break_characters;++j){
        if(line_buffer[i]==rl_completer_word_break_characters[j]){
          split_char=true;
          break;
        }
      }

      if(!split_char){
        command.push_back(line_buffer[i]);
      }
      else if(split_char && (!command.empty())){
        break;
      }

    }
    //First non-empty segment constructed represents the command
    //Structure of the loop accounts for offset before initial command ($ ) and potential opening quotes

    assert(!command.empty());
    return (custom_completer.contains(command));
  }

  std::array<std::string,3> get_completer_script_arguments(const char * line_buffer,int start,int end){
    size_t num_word_break_characters=strlen(rl_completer_word_break_characters);
    std::vector<std::string> words;
    words.reserve(CMD_ARG_RESERVE);
    std::string word;
    word.reserve(start);

    for(size_t i {};i<(size_t)start;++i){
      bool split_char=false;
      for(size_t j {};j<num_word_break_characters;++j){
        if(line_buffer[i]==rl_completer_word_break_characters[j]){
          split_char=true;
          break;
        }
      }
      if(!split_char){
        word.push_back(line_buffer[i]);
      }
      else if(split_char && (!word.empty())){
        words.push_back(word);
        word.clear();
      }
    }

    if(!word.empty()){
      words.push_back(word);
      word.clear();
    }

    assert(words.size()!=0);
    std::array<std::string,3> command_line_arguments;
    command_line_arguments[0]=words.front(); //command with custom completer
    command_line_arguments[1]=std::string(line_buffer+start,line_buffer+end);//word being completed
    command_line_arguments[2]=words.back();

    return command_line_arguments;
  }

  std::vector<std::string> construct_completions(std::string & result){
    std::vector<std::string> completions;
    std::stringstream stream(result);

    //Default behavior splits on newline character
    for(std::string segment;std::getline(stream,segment);) completions.push_back(segment);

    if(completions.empty()){
      return {}; //Indicates no match found. Ensures that terminal bell is rung
    }

    sort(completions.begin(),completions.end());
    return completions;

  }

  std::vector<std::string> perform_custom_completion(const char * line_buffer,int start,int end){
    assert(start!=0);

    std::array<std::string,3> command_line_arguments=get_completer_script_arguments(line_buffer,start,end);
    std::string path=custom_completer[command_line_arguments[0]];

    //access returns non-zero on failure
    if(access(path.c_str(),F_OK)){
      std::println(stderr,"path {} does not exist",path);
      return {};
    }

    if(access(path.c_str(),X_OK)){
      std::println(stderr,"path {} does not have executable permissions",path);
      return {};
    }

    std::string executeable=path+" "+command_line_arguments[0]+" "+command_line_arguments[1]+" "+command_line_arguments[2];

    FILE* fp;
    int status;
    char buffer[BUFFER_MAX];
    std::string result;
    const char * executable_cstr=executeable.c_str();

    char int_buffer[DIGIT_MAX];
    snprintf(int_buffer,DIGIT_MAX,"%d",end);

    if(setenv("COMP_LINE",line_buffer,OVERWRITE)){
      print_errno_message();
      return {};
    }

    if(setenv("COMP_POINT",int_buffer,OVERWRITE)){
      print_errno_message();
      return {};
    }


    fp=popen(executable_cstr,"r");
    if(fp==NULL){
      print_errno_message();
      return {};
    }

    while(fgets(buffer,BUFFER_MAX,fp)!=NULL){
      result+=buffer;
    }

    status=pclose(fp);
    if(status==-1){
      print_errno_message();
    }

    return construct_completions(result);

  }

  //Text argument is not relevant. This function is defined to interact with readline API
  char* custom_generator(const char * text, int state){
    static int index;
    static std::vector<std::string> matches;

    if(!state){
      index=0;
      matches.clear();
      matches=perform_custom_completion(rl_line_buffer,custom_start_invoke,custom_end_invoke);
    }

    char * match=((index<matches.size()) ? (strdup(matches[index++].data())) : NULL); //Post-fix increment to fetch the corect index for the next call

    return match;

  }

  char** command_completion(const char * text,int start,int end){
    char ** matches=NULL;

    if(start==0){
      matches=rl_completion_matches(text,command_generator);
      return matches;
    }

    if(!custom_completer.empty()){
      //Try the custom completer
      if(command_has_custom_completer(rl_line_buffer)){
        custom_start_invoke=start;
        custom_end_invoke=end;
        matches=rl_completion_matches(text,custom_generator);
      }
      else{
        matches=rl_completion_matches(text,command_generator);
      }
    }
    return matches;

  }

  void insert_path_executables(void){
    std::stringstream stream(path);
    std::string directory;

    while(std::getline(stream,directory,os_pathsep)){

      //Check if the process can read from the directory
      //Access returns 0 on success
      if(access(directory.c_str(),R_OK)) continue;
      std::filesystem::path curr_path{directory};

      for(auto const& dir_entry:std::filesystem::directory_iterator{curr_path}){
        auto file_path=dir_entry.path();

        if(!access(file_path.c_str(),X_OK)){
          insert(file_path.filename().string());
        }
      }
    }
  }

  void init_completion(void){
    for(auto & cmd:builtins){
      insert(cmd);
    }
    insert_path_executables();
    rl_attempted_completion_function=command_completion;
  }

};


//Think about error handling
namespace Shell_IO{
  std::vector<std::pair<int,int>> file_aliases;

  inline std::pair<std::string,std::string> redirection_tokenization(const std::string & command){
    assert(!command.empty());
    auto iter=std::find_if_not(command.begin(),command.end(),isdigit);

    if(iter==command.begin()){
      return std::pair{"",command};
    }
    else if(iter==command.end()){
      return std::pair{command,""};
    }
    else{
      return std::pair{std::string(command.begin(),iter),std::string(iter,command.end())};
    }
  }

  std::optional<std::pair<int,bool>> determine_redirection(const std::string & command){
    auto [file_descriptor,redirection_variant]=redirection_tokenization(command);

    if(redirection_variant.empty() || redirection_variant.size()>2) return std::nullopt;

    if(file_descriptor.empty() && redirection_variant=="<"){
      return std::pair{STDIN_FILENO,false};
    }
    //Check what is going on with append mode
    bool append_mode;
    if(redirection_variant==">"){
      append_mode=false;
    }
    else if(redirection_variant==">>"){
      append_mode=true;
    }
    else{
      return std::nullopt;
    }

    int descriptor=(file_descriptor.empty()) ? STDOUT_FILENO : (stoi(file_descriptor));
    return std::pair{descriptor,append_mode};
  }

  bool is_redirection(const std::string & command){
    return (determine_redirection(command).has_value());
  }

  //Might want to make this more robust in the future since redirection is not necessarily the suffix
  std::vector<std::string> filter_redirection_commands(const std::vector<std::string> & commands){
    auto iter=std::find_if_not(commands.begin(),commands.end(),[](const std::string & s){return !is_redirection(s);});

    if(iter==commands.end()){
      return commands;
    }
    else{
      return std::vector<std::string>(commands.begin(),iter);
    }

  }

  void redirect_channel(int fd,const bool append_flag,const char* filename){

    int flags=O_CREAT;

    if(fd){
      flags|=O_WRONLY;
    }
    else{

      flags|=O_RDONLY;
    }

    if(append_flag){
      flags|=O_APPEND;
    }
    else{
      flags|=O_TRUNC;
    }

    int save_fd;
    if((save_fd=dup(fd))==-1){
      print_errno_message();
      return;
    }

    int file_fd;
    if((file_fd=open(filename,flags,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH))==-1){
      print_errno_message();
      close(save_fd);
      return;
    }

    if(dup2(file_fd,fd)==-1){
      print_errno_message();
      close(file_fd);
      close(save_fd);
      return;
    }
    
    if(close(file_fd)==-1){
      print_errno_message();
      close(save_fd);
      return;
    }

    file_aliases.push_back(std::pair{save_fd,fd});
  } 
  void set_file_redirection(const std::vector<std::string> & commands){

    for(size_t i=0;i<commands.size()-1;++i){
      auto redirect_descriptor=determine_redirection(commands[i]);
      if(redirect_descriptor.has_value()){
        auto [fd,append_mode]=redirect_descriptor.value();
        redirect_channel(fd,append_mode,commands[i+1].data());
      }
    }
  }


  void restore_file_redirection(void){

    for(auto [old_fd,new_fd]:file_aliases){
      if(dup2(old_fd,new_fd)==-1){
        print_errno_message();
      }

      if(close(old_fd)==-1){
        print_errno_message();
      }
    }
    file_aliases.clear();
  }
};


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
        std::println(stderr,"Invalid token sequence single-quote case");
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
        std::println(stderr,"Invalid token sequence double-quote case");
        assert(false);
        break;
   } 
  }
  else if(letter=='\\'){
    //Escape logic handled separately
    assert(mode!=parse_mode::ESCAPE);
    return ((mode==parse_mode::WHITESPACE) ? (parse_mode::UNQUOTED) : mode);
  }
  else if(letter==' '){
    //Whitespace treated literally only in single and double quoted regions
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
  std::println(stderr,"Invalid parse mode variant");
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

    //Returns 0 to indicate file exists and have executable permissions
    if(!access(location.c_str(),F_OK|X_OK)){
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
  std::println("{}",buffer);
}

void determine_type(const std::vector<std::string> & tokens,const std::string & path){
  static const std::set<std::string> builtins(AutoComplete::builtins.begin(),AutoComplete::builtins.end());
  std::string arg_type;

  if(tokens.size()>1){
    arg_type=tokens[1];
  }

  if(arg_type.empty()) return;

  if(builtins.contains(arg_type)){
    std::println("{} is a shell builtin",arg_type);
    return;
  }

  std::optional<std::string> exec_path=find_exec_path(arg_type,path);
  if(exec_path.has_value()){
    std::println("{} is {}",arg_type,exec_path.value());
  }
  else{
    std::println("{}: not found",arg_type);
  }
}

void run_program(const std::vector<std::string> & tokens){
  std::vector<const char*> argv(tokens.size()+1);

  for(size_t i {};i<tokens.size();++i){
    argv[i]=tokens[i].c_str();
  }
  argv[tokens.size()]=NULL;

  pid_t pid;
  switch (pid=fork()){
    case -1:
      print_errno_message();
      break;
    case 0:
      //Child Process
      if(execvp(argv[0],const_cast<char* const*>(argv.data()))) print_errno_message();
      break;
    default:
      //Parent Process
      waitpid(pid,NULL,0);
      break;
  }
}

inline void print_path(const std::filesystem::path & input_path){

  std::println("{}",input_path.string());
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
    std::println(stderr,"cd: {}: No such file or directory",updated_directory);
  }
}

std::optional<std::pair<char,bool>> extract_completer_flag(const std::vector<std::string> & tokens){
  std::optional<std::string> flag_token=get_nth_token(tokens,1);

  if(flag_token.has_value()){
    std::string & flag_str=flag_token.value();
    assert(!flag_str.empty());

    if(flag_str.size()>2 || flag_str[0]!='-'){
      return std::pair<char,bool>{'\0',false};
    }
    else{
      return std::pair<char,bool>{flag_str[1],true};
    }
  }
  else{
    return std::nullopt;
  }
}


inline void print_registered_completion(const std::string & command,const std::string & path){
  std::println("complete -C \'{}\' {}",path,command);
}

void print_custom_completer_bindings(void){

  for(auto iter=AutoComplete::custom_completer.begin();iter!=AutoComplete::custom_completer.end();iter++){
    print_registered_completion(iter->first,iter->second);
  }
}

void configure_custom_completer(const std::vector<std::string> & tokens){
  assert(tokens[0]=="complete");

  std::optional<std::pair<char,bool>> flag_info=extract_completer_flag(tokens);

  if(!flag_info.has_value()){
    //complete has no additional arguments
    print_custom_completer_bindings();
    return;
  }

  auto [flag,well_formed]=flag_info.value();
  if(!well_formed){
    return;
  }

  switch (flag){
    case ('C'):{
      std::optional<std::string> path_optional=get_nth_token(tokens,2);
      std::optional<std::string> command_optional=get_nth_token(tokens,3);

      if(!path_optional.has_value() || !command_optional.has_value()) break;
      std::string & path=path_optional.value();
      std::string & command=command_optional.value();

      AutoComplete::custom_completer[command]=path;

      break;
    }    
    case ('p'):{
      std::optional<std::string> command_optional=get_nth_token(tokens,2);
      if(!command_optional.has_value()) break;
      std::string & command=command_optional.value();

      if(AutoComplete::custom_completer.contains(command)){
        print_registered_completion(command,AutoComplete::custom_completer[command]);
      }
      else{
        std::println(stderr,"complete: {}: no completion specification",command);
      }
      break;
    }
    case ('r'):{
      std::optional<std::string> command_optional=get_nth_token(tokens,2);
      if(!command_optional.has_value()) break;

      std::string & command=command_optional.value();
      if(AutoComplete::custom_completer.contains(command)) AutoComplete::custom_completer.erase(command);
      break;
    }
    default:
      std::println(stderr,"complete: {} invalid option",tokens[1]);
      break;
  }
}


void eval(std::string & line){

    std::vector<std::string> line_tokens=parse_input(line);

    Shell_IO::set_file_redirection(line_tokens); //Might want to rename from all_tokens
    //Think about rewrites a bit later

    std::vector<std::string> tokens=Shell_IO::filter_redirection_commands(line_tokens);

    std::string command=get_command(tokens);
    //Compile time hasing. Might add a dispatch table at the end. Issue with dispatch table is not all command evals have the same arguments
    if(command=="cd"){
      change_directory(tokens);
    }
    else if(command=="pwd"){
      std::filesystem::path working_path=std::filesystem::current_path();
      print_path(working_path);
    }
    else if(command=="echo"){
      echo_output(tokens);
    }
    else if(command=="jobs"){
      JobsManager::list_jobs(tokens);
    }
    else if(command=="history"){
      //Implement this
    }
    else if(command=="type"){
      determine_type(tokens,AutoComplete::path);
    }
    else if(command=="complete"){
      configure_custom_completer(tokens);
    }
    else if(command=="declare"){
      //Implement this
    }
    else if(command=="exit"){
      exit(0);
    }
    else{
      //Want to check if I can execute the program
      //Might change to execv since I already have the path
      std::optional<std::string> exec_path=find_exec_path(command,AutoComplete::path);
      if(exec_path.has_value()){
        run_program(tokens);
      }
      else{
        std::println(stderr,"{}: command not found",command);
      }

    }

}

inline bool is_background_job(const std::string & line){
  if(line.size()<2){
    return false;
  }
  return (line.back()=='&' && line[line.size()-2]==' ');
}


void execute_background_job(const std::string & line){
  std::vector<std::string> tokens=parse_input(line);

  std::vector<const char*> argv(tokens.size()+1);

  for(size_t i {};i<tokens.size();++i){
    argv[i]=tokens[i].c_str();
  }
  argv[tokens.size()]=NULL;

  if(execvp(argv[0],const_cast<char* const*>(argv.data()))) print_errno_message();

  exit(errno); //Might look into the printing format delay
}

void eval_background(const std::string & line){
  assert(line.size()>=2);
  std::string background_line=trim_leading_and_trailing_whitespace(std::string(line.begin(),line.end()-2));
  pid_t pid;
  switch((pid=fork())){
    case -1:
      print_errno_message();
      break;
    case 0:{
      //Child Process
      execute_background_job(background_line);
      break;
    }
    default:{
      //Parent Process
      JobsManager::register_process(line,background_line,pid); //fork returns the child process id to parent
      break;
    }

  }
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  AutoComplete::init_completion();
  rl_bind_key('\t',rl_complete);

  while(1){
    Shell_IO::restore_file_redirection();
    JobsManager::refresh_jobs();

    char * line_cstr=readline("$ ");

    std::string rawline(line_cstr);
    free(line_cstr);

    std::string line=trim_leading_and_trailing_whitespace(rawline);

    if(line.empty()) continue;

    if(is_background_job(line)){
      eval_background(line);
    }
    else{
      eval(line);
    }

  }
}
