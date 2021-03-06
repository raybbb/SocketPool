#include "SocketPool/server_connection_pool.h"
#include "SocketPool/parse_xml.h"
#include "SocketPool/parse_json.h"
#include <iostream>
#include <stdlib.h>
#include <boost/lexical_cast.hpp>

using std::cout;
using std::endl;

ServerPool::ServerPool() {
  ParseXmlObj myXml("../config/socket.xml");
  vector<map<string, string> > result_array = myXml.GetChildDataArray("Config.Server"); 
  //  这段注释的代码是读取json配置文件的
  //ParseJsonObj myJson("../config/socket.json");
  //vector<map<string, string> > result_array = myJson.GetChildDataArray("Config.Server.Connection"); 
  for (auto key_value_map : result_array) {
    serverHost_ = key_value_map["IP"];
    serverPort_ = boost::lexical_cast<int>(key_value_map["Port"]);
    serverBacklog_ = boost::lexical_cast<int>(key_value_map["Backlog"]);

    SocketObjPtr conn(new SocketObj(serverHost_, serverPort_, serverBacklog_));
    //Listen()当中已经封装了bind
    if (conn->Listen()) {
      //在insert之前需要先把ip和端口号整合成key,先对端口号做处理,把int转为string
      char stringPort[10];
      snprintf(stringPort, sizeof(stringPort), "%d", serverPort_);
      string key = serverHost_ + "###" + stringPort;
      server_map.insert(make_pair(key, conn));
    } else {
      strErrorMessage_ = conn->ErrorMessage();
    }
  }
}
  
ServerPool::~ServerPool() {
  //析构函数做的工作是轮询map,让每个连接都close掉
  for (multimap<string, SocketObjPtr>::iterator sIt = server_map.begin(); sIt != server_map.end(); ++sIt) {
    sIt->second->Close();
  }
}

/**
 * Dump函数,专业debug30年!
 */
void ServerPool::Dump() const {
  printf("\n=====ServerPool Dump START ========== \n");
  printf("serverHost_=%s ", serverHost_.c_str());
  printf("serverPort_=%d ", serverPort_);
  printf("serverBacklog_=%d ", serverBacklog_);
  printf("serverPoolSize_=%d ", serverPoolSize_);
  printf("strErrorMessage_=%s\n ", strErrorMessage_.c_str());
  int count = 0;
  for (auto it = server_map.begin(); it!=server_map.end(); ++it) {
    printf("count==%d ", count);
    it->second->Dump();
    ++count;
  }
  printf("\n===ServerPool DUMP END ============\n");
}

bool ServerPool::Empty() const {
  return server_map.empty();
}

/**
 * 从server_map当中选取一个连接
 * host和port是筛选的端口号
 */
SocketObjPtr ServerPool::GetConnection(string host, unsigned port) {
  // get connection operation
  unique_lock<mutex> lk(resource_mutex);
  char stringPort[10];
  snprintf(stringPort, sizeof(stringPort), "%d", port);
  string key = host + "###" + stringPort;
  multimap<string, SocketObjPtr>::iterator sIt = server_map.find(key);
  if (sIt != server_map.end()) {
    SocketObjPtr ret = sIt->second;
    server_map.erase(sIt);
    return ret;
  }
}
  
/**
 * 释放特定的连接,就是把SocketObjPtr放回到map当中
 * 其实严格地来说这个函数名字不应该叫做释放,而是插入,因为除了插入从池当中取出来的连接之外
 * 用户还能插入构造的连接,但是这样做没有意义
 */
bool ServerPool::ReleaseConnection(SocketObjPtr conn) {
  unique_lock<mutex> lk(resource_mutex);
  pair<string, int> sockPair = conn->GetSock();
  char stringPort[10];
  snprintf(stringPort, sizeof(stringPort), "%d", sockPair.second);
  string key = sockPair.first + "###" + stringPort;
  server_map.insert(make_pair(key, conn));
  return true;
}

/**
 * 构造函数创建poolsize个连接错误时候用来打印错误信息
 */
string ServerPool::ErrorMessage() const {
  return strErrorMessage_; 
} 
