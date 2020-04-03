#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <iostream>
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
const int Nx = 10;
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

void jacobi(double*** &grid1, int xLocalLength, int xLocalStartIndx, int lowerProcRank, int upperProcRank)
{
	MPI_Status status;

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

	do {
		maxConverg = 0.0;

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
				localConverg = abs(currentDestPtr[1][j][k] - currentSourcePtr[1][j][k]);
				if (localConverg > maxConverg) {
					maxConverg = localConverg;
				}
			}
		}

		if (lowerProcRank != -1) {
			for (int j = 0; j < Ny - 2; j++) {
				for (int k = 0; k < Nz - 2; k++) {
					messageBuf[(Ny - 2) * j + k] = currentDestPtr[1][j + 1][k + 1];
				}
			}

			MPI_Send((void*)messageBuf, messageLength, MPI_DOUBLE, lowerProcRank, UPPER_BOUND_TAG, MPI_COMM_WORLD);
		}

		// При i = xLength - 2
		for (int j = 1; j < Ny - 1; j++) {
			for (int k = 1; k < Nz - 1; k++) {
				// Первая дробь в скобках
				currentDestPtr[xLocalLength - 2][j][k] = (currentSourcePtr[xLocalLength - 1][j][k] + currentSourcePtr[xLocalLength - 3][j][k]) / hx2;

				// Вторая дробь в скобках
				currentDestPtr[xLocalLength - 2][j][k] += (currentSourcePtr[xLocalLength - 2][j + 1][k] + currentSourcePtr[xLocalLength - 2][j - 1][k]) / hy2;

				// Третья дробь в скобках
				currentDestPtr[xLocalLength - 2][j][k] += (currentSourcePtr[xLocalLength - 2][j][k + 1] + currentSourcePtr[xLocalLength - 2][j][k - 1]) / hz2;

				// Остальная часть вычисления нового значения для данного узла
				currentDestPtr[xLocalLength - 2][j][k] -= rho(currentSourcePtr[xLocalLength - 2][j][k]);
				currentDestPtr[xLocalLength - 2][j][k] *= c;

				// Сходимость для данного узла
				localConverg = abs(currentDestPtr[xLocalLength - 2][j][k] - currentSourcePtr[xLocalLength - 2][j][k]);
				if (localConverg > maxConverg) {
					maxConverg = localConverg;
				}
			}
		}

		if (upperProcRank != -1) {
			for (int j = 0; j < Ny - 2; j++) {
				for (int k = 0; k < Nz - 2; k++) {
					messageBuf[(Ny - 2) * j + k] = currentDestPtr[xLocalLength - 2][j + 1][k + 1];
				}
			}

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
					localConverg = abs(currentDestPtr[i][j][k] - currentSourcePtr[i][j][k]);
					if (localConverg > maxConverg) {
						maxConverg = localConverg;
					}

				}
			}
		}
		if (lowerProcRank != -1) {
			MPI_Recv((void*)messageBuf, messageLength, MPI_DOUBLE, lowerProcRank, LOWER_BOUND_TAG, MPI_COMM_WORLD, &status);
			for (int j = 0; j < Ny - 2; j++) {
				for (int k = 0; k < Nz - 2; k++) {
					currentDestPtr[0][j + 1][k + 1] = messageBuf[(Ny - 2) * j + k];
				}
			}
		}
		cout << "heh" << endl;

		if (upperProcRank != -1) {
			MPI_Recv((void*)messageBuf, messageLength, MPI_DOUBLE, upperProcRank, UPPER_BOUND_TAG, MPI_COMM_WORLD, &status);
			for (int j = 0; j < Ny - 2; j++) {
				for (int k = 0; k < Nz - 2; k++) {
					currentDestPtr[xLocalLength - 1][j + 1][k + 1] = messageBuf[(Ny - 2) * j + k];
				}
			}
		}

		// Меняем местами указатели на массив-источник и массив-приёмник
		tmpPtr = currentSourcePtr;
		currentSourcePtr = currentDestPtr;
		currentDestPtr = tmpPtr;
		cout << "lol" << endl;

		break;

	}
	while (maxConverg > epsilon);


	// В итоге массив должен содержать значения последней итерации
	if (currentSourcePtr == grid1) {
		deleteGrid(grid2, xLocalLength);
	}
	else {
		tmpPtr = grid1;
		grid1 = currentSourcePtr;
		deleteGrid(tmpPtr, xLocalLength);
	}
}

int main(int argc, char** argv)
{
	//Количество процессов
	int procCount;

	// Ранг процесса
	int myRank;
	// Ранг процесса, который содержит значения с меньшей координатой по оси x
	int myLowerProcRank;
	// Ранг процесса, который содержит значения с большей координатой по оси x
	int myUpperProcRank;

	// Границы обрабатываемой процессом области
	int xMyLocalStartIndx;
	int xMyLocalEndIndx;

	// Статус возврата
	MPI_Status status;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
	MPI_Comm_size(MPI_COMM_WORLD, &procCount);

	if (myRank == MAIN_PROC_RANK) {
		cout << "Main process start send initial data..." << endl;

		int integerPart = (Nx - 2) / procCount;
		int remainder   = (Nx - 2) % procCount;

		int batchSize = integerPart + ((myRank < remainder) ? 1 : 0);

		myLowerProcRank = -1;
		myUpperProcRank = procCount == 1 ? -1 : 1;

		xMyLocalStartIndx = 1;
		xMyLocalEndIndx = xMyLocalStartIndx + batchSize - 1;

		int lowerProcRank;
		int upperProcRank;

		int xLocalStartIndx = 1;
		int xLocalEndIndx;

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
		cout << "Main process finish send initial data." << endl;
	}

	int xMyLocalLength = xMyLocalEndIndx - xMyLocalStartIndx + 1;
	xMyLocalLength += 2;

	double*** myGrid;
	initGrid(myGrid, xMyLocalLength, toReal(xStart, hx, xMyLocalStartIndx - 1));
	
	if (myRank == MAIN_PROC_RANK) {
		// Записываем краевые значения
		// При i = 0
		for (int j = 1; j < Ny - 1; j++) {
			for (int k = 1; k < Nz - 1; k++) {
				myGrid[0][j][k] = phi(xStart, toReal(yStart, hy, j), toReal(zStart, hz, k));
			}
		}
	}

	if (myRank == procCount - 1) {
		// При i = Nx - 1 
		for (int j = 1; j < Ny - 1; j++) {
			for (int k = 1; k < Nz - 1; k++) {
				myGrid[xMyLocalLength - 1][j][k] = phi(xEnd, toReal(yStart, hy, j), toReal(zStart, hz, k));
			}
		}
	}
	cout << "Local grid calculation finished." << endl;

	jacobi(myGrid, xMyLocalLength, xMyLocalStartIndx, myLowerProcRank, myUpperProcRank);

	for (int i = 0; i < procCount; i++) {
		if (i == myRank) {
			cout << "========================================" << endl;
			cout << "Proc rank: " << myRank << endl;
			cout << "Proc local grid:" << endl;

			for (int x = 0; x < xMyLocalLength; x++) {
				cout << "x = " << xMyLocalStartIndx + x - 1 << endl;

				for (int y = 0; y < Ny; y++) {
					for (int z = 0; z < Nz; z++) {
						printf("%.5f ", myGrid[x][y][z]);
					}
					cout << endl;
				}
			}
		}

		MPI_Barrier(MPI_COMM_WORLD);
	}

	MPI_Finalize();
}
