#define _CRT_SECURE_NO_WARNINGS
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <CL\cl.hpp>

using namespace std;
using namespace cl;

const int image_width = 2048;
const int image_height = 2048;

cl_float4* cpu_output;
CommandQueue queue;
Device device;
Kernel kernel;
Context context;
Program program;

Buffer cl_output;
Buffer cl_spheres;

struct Sphere
{
	cl_float radius;
	cl_float dummy1;
	cl_float dummy2;
	cl_float dummy3;

	cl_float3 position;

	cl_float3 color;

	cl_float3 emission;
};

void pickPlatform(Platform& platform, const vector<Platform>& platforms)
{

	if (platforms.size() == 1) platform = platforms[0];
	else {
		int input = 0;
		cout << "\nChoose an OpenCL platform: ";
		cin >> input;

		// Обрабатываем некорректный ввод
		while (input < 1 || input > platforms.size()) {
			cin.clear();
			cin.ignore(cin.rdbuf()->in_avail(), '\n');
			cout << "No such option. Choose an OpenCL platform: ";
			cin >> input;
		}
		platform = platforms[input - 1];
	}
}

void pickDevice(Device& device, const vector<Device>& devices)
{

	if (devices.size() == 1) device = devices[0];
	else {
		int input = 0;
		cout << "\nChoose an OpenCL device: ";
		cin >> input;

		// Обрабатываем некорректный ввод
		while (input < 1 || input > devices.size()) {
			cin.clear();
			cin.ignore(cin.rdbuf()->in_avail(), '\n');
			cout << "No such option. Choose an OpenCL device: ";
			cin >> input;
		}
		device = devices[input - 1];
	}
}

void pickCPUCoreCount(Device& device)
{
	cl_uint input = 0;
	cout << "\nChoose compute units count: ";
	cin >> input;

	// Обрабатываем некорректный ввод
	while (input < 1 || input > device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>()) {
		cin.clear();
		cin.ignore(cin.rdbuf()->in_avail(), '\n');
		cout << "Core count must be between 1 and " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << "!";
		cout << "\nChoose core count: ";
		cin >> input;
	}

	cl_device_partition_property properties[] = { CL_DEVICE_PARTITION_BY_COUNTS, input, CL_DEVICE_PARTITION_BY_COUNTS_LIST_END };

	vector<Device> subDevices;
	device.createSubDevices(properties, &subDevices);

	device = subDevices[0];
}

std::size_t pickWorkGroupSize(Device& device)
{

	int input = 0;
	cout << "\nChoose work group size: ";
	cin >> input;

	// Обрабатываем некорректный ввод
	while (input < 1 || input > device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>()) {
		cin.clear();
		cin.ignore(cin.rdbuf()->in_avail(), '\n');
		cout << "Work group size must be between 1 and " << device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << "!";
		cout << "\nChoose work group size: ";
		cin >> input;
	}

	return input;
}

void printErrorLog(const Program& program, const Device& device)
{

	// Получаем логи ошибки и выводим в консоль
	string buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
	cerr << "Build log:" << std::endl << buildlog << std::endl;

	// Записываем лог в файл
	FILE* log = fopen("errorlog.txt", "w");
	fprintf(log, "%s\n", buildlog);
	cout << "Error log saved in 'errorlog.txt'" << endl;
	system("PAUSE");
	exit(1);
}

void initOpenCL()
{
	// Получаем все доступные платформы для OpenCL
	vector<Platform> platforms;
	Platform::get(&platforms);
	cout << "Available OpenCL platforms : " << endl << endl;
	for (int i = 0; i < platforms.size(); i++)
		cout << "\t" << i + 1 << ": " << platforms[i].getInfo<CL_PLATFORM_NAME>() << endl;

	// Выбираем одну из платформ
	Platform platform;
	pickPlatform(platform, platforms);
	cout << "\nUsing OpenCL platform: \t" << platform.getInfo<CL_PLATFORM_NAME>() << endl;

	// Получаем доступные устройства
	vector<Device> devices;
	platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);

	cout << "Available OpenCL devices on this platform: " << endl << endl;
	for (int i = 0; i < devices.size(); i++) {
		cout << "\t" << i + 1 << ": " << devices[i].getInfo<CL_DEVICE_NAME>() << endl;
		cout << "\t\tMax compute units: " << devices[i].getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << endl;
		cout << "\t\tMax work group size: " << devices[i].getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << endl << endl;
	}

	// Выбираем устройство
	pickDevice(device, devices);
	cout << "\nUsing OpenCL device: \t" << device.getInfo<CL_DEVICE_NAME>() << endl;
	cout << "\t\t\tMax compute units: " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << endl;
	cout << "\t\t\tMax work group size: " << device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << endl;

	if (device.getInfo<CL_DEVICE_TYPE>() == CL_DEVICE_TYPE_CPU) {
		pickCPUCoreCount(device);
	}

	// Создаём контекст OpenCL и очередь команд 
	context = Context(device);
	queue = CommandQueue(context, device);

	// Переводим OpenCL код в строку
	string source;
	ifstream file("opencl_kernel.cl");
	if (!file) {
		cout << "\nNo OpenCL file found!" << endl << "Exiting..." << endl;
		system("PAUSE");
		exit(1);
	}
	while (!file.eof()) {
		char line[256];
		file.getline(line, 255);
		source += line;
	}

	const char* kernel_source = source.c_str();

	// Создаём OpenCL программу, выполняя компиляцию для выбранного устройства
	program = Program(context, kernel_source);
	cl_int result = program.build({ device });
	if (result) cout << "Error during compilation OpenCL code!!!\n (" << result << ")" << endl;
	if (result == CL_BUILD_PROGRAM_FAILURE) printErrorLog(program, device);

	// Создаём входную точку программы OpenCL
	kernel = Kernel(program, "render_kernel");
}

void cleanUp()
{
	delete cpu_output;
}

inline float clamp(float x)
{
	return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
}

// Конвертируем RGB вещественные числа [0, 1] в целочисленный диапаон [0, 255] и выполняем гамма коррекцию
inline int toInt(float x)
{
	return int(clamp(x) * 255 + .5);
}

void saveImage()
{
	FILE* f = fopen("opencl_raytracer.ppm", "w");
	fprintf(f, "P3\n%d %d\n%d\n", image_width, image_height, 255);

	for (int i = 0; i < image_width * image_height; i++)
		fprintf(f, "%d %d %d ",
			toInt(cpu_output[i].s[0]),
			toInt(cpu_output[i].s[1]),
			toInt(cpu_output[i].s[2]));
}

void initScene(Sphere* cpu_spheres)
{

	// Левая стена
	cpu_spheres[0].radius = 200.0f;
	cpu_spheres[0].position = { -200.6f, 0.0f, 0.0f };
	cpu_spheres[0].color = { 0.75f, 0.25f, 0.25f };
	cpu_spheres[0].emission = { 0.0f, 0.0f, 0.0f };

	// Правая стена
	cpu_spheres[1].radius = 200.0f;
	cpu_spheres[1].position = { 200.6f, 0.0f, 0.0f };
	cpu_spheres[1].color = { 0.25f, 0.25f, 0.75f };
	cpu_spheres[1].emission = { 0.0f, 0.0f, 0.0f };

	// Пол
	cpu_spheres[2].radius = 200.0f;
	cpu_spheres[2].position = { 0.0f, -200.4f, 0.0f };
	cpu_spheres[2].color = { 0.71f, 0.45f, 0.23f };
	cpu_spheres[2].emission = { 0.0f, 0.0f, 0.0f };

	// Потолок
	cpu_spheres[3].radius = 200.0f;
	cpu_spheres[3].position = { 0.0f, 200.49f, 0.0f };
	cpu_spheres[3].color = { 0.9f, 0.8f, 0.7f };
	cpu_spheres[3].emission = { 0.0f, 0.0f, 0.0f };

	// Задняя стена
	cpu_spheres[4].radius = 200.0f;
	cpu_spheres[4].position = { 0.0f, 0.0f, -200.36f };
	cpu_spheres[4].color = { 0.13f, 0.68f, 0.36f };
	cpu_spheres[4].emission = { 0.0f, 0.0f, 0.0f };

	// Передняя стена
	cpu_spheres[5].radius = 200.0f;
	cpu_spheres[5].position = { 0.0f, 0.0f, 202.0f };
	cpu_spheres[5].color = { 0.13f, 0.68f, 0.36f };
	cpu_spheres[5].emission = { 0.0f, 0.0f, 0.0f };

	// Левая сфера
	cpu_spheres[6].radius = 0.16f;
	cpu_spheres[6].position = { -0.25f, -0.24f, -0.1f };
	cpu_spheres[6].color = { 0.9f, 0.8f, 0.7f };
	cpu_spheres[6].emission = { 0.0f, 0.0f, 0.0f };

	// Правая сфера
	cpu_spheres[7].radius = 0.16f;
	cpu_spheres[7].position = { 0.25f, -0.24f, 0.2f };
	cpu_spheres[7].color = { 0.9f, 0.8f, 0.7f };
	cpu_spheres[7].emission = { 0.0f, 0.0f, 0.0f };

	// Источник света
	cpu_spheres[8].radius = 1.0f;
	cpu_spheres[8].position = { 0.0f, 1.43f, 0.0f };
	cpu_spheres[8].color = { 0.0f, 0.0f, 0.0f };
	cpu_spheres[8].emission = {9.0f, 8.0f, 6.0f};
}

void main()
{
	std::srand(unsigned(std::time(0)));

	unsigned int seed1 = rand();
	unsigned int seed2 = rand();

	initOpenCL();

	// Выделяем память для будущего изображения
	cpu_output = new cl_float3[image_width * image_height];

	// Инициализируем сцену
	const int sphere_count = 9;
	Sphere cpu_spheres[sphere_count];
	initScene(cpu_spheres);

	// Создаём буферы для устройства, которое будет выполнять вычисления
	cl_output = Buffer(context, CL_MEM_WRITE_ONLY, image_width * image_height * sizeof(cl_float3));
	cl_spheres = Buffer(context, CL_MEM_READ_ONLY, sphere_count * sizeof(Sphere));
	queue.enqueueWriteBuffer(cl_spheres, CL_TRUE, 0, sphere_count * sizeof(Sphere), cpu_spheres);

	// Указываем аргументы для OpenCL программы
	kernel.setArg(0, cl_spheres);
	kernel.setArg(1, image_width);
	kernel.setArg(2, image_height);
	kernel.setArg(3, sphere_count);
	kernel.setArg(4, seed1);
	kernel.setArg(5, seed2);
	kernel.setArg(6, cl_output);

	// Каждый пиксель просчитывается в отдельном потоке 
	std::size_t global_work_size = image_width * image_height;
	// Указываем число рабочих групп
	std::size_t local_work_size = pickWorkGroupSize(device);

	cout << "Kernel work group size: " << local_work_size << endl;

	// Нужно чтобы глобальное число потоков нацело делилось на размер рабочей группы
	if (global_work_size % local_work_size != 0)
		global_work_size = (global_work_size / local_work_size + 1) * local_work_size;

	cout << "Rendering started..." << endl;

	auto begin = chrono::steady_clock::now();

	// Запускаем OpenCL
	queue.enqueueNDRangeKernel(kernel, NULL, global_work_size, local_work_size);
	queue.finish();

	auto end = chrono::steady_clock::now();

	auto elapsed_time = chrono::duration_cast<std::chrono::milliseconds>(end - begin);
	cout << "Calculations was running " << elapsed_time.count() << " ms" << endl;


	cout << "Rendering done! \nCopying output from device to host" << endl;

	// Считывем и копируем вывод из OpenCL
	queue.enqueueReadBuffer(cl_output, CL_TRUE, 0, image_width * image_height * sizeof(cl_float3), cpu_output);

	saveImage();
	cout << "Saved image to 'opencl_raytracer.ppm'" << endl;

	cleanUp();

	system("PAUSE");
}
