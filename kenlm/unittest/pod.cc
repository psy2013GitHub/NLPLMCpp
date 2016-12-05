
//#include "stdafx.h"  
#include <iostream>  
#include <type_traits>
#include <cstdio>
#include <cstring>
  
using namespace std;  
  
class A   
{   
public:  
    int x;  
    double y;  
};  
  
int main(int argc, char** argv)  
{  
    if (std::is_pod<A>::value)  
    {  
      std::cout << "before" << std::endl;  
      A a;  
      a.x = 8;  
      a.y = 10.5;  
      std::cout << a.x << std::endl;  
      std::cout << a.y << std::endl;  
  
      size_t size = sizeof(a);  
      char *p = new char[size];  
      memcpy(p, &a, size);  
      A *pA = (A*)p;  
  
      std::cout << "after" << std::endl;  
      std::cout << pA->x << std::endl;  
      std::cout << pA->y << std::endl;  
  
      delete p;  
      p = NULL;
    }  
    system("pause");  
    return 0;  
}  
