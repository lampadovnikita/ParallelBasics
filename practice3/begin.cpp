// Пункт №2-3

#include <omp.h>
#include <iostream>
#include <cmath>
#include <chrono>

using namespace std;

// Функция для моделирования долгих вычислений
float mock(float num)
{
    const int someConst = 35'000'000;

    for (int i = 0; i < someConst; i++) 
    {
        num += sin(num);
    }

    return num / someConst;
}

int main()
{
    float a[100], b[100];

    // Инициализация массива b
    for (int i = 0; i < 100; i++)
        b[i] = i;

    // Директива OpenMP для распараллеливания цикла
    #pragma omp parallel for
    for (int i = 0; i < 100; i++)
    {
        a[i] = mock(b[i]);
        b[i] = 2 * a[i];
    }

    float result = 0;

    // Далее значения a[i] и b[i] используются, например, так:
    #pragma omp parallel for reduction(+ : result)
    for (int i = 0; i < 100; i++)
        result += (a[i] + b[i]);

    cout << "Result = " << result << endl;

    return 0;
}