#include <iostream>
using namespace std;

bool isPrime(int n) {
    if (n < 2) return false;
    for (int d = 2; d * d <= n; d++) {
        if (n % d == 0) return false;
    }
    return true;
}

int main() {
    int count = 0;
    int n = 2;
    cout << "Перші 10 простих чисел:" << endl;
    while (true) {
        if (isPrime(n)) {
            cout << n << " ";
            count += 1;
            if (count == 10) break;
        }
        n++;
    }
    cout << endl;
    return 0;
}
