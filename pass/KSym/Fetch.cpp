#include "Project.h"

// fetch signatures
static const map<string, FetchDef> FILTERS({
    // most common ones
    {string("_copy_from_user"),             {1, 2, 0}},
    {string("call __get_user_${4:P}"),      {0, 1, -1}},
    {string("memdup_user"),                 {0, 1, -1}},
    // less common ones
    {string("__copy_user_flushcache"),      {1, 2, 0}},
    {string("__copy_user_nocache"),         {1, 2, 0}},
    });

// helpers
const FetchDef *Fetch::findDefMatch(const string &name) {
  auto i = FILTERS.find(name);
  if(i == FILTERS.end()){
    return nullptr;
  } else {
    return &i->second;
  }
}

