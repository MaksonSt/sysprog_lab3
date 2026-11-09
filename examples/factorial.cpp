#include <iostream>
using namespace std;

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

int main() {
    for (int i = 1; i <= 5; i++) {
        cout << i << "! = " << factorial(i) << endl;
    }
    double avg = 7.5;
    bool ok = true;
    cout << "avg = " << avg << ", ok = " << ok << "\n";
    return 0;
}
