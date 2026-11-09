#include <iostream>
#include <string>
using namespace std;

int main() {
    string name;
    int n;
    cout << "Ім'я: ";
    cin >> name;
    cout << "Скільки чисел підсумувати? ";
    cin >> n;

    double sum = 0;
    for (int i = 0; i < n; i++) {
        double x;
        cin >> x;
        sum += x;
    }
    cout << "Привіт, " + name + "!" << endl;
    cout << "Сума = " << sum << ", середнє = " << sum / n << endl;
    return 0;
}
