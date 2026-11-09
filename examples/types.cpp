#include <iostream>
#include <string>
using namespace std;

double average(int a, int b) {
    return (a + b) / 2.0;
}

string grade(int score) {
    if (score >= 90) return "A";
    else if (score >= 75) return "B";
    else return "C";
}

int main() {
    cout << "7 / 2 = " << 7 / 2 << ", -7 % 3 = " << -7 % 3 << ", 7 / 2.0 = " << 7 / 2.0 << endl;
    cout << "average(3, 4) = " << average(3, 4) << ", 1.0 / 3 = " << 1.0 / 3 << endl;

    int n = 3.99;
    n += 2.5;
    bool big = n > 4;
    cout << "n = " << n << ", big = " << big << endl;

    string s = "abc";
    s += "def";
    cout << s + "!" << endl;

    int k = 5;
    int a = k++;
    int b = ++k;
    int let = - -a;
    cout << "k = " << k << ", a = " << a << ", b = " << b << ", let = " << let << endl;

    for (int score = 60; score <= 100; score += 15) {
        if (score == 75) continue;
        cout << score << " -> " << grade(score) << endl;
    }
    return 0;
}
