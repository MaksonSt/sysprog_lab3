#include <iostream>
using namespace std;

int main() {
    int a = 1;
    int b = 0;

    if (a == 1)
        if (b == 1)
            cout << "a=1, b=1" << endl;
        else
            cout << "a=1, b!=1" << endl;

    return 0;
}
