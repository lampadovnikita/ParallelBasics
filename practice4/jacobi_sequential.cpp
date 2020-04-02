#include <iostream>
#include <cmath>
#include <vector>

using namespace std;

const double a = 10.0e5;

const double epsilon = 1.0e-8;

// Начальное приближение
const double phi0 = 0.0;

// Границы области
const double xStart = -1.0;
const double xEnd   =  1.0;

const double yStart = -1.0;
const double yEnd   =  1.0;

const double zStart = -1.0;
const double zEnd   =  1.0;

// Размеры области
const double Dx = xEnd - xStart;
const double Dy = yEnd - yStart;
const double Dz = zEnd - zStart;

// Количество узлов сетки
const int Nx = 8;
const int Ny = 8;
const int Nz = 8;

// Размеры шага на сетке
const double hx = Dx / (Nx - 1);
const double hy = Dy / (Ny - 1);
const double hz = Dz / (Nz - 1);

void printGrid(double*** grid)
{
    for (int i = 0; i < Nx; i++) {
        printf("x = %d\n", i);
        
        for (int j = 0; j < Ny; j++) {
            for (int k = 0; k < Nz; k++) {
                printf("%.5f ", grid[i][j][k]);
            }

            printf("\n\n");
        }
    } 
}

double phi(double x, double y, double z)
{
    return pow(x, 2) + pow(y, 2) + pow(z, 2);
}

double rho(double phiValue)
{
    return 6.0 - a * phiValue;
}

// Перевод из координаты сетки в настоящее значение
double toReal(double start, double step, int bias)
{
    return start + step * bias;
}

void initGrid(double*** &grid)
{
    grid = new double** [Nx];
    for (int i = 0; i < Nx; i++) {
        grid[i] = new double* [Ny];
        for (int j = 0; j < Ny; j++) {
            grid[i][j] = new double [Nz];
        }
    }

    // Записываем значения внутри области
    for (int i = 1; i < Nx - 1; i++) {
        for (int j = 1; j < Ny; j++) {
            for (int k = 1; k < Nz; k++) {
                grid[i][j][k] = phi0;
            }
        }
    }

    // Записываем краевые значения
    // При i = 0
    for (int j = 0; j < Ny; j++) {
        for (int k = 0; k < Nz; k++) {
            grid[0][j][k] = phi(xStart, toReal(yStart, hy, j), toReal(zStart, hz, k));
        }
    }
    // При i = Nx - 1 
    for (int j = 0; j < Ny; j++) {
        for (int k = 0; k < Nz; k++) {
            grid[Nx - 1][j][k] = phi(xEnd, toReal(yStart, hy, j), toReal(zStart, hz, k));
        }
    }

    double xCurr;
    for (int i = 1; i < Nx - 1; i++) {
        xCurr = toReal(xStart, hx, i);
        
        for (int k = 0; k < Nz; k++) {
            // При j = 0
            grid[i][0][k] = phi(xCurr, yStart, toReal(zStart, hz, k));
        }

        for (int k = 0; k < Nz; k++) {
            // При j = Ny - 1
            grid[i][Ny - 1][k] = phi(xCurr, yEnd, toReal(zStart, hz, k));
        }

        double yCurr;
        for (int j = 1; j < Ny - 1; j++) {
            yCurr = toReal(yStart, hy, j);

            // При k = 0
            grid[i][j][0] = phi(xCurr, yCurr, zStart);

            // При k = Nz - 1
            grid[i][j][Nz - 1] = phi(xCurr, yCurr, zEnd);
        }

    }
}

void deleteGrid(double*** grid) 
{
    for (int i = 0; i < Nx; i++) {
        for (int j = 0; j < Ny; j++) {
            delete[] grid[i][j];
        }
        delete[] grid[i];
    }
    delete[] grid;
}

void jacobi(double*** grid1)
{
    // Значение сходимости для некоторого узла сетки
    double localConverg;
    // Максимальное значение сходимости по всем узлам на некоторой итерации
    double maxConverg;

    const double hx2 = pow(hx, 2);
    const double hy2 = pow(hy, 2);
    const double hz2 = pow(hz, 2);

    // Константа, вынесенная за скобки
    double c = 1 / ((2 / hx2) + (2 / hy2) + (2 / hz2) + a);

    // Второй массив для того, чтобы использовать значения предыдущей итерации
    double*** grid2;
    initGrid(grid2);

    // Указатель на массив, из которого на некоторой итерации
    // берутся значения для расчёта
    double*** currentSourcePtr = grid1;
    // Указатель на массив, в который на некоторой итерации
    // Записываются новые значения
    double*** currentDestPtr = grid2;
    // Вспомогательный указатель для перемены указателей на массивы
    double*** tmpPtr;
    
    do {
        maxConverg = 0.0;
        
        for (int i = 1; i < Nx - 1; i++) {
            for (int j = 1; j < Ny - 1; j++) {
                for (int k = 1; k < Nz - 1; k++) {

                    // Первая дробь в скобках
                    currentDestPtr[i][j][k] = (currentSourcePtr[i + 1][j][k] + currentSourcePtr[i - 1][j][k]) / hx2;

                    // Вторая дробь в скобках
                    currentDestPtr[i][j][k] += (currentSourcePtr[i][j + 1][k] + currentSourcePtr[i][j - 1][k]) / hy2;

                    // Третья дробь в скобках
                    currentDestPtr[i][j][k] += (currentSourcePtr[i][j][k + 1] + currentSourcePtr[i][j][k - 1]) / hz2;

                    // Остальная часть вычисления нового значения для данного узла
                    currentDestPtr[i][j][k] -= rho(currentSourcePtr[i][j][k]);
                    currentDestPtr[i][j][k] *= c;

                    // Сходимость для данного узла
                    localConverg = abs(currentDestPtr[i][j][k] - currentSourcePtr[i][j][k]);
                    if (localConverg > maxConverg) {
                        maxConverg = localConverg;
                    }

                }
            }
        }

        // Меняем местами указатели на массив-источник и массив-приёмник
        tmpPtr = currentSourcePtr;
        currentSourcePtr = currentDestPtr;
        currentDestPtr = tmpPtr;
    }
    while (maxConverg > epsilon);


    // В итоге массив должен содержать значения последней итерации
    if (currentSourcePtr == grid1) {
        deleteGrid(grid2);
    }
    else {
        tmpPtr = grid1;
        grid1 = grid2;
        deleteGrid(tmpPtr);
    }
}

// Считаем точность, как максимальное значение модуля отклонения
// от истинного значения функции
double getPrecision(double*** &grid)
{
    double localErr;
    double maxErr = 0.0;

    for (int i = 1; i < Nx - 1; i++) {
        for (int j = 1; j < Ny - 1; j++) {
            for (int k = 1; k < Nz - 1; k++) {
                localErr = abs(grid[i][j][k] - 
                    phi(toReal(xStart, hx, i), toReal(yStart, hy, j), toReal(zStart, hz, k)));
                
                if (localErr > maxErr) {
                    maxErr = localErr;
                }
            }
        }
    }

    return maxErr;
}

int main(int argc, char** argv)
{   
    double*** grid;
    initGrid(grid);
    
    printGrid(grid);

    jacobi(grid);

    double precision = getPrecision(grid);
    printf("Precision = %f\n", precision);

    deleteGrid(grid);

    return 0;
}