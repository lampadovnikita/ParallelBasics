#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <cmath>
#include "mpi.h"

using namespace std;

const int MAIN_PROC_RANK = 0;

const int LOWER_PROC_RANK_TAG = 100;
const int UPPER_PROC_RANK_TAG = 101;

const int X_LOCAL_START_TAG = 102;
const int X_LOCAL_END_TAG   = 103;

const int LOWER_BOUND_TAG = 104;
const int UPPER_BOUND_TAG = 105;

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
const int Nx = 18;
const int Ny = 10;
const int Nz = 10;

// Размеры шага на сетке
const double hx = Dx / (Nx - 1);
const double hy = Dy / (Ny - 1);
const double hz = Dz / (Nz - 1);

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

void initGrid(double***& grid, int xLength, double xLocalStart)
{
	// Создаём массив и заполняем начальными значениями
	grid = new double** [xLength];
	for (int i = 0; i < xLength; i++) {
		grid[i] = new double* [Ny];

		for (int j = 0; j < Ny; j++) {
			grid[i][j] = new double[Nz];

			for (int k = 1; k < Nz - 1; k++) {
				grid[i][j][k] = phi0;
			}
		}
	}

	// Записываем краевые значения
	double xCurr;
	for (int i = 0; i < xLength; i++) {
		xCurr = toReal(xLocalStart, hx, i);

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

void deleteGrid(double*** grid, int xLocalLength)
{
	for (int i = 0; i < xLocalLength; i++) {
		for (int j = 0; j < Ny; j++) {
			delete[] grid[i][j];
		}
		delete[] grid[i];
	}
	delete[] grid;
}

// Считаем точность, как максимальное значение модуля отклонения
// от истинного значения функции
double getPrecisionMPI(double*** grid, int xLocalLength, double xLocalStart, int rootProcRank)
{
	// Значение ошибки на некотором узле
	double currErr;
	// Максимальное значение ошибки данного процесса
	double maxLocalErr = 0.0;

	for (int i = 1; i < xLocalLength - 1; i++) {
		for (int j = 1; j < Ny - 1; j++) {
			for (int k = 1; k < Nz - 1; k++) {
				currErr = abs(grid[i][j][k] -
					phi(toReal(xLocalStart, hx, i), toReal(yStart, hy, j), toReal(zStart, hz, k)));

				if (currErr > maxLocalErr) {
					maxLocalErr = currErr;
				}
			}
		}
	}
	
	// Максимальное значение ошибки по всем процессам
	double absoluteMax = -1;
	MPI_Reduce((void*)&maxLocalErr, (void*)&absoluteMax, 1, MPI_DOUBLE, MPI_MAX, rootProcRank, MPI_COMM_WORLD);

	return absoluteMax;
}

void jacobiMPI(double*** &grid1, int xLocalLength, int xLocalStartIndx, int lowerProcRank, int upperProcRank)
{
	MPI_Status status;

	// Значение сходимости для некоторого узла сетки
	double currConverg;
	// Максимальное значение сходимости по всем узлам на некоторой итерации
	double maxLocalConverg;

	// Флаг, показывающий, является ли эпсилон меньше любого значения сходимости для данного процесса
	char isEpsilonLower;

	const double hx2 = pow(hx, 2);
	const double hy2 = pow(hy, 2);
	const double hz2 = pow(hz, 2);

	// Константа, вынесенная за скобки
	double c = 1 / ((2 / hx2) + (2 / hy2) + (2 / hz2) + a);

	// Второй массив для того, чтобы использовать значения предыдущей итерации
	double*** grid2;
	// Просто копируем входной массив
	grid2 = new double** [xLocalLength];
	for (int i = 0; i < xLocalLength; i++) {
		grid2[i] = new double* [Ny];

		for (int j = 0; j < Ny; j++) {
			grid2[i][j] = new double[Nz];

			for (int k = 0; k < Nz; k++) {
				grid2[i][j][k] = grid1[i][j][k];
			}
		}
	}

	// Указатель на массив, из которого на некоторой итерации
	// берутся значения для расчёта
	double*** currentSourcePtr = grid1;
	// Указатель на массив, в который на некоторой итерации
	// Записываются новые значения
	double*** currentDestPtr = grid2;
	// Вспомогательный указатель для перемены указателей на массивы
	double*** tmpPtr;

	int messageLength = (Ny - 2) * (Nz - 2);
	double* messageBuf = new double[messageLength];

	// Флаг, который показывает, что нужно продолжать вычисления 
	char loopFlag = 1;

	while (loopFlag) {
		maxLocalConverg = 0.0;

		// Сначала вычисляем граничные значения
		// При i = 1
		for (int j = 1; j < Ny - 1; j++) {
			for (int k = 1; k < Nz - 1; k++) {
				// Первая дробь в скобках
				currentDestPtr[1][j][k] = (currentSourcePtr[2][j][k] + currentSourcePtr[0][j][k]) / hx2;

				// Вторая дробь в скобках
				currentDestPtr[1][j][k] += (currentSourcePtr[1][j + 1][k] + currentSourcePtr[1][j - 1][k]) / hy2;

				// Третья дробь в скобках
				currentDestPtr[1][j][k] += (currentSourcePtr[1][j][k + 1] + currentSourcePtr[1][j][k - 1]) / hz2;

				// Остальная часть вычисления нового значения для данного узла
				currentDestPtr[1][j][k] -= rho(currentSourcePtr[1][j][k]);
				currentDestPtr[1][j][k] *= c;

				// Сходимость для данного узла
				currConverg = abs(currentDestPtr[1][j][k] - currentSourcePtr[1][j][k]);
				if (currConverg > maxLocalConverg) {
					maxLocalConverg = currConverg;
				}
			}
		}

		// Если процесс должен отправить свой крайний слой с младшим значением x (не содержит слоя с x = 0)
		if (lowerProcRank != -1) {
			for (int j = 0; j < Ny - 2; j++) {
				for (int k = 0; k < Nz - 2; k++) {
					messageBuf[(Ny - 2) * j + k] = currentDestPtr[1][j + 1][k + 1];
				}
			}
			// Отправляем слой младшему процессу
			MPI_Send((void*)messageBuf, messageLength, MPI_DOUBLE, lowerProcRank, UPPER_BOUND_TAG, MPI_COMM_WORLD);
		}

		// Если процесс обрабатывает более одного слоя
		if (xLocalLength != 3) {
			// Вычисляем граничные значения
			// При i = xLength - 2
			for (int j = 1; j < Ny - 1; j++) {
				for (int k = 1; k < Nz - 1; k++) {
					// Первая дробь в скобках
					currentDestPtr[xLocalLength - 2][j][k] = (currentSourcePtr[xLocalLength - 1][j][k] +
						currentSourcePtr[xLocalLength - 3][j][k]) / hx2;

					// Вторая дробь в скобках
					currentDestPtr[xLocalLength - 2][j][k] += (currentSourcePtr[xLocalLength - 2][j + 1][k] +
						currentSourcePtr[xLocalLength - 2][j - 1][k]) / hy2;

					// Третья дробь в скобках
					currentDestPtr[xLocalLength - 2][j][k] += (currentSourcePtr[xLocalLength - 2][j][k + 1] +
						currentSourcePtr[xLocalLength - 2][j][k - 1]) / hz2;

					// Остальная часть вычисления нового значения для данного узла
					currentDestPtr[xLocalLength - 2][j][k] -= rho(currentSourcePtr[xLocalLength - 2][j][k]);
					currentDestPtr[xLocalLength - 2][j][k] *= c;

					// Сходимость для данного узла
					currConverg = abs(currentDestPtr[xLocalLength - 2][j][k] -
						currentSourcePtr[xLocalLength - 2][j][k]);
					
					if (currConverg > maxLocalConverg) {
						maxLocalConverg = currConverg;
					}
				}
			}		
		}

		// Если процесс должен отправить свой крайний слой со старшим значением x (не содержит слоя с x = Nx - 1)
		if (upperProcRank != -1) {
			for (int j = 0; j < Ny - 2; j++) {
				for (int k = 0; k < Nz - 2; k++) {
					messageBuf[(Ny - 2) * j + k] = currentDestPtr[xLocalLength - 2][j + 1][k + 1];
				}
			}
			// Отправляем слой старшему процессу
			MPI_Send((void*)messageBuf, messageLength, MPI_DOUBLE, upperProcRank, LOWER_BOUND_TAG, MPI_COMM_WORLD);
		}


		for (int i = 2; i < xLocalLength - 2; i++) {
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
					currConverg = abs(currentDestPtr[i][j][k] - currentSourcePtr[i][j][k]);
					if (currConverg > maxLocalConverg) {
						maxLocalConverg = currConverg;
					}

				}
			}
		}

		// Если процесс должен получить слой соседнего процесса для просчёта своего слоя с младшим значением x
		// (не содержит слоя с x = 0)
		if (lowerProcRank != -1) {
			MPI_Recv((void*)messageBuf, messageLength, MPI_DOUBLE, lowerProcRank, LOWER_BOUND_TAG, MPI_COMM_WORLD, &status);
			for (int j = 0; j < Ny - 2; j++) {
				for (int k = 0; k < Nz - 2; k++) {
					currentDestPtr[0][j + 1][k + 1] = messageBuf[(Ny - 2) * j + k];
				}
			}
		}

		// Если процесс должен получить слой соседнего процесса для просчёта своего слоя со старшим значением x
		// (не содержит слоя с x = Nx - 1)
		if (upperProcRank != -1) {
			MPI_Recv((void*)messageBuf, messageLength, MPI_DOUBLE, upperProcRank, UPPER_BOUND_TAG, MPI_COMM_WORLD, &status);
			for (int j = 0; j < Ny - 2; j++) {
				for (int k = 0; k < Nz - 2; k++) {
					currentDestPtr[xLocalLength - 1][j + 1][k + 1] = messageBuf[(Ny - 2) * j + k];
				}
			}
		}

		if(epsilon < maxLocalConverg) {
			isEpsilonLower = 1;
		}
		else {
			isEpsilonLower = 0;
		}

		// Применяем логичекую операцию ИЛИ над флагом сходимости между всеми процессами и помещаем результат во флаг цикла.
		// Таким образом, цикл завершится, когда у всех процессов сходимость будет меньше, чем эпсилон
		MPI_Reduce((void*)&isEpsilonLower, (void*)&loopFlag, 1, MPI_CHAR, MPI_BOR, MAIN_PROC_RANK, MPI_COMM_WORLD);
		MPI_Bcast((void*)&loopFlag, 1, MPI_CHAR, MAIN_PROC_RANK, MPI_COMM_WORLD);

		// Меняем местами указатели на массив-источник и массив-приёмник
		tmpPtr = currentSourcePtr;
		currentSourcePtr = currentDestPtr;
		currentDestPtr = tmpPtr;

		MPI_Barrier(MPI_COMM_WORLD);
	}


	// В итоге массив должен содержать значения последней итерации
	if (currentSourcePtr == grid1) {
		deleteGrid(grid2, xLocalLength);
	}
	else {
		tmpPtr = grid1;
		grid1 = currentSourcePtr;
		deleteGrid(tmpPtr, xLocalLength);
	}

	delete[] messageBuf;
}

int main(int argc, char** argv)
{
	// Количество процессов
	int procCount;

	// Ранг текущего процесса
	int myRank;

	// Ранг младшего процесса для текущего
	// (Содержит слой, который необходим для просчёта локального слоя с младшим по x значением)
	// Если -1, то такого процесса нет
	int myLowerProcRank;
	// Ранг старшего процесса для текущего
	// (Содержит слой, который необходим для просчёта локального слоя со старшим по x значением)
	// Если -1, то такого процесса нет
	int myUpperProcRank;

	// Глобальный индекс по x, с которого начинается область текущего процесса (включительно)
	int xMyLocalStartIndx;
	// Глобальный индекс по x, на котором заканчивается область текущего процесса (включительно)
	int xMyLocalEndIndx;

	// Статус возврата
	MPI_Status status;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
	MPI_Comm_size(MPI_COMM_WORLD, &procCount);

	// Если процессов больше, чем внутренних слоёв
	if (procCount > (Nx - 2)) {
		if (myRank == MAIN_PROC_RANK) {
			cout << "Too many processes!" << endl;
		}

		MPI_Abort(MPI_COMM_WORLD, 0);
	}

	if (myRank == MAIN_PROC_RANK) {
		cout << "Main process start send initial data..." << endl;

		int integerPart = (Nx - 2) / procCount;
		int remainder   = (Nx - 2) % procCount;

		// Размер партии (сразу вычисляем для главного процесса)
		int batchSize = integerPart + ((myRank < remainder) ? 1 : 0);


		myLowerProcRank = -1;
		myUpperProcRank = procCount == 1 ? -1 : 1;

		xMyLocalStartIndx = 1;
		xMyLocalEndIndx = xMyLocalStartIndx + batchSize - 1;

		// Младший процесс, для процесса-получателя
		int lowerProcRank;
		// Старший процесс, для процесса-получателя
		int upperProcRank;

		// Начало области для процесса-получателя
		int xLocalStartIndx = 1;
		// Конец области для процесса-получателя
		int xLocalEndIndx;

		// Рассылаем всем процессам необходимую информацию
		for (int destRank = 1; destRank < procCount; destRank++) {
			lowerProcRank = destRank - 1;
			MPI_Send((void*)&lowerProcRank, 1, MPI_INT, destRank, LOWER_PROC_RANK_TAG, MPI_COMM_WORLD);

			upperProcRank = destRank == procCount - 1 ? -1 : destRank + 1;
			MPI_Send((void*)&upperProcRank, 1, MPI_INT, destRank, UPPER_PROC_RANK_TAG, MPI_COMM_WORLD);

			xLocalStartIndx += batchSize;

			batchSize = integerPart + ((destRank < remainder) ? 1 : 0);

			MPI_Send((void*)&xLocalStartIndx, 1, MPI_INT, destRank, X_LOCAL_START_TAG, MPI_COMM_WORLD);
		
			xLocalEndIndx = xLocalStartIndx + batchSize - 1;
			MPI_Send((void*)&xLocalEndIndx, 1, MPI_INT, destRank, X_LOCAL_END_TAG, MPI_COMM_WORLD);
		}
	}
	else {
		// Остальные процессы получают от главного необходимую информацию
		MPI_Recv((void*)&myLowerProcRank, 1, MPI_INT, MAIN_PROC_RANK, LOWER_PROC_RANK_TAG, MPI_COMM_WORLD, &status);
		MPI_Recv((void*)&myUpperProcRank, 1, MPI_INT, MAIN_PROC_RANK, UPPER_PROC_RANK_TAG, MPI_COMM_WORLD, &status);

		MPI_Recv((void*)&xMyLocalStartIndx, 1, MPI_INT, MAIN_PROC_RANK, X_LOCAL_START_TAG, MPI_COMM_WORLD, &status);
		MPI_Recv((void*)&xMyLocalEndIndx,   1, MPI_INT, MAIN_PROC_RANK, X_LOCAL_END_TAG,   MPI_COMM_WORLD, &status);
	}

	for (int i = 0; i < procCount; i++) {
		if (i == myRank) {
			cout << "Proc rank: " << i << endl;
			cout << "Lower proc rank: " << myLowerProcRank << endl;
			cout << "Upper proc rank: " << myUpperProcRank << endl;
			cout << "x start: " << xMyLocalStartIndx << endl;
			cout << "x end:   " << xMyLocalEndIndx << endl;
			cout << endl;
		}
		MPI_Barrier(MPI_COMM_WORLD);
	}

	if (myRank == MAIN_PROC_RANK) {
		cout << "Main process finish sends initial data." << endl;
	}

	// Длина области текущего процесса
	int xMyLocalLength = xMyLocalEndIndx - xMyLocalStartIndx + 1;
	// Добавляем два слоя, которые вычисляют соседние процессы, чтобы производить сво вычисления на крайних слоях
	// (либо это константные значения внешних слоёв, если таковых процессов нет)
	xMyLocalLength += 2;

	// Инициализируем решётку
	double*** myGrid;
	initGrid(myGrid, xMyLocalLength, toReal(xStart, hx, xMyLocalStartIndx - 1));
	
	// Если процесс содержит первый внешний слой, где x - const (x = xStart)
	if (myLowerProcRank == -1) {
		// Записываем краевые значения
		// При i = xStart
		for (int j = 1; j < Ny - 1; j++) {
			for (int k = 1; k < Nz - 1; k++) {
				myGrid[0][j][k] = phi(xStart, toReal(yStart, hy, j), toReal(zStart, hz, k));
			}
		}
	}

	// Если процесс содержит второй внешний слой, где x - const (x = xEnd)
	if (myUpperProcRank == -1) {
		// Записываем краевые значения
		// При i = xEnd 
		for (int j = 1; j < Ny - 1; j++) {
			for (int k = 1; k < Nz - 1; k++) {
				myGrid[xMyLocalLength - 1][j][k] = phi(xEnd, toReal(yStart, hy, j), toReal(zStart, hz, k));
			}
		}
	}

	for (int i = 0; i < procCount; i++) {
		if (myRank == i) {
			cout << "Process " << myRank << " finish grid initialization." << endl;
		}
		MPI_Barrier(MPI_COMM_WORLD);
	}

	if (myRank == MAIN_PROC_RANK) {
		cout << "Calculation..." << endl;
	}

	double startTime = MPI_Wtime();

	// Производим вычисления по методу Якоби
	jacobiMPI(myGrid, xMyLocalLength, xMyLocalStartIndx, myLowerProcRank, myUpperProcRank);

	double endTime = MPI_Wtime();

	double elapsedTime = endTime - startTime;

	// Считаем общую точность
	double precsision = getPrecisionMPI(myGrid, xMyLocalLength, toReal(xStart, hx, xMyLocalStartIndx - 1), MAIN_PROC_RANK);
	if (myRank == MAIN_PROC_RANK) {
		cout << endl << "Precsicion: " << precsision << endl;
		cout << endl << "Calculation time: " << elapsedTime << " s."<< endl;
	}
	
	MPI_Finalize();

	deleteGrid(myGrid, xMyLocalLength);
}
