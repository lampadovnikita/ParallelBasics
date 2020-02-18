#include <cstdlib>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <chrono>

using namespace std;

// Количество потоков
const unsigned int THREADS_COUNT = 5;

// Структура, передаваемая в качестве аргумента потока
struct thread_args {
	// Имя потока
	char name[20];
	// Количество однотипных операций внутри потока
	unsigned int operation_count;
	// Нужно ли выводить в консоль информацию об атрибутах потока
	bool print_attrs;
};

// Функция, которую будет исполнять созданный поток
void* thread_job(void* arg)
{
	// Преобразуем аргументы к нужному типу
	thread_args *param = (thread_args *) arg;
	// Если не было передано аргументов, то завершаем работу потока
	if (param == NULL) {
		cout << "No args to work";
		return NULL;
	}

	// Из структуры аргумента получаем число операций
	int operation_count = param->operation_count;

	double number = 1.0;
	for (unsigned int i = 0; i < operation_count; i++) {
		number += 1.12345;
	}

	// Если необходимо, то выводим информацию об атрибутах потока
	if (param->print_attrs == true) {
		// Получаем индентефикатор текущего потока
		pthread_t thread;
		thread = pthread_self();

		// Получаем переменную с информацией об атрибутах текущего потока
		pthread_attr_t attr;
		pthread_getattr_np(thread, &attr);

		// Из структуры получаем:
		// + Тип создаваемого потока
		int detach_state;
		pthread_attr_getdetachstate(&attr, &detach_state);

		// + Размер охранной зоны, байт
		size_t guard_size;
		pthread_attr_getguardsize(&attr, &guard_size);

		// + Размер стека, байт
		size_t stack_size;
		// + Адрес стека
		void * stack_addr;
		pthread_attr_getstack(&attr, &stack_addr, &stack_size);

		// Освобождаем память, занятую под хранение атрибутов потока
		pthread_attr_destroy(&attr);

		cout << "*****Thread info*****" << endl;
		
		cout << "Name: " << param->name << endl;
		cout << "Detach state: ";
		if (detach_state == PTHREAD_CREATE_JOINABLE) {
			cout << "joinable";
		} else {
			cout << "detached";
		}
		cout << endl;

		cout << "Guard size: " << guard_size << endl;
		cout << "Stack address: " << stack_addr << endl;
		cout << "Stack size: " << stack_size << endl;
	}
	
	return NULL;
}

int main()
{
	pthread_t thread;
	int err;

	// Инициализируем структуру-аргумент
	thread_args targs;
	// Начальное число операций
	targs.operation_count = 1000;
	// Не будем выводить информацию об атрибутах, т.к. это занимает время
	targs.print_attrs = false;
	
	// Последовательно запускаем заданное число потоков
	// с возрастающим числом операций
	for (int i = 0; i < THREADS_COUNT; i++) {
		auto begin = chrono::steady_clock::now();

		// Создаём поток
		err = pthread_create(&thread, NULL, thread_job, (void *) &targs);
		// Если при создании потока произошла ошибка, выводим
		// сообщение об ошибке и прекращаем работу программы
		if (err != 0) {
			cout << "Cannot create a thread: " << strerror(err) << endl;
			exit(-1);
		}

		// Ожидаем завершение созданного потока
		err = pthread_join(thread, NULL);
		if (err != 0) {
			cout << "Cannot join a thread: " << strerror(err) << endl;
			exit(-1);
		}
		
		auto end = chrono::steady_clock::now();

		auto elapsed_time = chrono::duration_cast<std::chrono::milliseconds>(end - begin);

		cout << "With " << targs.operation_count << " operations" << endl
		     << "Thread was running: (chrono)" << elapsed_time.count() << " ms" << endl
			 << "=====================================" << endl;
		
		targs.operation_count *= 20;
	}

	// Проинициализируем переменную для хранения атрибутов потока
	pthread_attr_t thread_attr;
	err = pthread_attr_init(&thread_attr);
	if (err != 0)
	{
		cout << "Cannot create thread attribute: " << strerror(err) << endl;
		exit(-1);
	}

	// Поменяем размер стека
	err = pthread_attr_setstacksize(&thread_attr, 1024 * 1024);
	if (err != 0)
	{
		cout << "Setting stack size attribute failed: " << strerror(err)
			<< endl;
		exit(-1);
	}

	// Установим необходимость вывода информации об атрибутах потока
	targs.print_attrs = true;
	// Зададим имя
	strcpy(targs.name, "MyThread");

	// Создаём поток с нестандартными атрибутами
	err = pthread_create(&thread, &thread_attr, thread_job, (void *) &targs);
	if (err != 0)
	{
		cout << "Cannot create a thread: " << strerror(err) << endl;
		exit(-1);
	}

	// Освобождаем память, занятую под хранение атрибутов потока
	pthread_attr_destroy(&thread_attr);

	// Ожидаем завершения созданного потока перед завершением
	// работы программы
	pthread_exit(NULL);
}
