#include "../src/openxr/ActionExitHandoffPolicy.h"
#define CHECK(x) do {if(!(x)) return __LINE__;} while(false)
int main(){
  CHECK(kharvox::actionExitHandoffDelayMs(false,false)==150);
  CHECK(kharvox::actionExitHandoffDelayMs(true,false)==40);
  CHECK(kharvox::actionExitHandoffDelayMs(false,true)==40);
  CHECK(kharvox::actionExitHandoffDelayMs(true,true)==40);
  CHECK(kharvox::kGkExitDelayMs+kharvox::kExitBlendMs==240);
  CHECK(362-kharvox::kGkExitDelayMs-kharvox::kExitBlendMs>100);
  return 0;
}
