#include <SkipList.h>
#include <string>
using std::string;
int main()
{
    SkipList<int, string> skip_list(8, 0.5);
    skip_list.put(1, "one");
    skip_list.put(5, "six");
    skip_list.put(2, "two");
    skip_list.put(5, "five");
    skip_list.put(4, "four");
    skip_list.put(7, "seven");
    skip_list.put(3, "three");
    skip_list.put(10, "ten");
    skip_list.put(8, "eight");
    skip_list.put(9, "nine");
    skip_list.print();
    skip_list.remove(1);
    skip_list.remove(5);
    skip_list.remove(7);
    skip_list.remove(10);
    skip_list.print();
    return 0;
}