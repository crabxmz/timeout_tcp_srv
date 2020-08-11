#include"threadpool.h"
#include<iostream>

void c(int a)
{
    std::cout<<a<<std::endl;
}
void cc(int a, int aa)
{
    std::cout<<aa<<std::endl;
}
void ccc(int a, int aa, int aaa)
{
    std::cout<<aaa<<std::endl;
}
void cccc(int a, int aa, int aaa, int aaaa)
{
    std::cout<<aaaa<<std::endl;
}
int main()
{
    my_srv::threadPool thp(3);
    for (int i=0;i<1000;i++) {
        thp.enqueue(c, 1);
        thp.enqueue(cc, 1, 2);
        thp.enqueue(ccc, 1, 2, 3);
        thp.enqueue(cccc, 1, 2, 3, 4);
    }
    return 0;
}