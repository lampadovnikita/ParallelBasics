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
const int X_LOCAL_END_TAG = 103;

const double a = 10.0e5;

const double epsilon = 1.0e-8;

// Начальное приближение
const double phi0 = 0.0;

// Границы области
const double xStart = -1.0;
const double xEnd = 1.0;

const double yStart = -1.0;
const double yEnd = 1.0;

const double zStart = -1.0;
const double zEnd = 1.0;

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
	int xMyLocalStart;
	int xMyLocalEnd;

	// Статус возврата
	MPI_Status status; 

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
	MPI_Comm_size(MPI_COMM_WORLD, &procCount);

	if (myRank == MAIN_PROC_RANK)
	{
		cout << "Main process start..." << endl;

		int integerPart = (Nx - 2) / procCount;
		int remainder   = (Nx - 2) % procCount;

		int batchSize = integerPart + ((myRank < remainder) ? 1 : 0);

		myLowerProcRank = -1;
		myUpperProcRank =  1;

		xMyLocalStart = 1;
		xMyLocalEnd   = xMyLocalStart + batchSize - 1;

		int lowerProcRank;
		int upperProcRank;

		int xLocalStart = 1;
		int xLocalEnd;

		for (int destRank = 1; destRank < procCount; destRank++)
		{
			lowerProcRank = destRank - 1;
			MPI_Send((void*)&lowerProcRank, 1, MPI_INT, destRank, LOWER_PROC_RANK_TAG, MPI_COMM_WORLD);

			upperProcRank = destRank == procCount - 1 ? -1 : destRank + 1;
			MPI_Send((void*)&upperProcRank, 1, MPI_INT, destRank, UPPER_PROC_RANK_TAG, MPI_COMM_WORLD);

			xLocalStart += batchSize;

			batchSize = integerPart + ((destRank < remainder) ? 1 : 0);

			MPI_Send((void*)&xLocalStart, 1, MPI_INT, destRank, X_LOCAL_START_TAG, MPI_COMM_WORLD);

			xLocalEnd = xLocalStart + batchSize - 1;
			MPI_Send((void*)&xLocalEnd, 1, MPI_INT, destRank, X_LOCAL_END_TAG, MPI_COMM_WORLD);
			
		}
	}
	else
	{
		MPI_Recv((void*)&myLowerProcRank, 1, MPI_INT, MAIN_PROC_RANK, LOWER_PROC_RANK_TAG, MPI_COMM_WORLD, &status);
		MPI_Recv((void*)&myUpperProcRank, 1, MPI_INT, MAIN_PROC_RANK, UPPER_PROC_RANK_TAG, MPI_COMM_WORLD, &status);

		MPI_Recv((void*)&xMyLocalStart, 1, MPI_INT, MAIN_PROC_RANK, X_LOCAL_START_TAG, MPI_COMM_WORLD, &status);
		MPI_Recv((void*)&xMyLocalEnd,   1, MPI_INT, MAIN_PROC_RANK, X_LOCAL_END_TAG,   MPI_COMM_WORLD, &status);
	}

	for (int i = 0; i < procCount; i++)
	{
		MPI_Barrier(MPI_COMM_WORLD);

		if (i == myRank)
		{
			cout << "Proc rank: " << i << endl;
			cout << "Lower proc rank: " << myLowerProcRank << endl;
			cout << "Upper proc rank: " << myUpperProcRank << endl;		
			cout << "x start: " << xMyLocalStart << endl;		
			cout << "x end:   " << xMyLocalEnd << endl;		
			cout << endl;
		}
		
	}

	MPI_Finalize();
}
